/****************************************************************************
 *  Header: remcom.c,v 1.34 91/03/09 12:29:49 glenne Exp $
 *
 *  Module name: remcom.c $
 *  Revision: 1.34 $
 *  Date: 91/03/09 12:29:49 $
 *  Contributor:     Lake Stevens Instrument Division$
 *
 *  Description:     low level support for gdb debugger. $
 *
 *  Considerations:  only works on target hardware $
 *
 *  Written by:      Glenn Engel $
 *  ModuleState:     Experimental $
 *
 *  NOTES:           See Below $
 *
 *  Modified for 386 by Jim Kingdon, Cygnus Support.
 *
 *  To enable debugger support, two things need to happen.  One, a
 *  call to set_debug_traps() is necessary in order to allow any breakpoints
 *  or error conditions to be properly intercepted and reported to gdb.
 *  Two, a breakpoint needs to be generated to begin communication.  This
 *  is most easily accomplished by a call to breakpoint().  Breakpoint()
 *  simulates a breakpoint by executing a trap #1.
 *
 *  The external function exceptionHandler() is
 *  used to attach a specific handler to a specific 386 vector number.
 *  It should use the same privilege level it runs at.  It should
 *  install it as an interrupt gate so that interrupts are masked
 *  while the handler runs.
 *
 *  Because gdb will sometimes write to the stack area to execute function
 *  calls, this program cannot rely on using the supervisor stack so it
 *  uses it's own stack area reserved in the int array remcomStack.
 *
 *************
 *
 *    The following gdb commands are supported:
 *
 * command          function                               Return value
 *
 *    g             return the value of the CPU registers  hex data or ENN
 *    G             set the value of the CPU registers     OK or ENN
 *
 *    mAA..AA,LLLL  Read LLLL bytes at address AA..AA      hex data or ENN
 *    MAA..AA,LLLL: Write LLLL bytes at address AA.AA      OK or ENN
 *    XAA..AA,LLLL: Same, but data is binary (not hex)     OK or ENN
 *
 *    c             Resume at current address              SNN   ( signal NN)
 *    cAA..AA       Continue at address AA..AA             SNN
 *    CNN;          Resume at current address with signal  SNN
 *    CNN;AA..AA    Resume at address AA..AA with signal   SNN
 *
 *    s             Step one instruction                   SNN
 *    sAA..AA       Step one instruction from AA..AA       SNN
 *    SNN;          Step one instruction with signal       SNN
 *    SNNAA..AA     Step one instruction from AA..AA w/NN  SNN
 *
 *    k             kill (Detach GDB)
 *
 *    d             Toggle debug flag
 *    D             Detach GDB
 *
 *    Hct           Set thread t for operations,           OK or ENN
 *                  c = 'c' (step, cont), c = 'g' (other
 *                  operations)
 *
 *    qC            Query current thread ID                QCpid
 *    qfThreadInfo  Get list of current threads (first)    m<id>
 *    qsThreadInfo   "    "  "     "      "   (subsequent)
 *    qOffsets      Get section offsets                  Text=x;Data=y;Bss=z
 *
 *    TXX           Find if thread XX is alive             OK or ENN
 *    ?             What was the last sigval ?             SNN   (signal NN)
 *    O             Output to GDB console
 *
 * All commands and responses are sent with a packet which includes a
 * checksum.  A packet consists of
 *
 * $<packet info>#<checksum>.
 *
 * where
 * <packet info> :: <characters representing the command or response>
 * <checksum>    :: < two hex digits computed as modulo 256 sum of <packetinfo>>
 *
 * When a packet is received, it is first acknowledged with either '+' or '-'.
 * '+' indicates a successful transfer.  '-' indicates a failed transfer.
 *
 * Example:
 *
 * Host:                  Reply:
 * $m0,10#2a               +$00010203040506070809101112131415#42
 *
 ****************************************************************************/
/*
 *
 * Remote communication protocol.
 *
 *    A debug packet whose contents are <data> is encapsulated for
 *    transmission in the form:
 *
 *       $ <data> # CSUM1 CSUM2
 *
 *       <data> must be ASCII alphanumeric and cannot include characters
 *       '$' or '#'.  If <data> starts with two characters followed by
 *       ':', then the existing stubs interpret this as a sequence number.
 *
 *       CSUM1 and CSUM2 are ascii hex representation of an 8-bit
 *       checksum of <data>, the most significant nibble is sent first.
 *       the hex digits 0-9,a-f are used.
 *
 *    Receiver responds with:
 *
 *       +       - if CSUM is correct and ready for next packet
 *       -       - if CSUM is incorrect
 *
 * Responses can be run-length encoded to save space.  A '*' means that
 * the next character is an ASCII encoding giving a repeat count which
 * stands for that many repititions of the character preceding the '*'.
 * The encoding is n+29, yielding a printable character where n >=3
 * (which is where RLE starts to win).  Don't use an n > 126.
 *
 * So "0* " means the same as "0000".
 */

/*
 * ARM port Copyright (c) 2002 MontaVista Software, Inc
 *
 * Authors:  George Davis <davis_g@mvista.com>
 *          Deepak Saxena <dsaxena@mvista.com>
 *
 *
 * See Documentation/ARM/kgdb for information on porting to a new board
 *
 * tabstop=3 to make this readable
 */

#include <linux/config.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/spinlock.h>
#include <linux/personality.h>
#include <linux/ptrace.h>
#include <linux/elf.h>
#include <linux/interrupt.h>
#include <linux/init.h>

#include <asm/atomic.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/unistd.h>

#include <asm/kgdb.h>

#ifdef CONFIG_MAGIC_SYSRQ
#include <linux/sysrq.h>
#endif


#ifdef	CONFIG_DEBUG_LL
// #define printascii(x, args...)  printk(x, ## args)
// #undef printascii
// #define printascii(x, args...)
extern void printascii(const char *);
extern void printhex8(unsigned int);
#else
#define	printascii(s)
#define	printhex8(i)
#endif


#define GDB_MAXREGS	(16 + 8 * 3 + 1 + 1)
#define GDB_REGBYTES	(GDB_MAXREGS << 2)

#define BUFMAX 2048

#define PC_REGNUM	0x0f
#define	LR_REGNUM	0x0e
#define	SP_REGNUM	0x0d


/* External functions */

extern struct pt_regs *get_task_registers(const struct task_struct *);


/* Forward declarations */

static unsigned long get_next_pc(struct pt_regs *);


/* Globals */

int kgdb_fault_expected = 0;	/* Boolean to ignore bus errs (i.e. in GDB) */
int kgdb_enabled = 0;


/* Locals */

static char remote_debug = 0;
static const char hexchars[]="0123456789abcdef";
static int fault_jmp_buf[32]; /* Jump buffer for kgdb_setjmp/longjmp */
static char remcomInBuffer[BUFMAX];
static char remcomOutBuffer[BUFMAX];
static int kgdb_initialized = 0;
static volatile unsigned int *step_addr = NULL;
static unsigned int step_instr = 0;

static struct pt_regs kgdb_regs;
static unsigned int gdb_regs[GDB_MAXREGS];

#ifdef CONFIG_KGDB_THREAD
static struct task_struct *trapped_thread;
static struct task_struct *current_thread;
typedef unsigned char threadref[8];
#define BUF_THREAD_ID_SIZE 16
#endif


/*
 * Various conversion functions
 */
static int hex(unsigned char ch)
{
  if ((ch >= 'a') && (ch <= 'f')) return (ch-'a'+10);
  if ((ch >= '0') && (ch <= '9')) return (ch-'0');
  if ((ch >= 'A') && (ch <= 'F')) return (ch-'A'+10);
  return (-1);
}

static unsigned char * mem2hex(const char *mem, char *buf, int count)
{
	/* Accessing 16-bit and 32-bit objects in a single
	 * load instruction is required to avoid bad side
	 * effects for some IO registers.
	 */

	if ((count == 2) && (((long)mem & 1) == 0)) {
		unsigned short tmp_s =
			cpu_to_be16(*(unsigned short *)mem);
		mem += 2;
		*buf++ = hexchars[(tmp_s >> 12) & 0xf];
		*buf++ = hexchars[(tmp_s >> 8) & 0xf];
		*buf++ = hexchars[(tmp_s >> 4) & 0xf];
		*buf++ = hexchars[tmp_s & 0xf];
	} else if ((count == 4) && (((long)mem & 3) == 0)) {
		unsigned int tmp_l =
			cpu_to_be32(*(unsigned int *)mem);
		mem += 4;
		*buf++ = hexchars[(tmp_l >> 28) & 0xf];
		*buf++ = hexchars[(tmp_l >> 24) & 0xf];
		*buf++ = hexchars[(tmp_l >> 20) & 0xf];
		*buf++ = hexchars[(tmp_l >> 16) & 0xf];
		*buf++ = hexchars[(tmp_l >> 12) & 0xf];
		*buf++ = hexchars[(tmp_l >> 8) & 0xf];
		*buf++ = hexchars[(tmp_l >> 4) & 0xf];
		*buf++ = hexchars[tmp_l & 0xf];
	} else {
		unsigned char ch;
		while (count-- > 0) {
			ch = *mem++;
			*buf++ = hexchars[ch >> 4];
			*buf++ = hexchars[ch & 0xf];
		}
	}

	*buf = 0;
	return buf;
}

/*
 * convert the hex array pointed to by buf into binary to be placed in mem
 * return a pointer to the character AFTER the last byte written.
 */
static char * hex2mem(char *buf, char *mem, int count)
{
	/* Accessing 16-bit and 32-bit objects in a single
	 * store instruction is required to avoid bad side
	 * effects for some IO registers.
	 */

	if ((count == 2) && (((long)mem & 1) == 0)) {
		unsigned short tmp_s =
			hex(*buf++) << 12;
		tmp_s |= hex(*buf++) << 8;
		tmp_s |= hex(*buf++) << 4;
		tmp_s |= hex(*buf++);
		*(unsigned short *)mem = be16_to_cpu(tmp_s);
		mem += 2;
	} else if ((count == 4) && (((long)mem & 3) == 0)) {
		unsigned int tmp_l =
			hex(*buf++) << 28;
		tmp_l |= hex(*buf++) << 24;
		tmp_l |= hex(*buf++) << 20;
		tmp_l |= hex(*buf++) << 16;
		tmp_l |= hex(*buf++) << 12;
		tmp_l |= hex(*buf++) << 8;
		tmp_l |= hex(*buf++) << 4;
		tmp_l |= hex(*buf++);
		*(unsigned int *)mem = be32_to_cpu(tmp_l);
		mem += 4;
	} else {
		int i;
		unsigned char ch;
		for (i=0; i<count; i++) {
			ch = hex(*buf++) << 4;
			ch |= hex(*buf++);
			*mem++ = ch;
		}
	}

	return mem;
}

/*  Copy the binary array pointed to by buf into mem.  Fix $, #,
    and 0x7d escaped with 0x7d.  Return a pointer to the character
    after the last byte written. */
static char *ebin2mem(const char *buf, char *mem, int count)
{
	for (; count > 0; count--, buf++) {
		if (*buf == 0x7d)
			*mem++ = *(++buf) ^ 0x20;
		else
			*mem++ = *buf;
	}
	return mem;
}

/*
 * While we find nice hex chars, build an int.
 * Return number of chars processed.
 */
static int hex2int(char **ptr, int *intValue)
{
	int numChars = 0;
	int hexValue;

	*intValue = 0;

	while (**ptr) {
		hexValue = hex(**ptr);
		if (hexValue < 0)
			break;

		*intValue = (*intValue << 4) | hexValue;
		numChars ++;

		(*ptr)++;
	}

	return (numChars);
}

/* Make a local copy of the registers passed into the handler (bletch) */
static void kregs2gregs(const struct pt_regs *kregs, int *gregs)
{
	int regno;

	/* Initialize to zero */
	for (regno = 0; regno < GDB_MAXREGS; regno++)
		gregs[regno] = 0;

	gregs[0] = kregs->ARM_r0;
	gregs[1] = kregs->ARM_r1;
	gregs[2] = kregs->ARM_r2;
	gregs[3] = kregs->ARM_r3;
	gregs[4] = kregs->ARM_r4;
	gregs[5] = kregs->ARM_r5;
	gregs[6] = kregs->ARM_r6;
	gregs[7] = kregs->ARM_r7;
	gregs[8] = kregs->ARM_r8;
	gregs[9] = kregs->ARM_r9;
	gregs[10] = kregs->ARM_r10;
	gregs[11] = kregs->ARM_fp;
	gregs[12] = kregs->ARM_ip;
	gregs[13] = kregs->ARM_sp;
	gregs[14] = kregs->ARM_lr;
	gregs[15] = kregs->ARM_pc;
	gregs[GDB_MAXREGS - 1] = kregs->ARM_cpsr;
}

/* Copy local gdb registers back to kgdb regs, for later copy to kernel */
static void gregs2kregs(const int *gregs, struct pt_regs *kregs)
{
	kregs->ARM_r0 = gregs[0];
	kregs->ARM_r1 = gregs[1];
	kregs->ARM_r2 = gregs[2];
	kregs->ARM_r3 = gregs[3];
	kregs->ARM_r4 = gregs[4];
	kregs->ARM_r5 = gregs[5];
	kregs->ARM_r6 = gregs[6];
	kregs->ARM_r7 = gregs[7];
	kregs->ARM_r8 = gregs[8];
	kregs->ARM_r9 = gregs[9];
	kregs->ARM_r10 = gregs[10];
	kregs->ARM_fp = gregs[11];
	kregs->ARM_ip = gregs[12];
	kregs->ARM_sp = gregs[13];
	kregs->ARM_lr = gregs[14];
	kregs->ARM_pc = gregs[15];
	kregs->ARM_cpsr = gregs[GDB_MAXREGS - 1];
}

#ifdef CONFIG_KGDB_THREAD
/* Make a local copy of registers from the specified thread */
static void tregs2gregs(const struct task_struct *task, int *gregs)
{
	int regno;
	struct pt_regs *tregs;

	/* Initialize to zero */
	for (regno = 0; regno < GDB_MAXREGS; regno++)
		gregs[regno] = 0;

	/* Just making sure... */
	if (task == NULL)
		return;

#if	0 /* REVISIT */
	/* A new fork has pt_regs on the stack from a fork() call */
	if (task->thread->save.pc == (unsigned long)ret_from_fork) {
		struct pt_regs *tregs;
		kregs = (struct pt_regs*)task->thread->save;

		gregs[0] = tregs->ARM_r0;
		gregs[1] = tregs->ARM_r1;
		gregs[2] = tregs->ARM_r2;
		gregs[3] = tregs->ARM_r3;
		gregs[4] = tregs->ARM_r4;
		gregs[5] = tregs->ARM_r5;
		gregs[6] = tregs->ARM_r6;
		gregs[7] = tregs->ARM_r7;
		gregs[8] = tregs->ARM_r8;
		gregs[9] = tregs->ARM_r9;
		gregs[10] = tregs->ARM_r10;
		gregs[11] = tregs->ARM_fp;
		gregs[12] = tregs->ARM_ip;
		gregs[13] = tregs->ARM_sp;
		gregs[14] = tregs->ARM_lr;
		gregs[15] = tregs->ARM_pc;
		gregs[GDB_MAXREGS - 1] = tregs->ARM_cpsr;
	}
#endif

	/* Otherwise, we have only some registers from switch_to() */
	tregs = get_task_registers(task);
	gregs[0] = tregs->ARM_r0; /* Not really valid? */
	gregs[1] = tregs->ARM_r1; /* "               " */
	gregs[2] = tregs->ARM_r2; /* "               " */
	gregs[3] = tregs->ARM_r3; /* "               " */
	gregs[4] = tregs->ARM_r4;
	gregs[5] = tregs->ARM_r5;
	gregs[6] = tregs->ARM_r6;
	gregs[7] = tregs->ARM_r7;
	gregs[8] = tregs->ARM_r8;
	gregs[9] = tregs->ARM_r9;
	gregs[10] = tregs->ARM_r10;
	gregs[11] = tregs->ARM_fp;
	gregs[12] = tregs->ARM_ip;
	gregs[13] = tregs->ARM_sp;
	gregs[14] = tregs->ARM_lr;
	gregs[15] = tregs->ARM_pc;
	gregs[GDB_MAXREGS - 1] = tregs->ARM_cpsr;
}

static char * pack_hex_byte(char *pkt, int byte)
{
	*pkt++ = hexchars[(byte >> 4) & 0xf];
	*pkt++ = hexchars[(byte & 0xf)];
	return pkt;
}

static char * pack_threadid(char *pkt, threadref * id)
{
	char *limit;
	unsigned char *altid;

	altid = (unsigned char *) id;

	limit = pkt + BUF_THREAD_ID_SIZE;
	while (pkt < limit)
		pkt = pack_hex_byte(pkt, *altid++);
	return pkt;
}

static void int_to_threadref(threadref * id, const int value)
{
	unsigned char *scan = (unsigned char *) id;
	int i = 4;

	while (i--)
		*scan++ = 0;

	*scan++ = (value >> 24) & 0xff;
	*scan++ = (value >> 16) & 0xff;
	*scan++ = (value >> 8) & 0xff;
	*scan++ = (value & 0xff);
}

/* Return a task structure ptr for a particular pid */
static struct task_struct * get_thread(int pid)
{
	struct task_struct *thread;

	/* Use PID_MAX w/gdb for pid 0 */
	if (pid == PID_MAX) pid = 0;

	/* First check via PID */
	thread = find_task_by_pid(pid);

	if (thread)
		return thread;

	/* Start at the start */
	thread = init_tasks[0];

	/* Walk along the linked list of tasks */
	do {
		if (thread->pid == pid)
			return thread;
		thread = thread->next_task;
	} while (thread != init_tasks[0]);

	return NULL;
}
#endif /* CONFIG_KGDB_THREAD */

void kgdb_msg(const char * msg)
{
	const char *emsg;

	for (emsg = msg; *emsg; emsg++) ;

	remcomOutBuffer[0] = 'O';

	mem2hex(msg, remcomOutBuffer+1, emsg-msg);

	if (kgdb_active()) kgdb_put_packet(remcomOutBuffer);
}

static void send_ok_msg(void)
{
	kgdb_put_packet("OK");
}

static void send_err_msg(void)
{
	kgdb_put_packet("E01");
}

static void send_empty_msg(void)
{
	kgdb_put_packet("");
}

static void send_signal_msg(const int signum)
{
#ifndef CONFIG_KGDB_THREAD
	remcomOutBuffer[0] = 'S';
	remcomOutBuffer[1] =  hexchars[(signum >> 4) & 0xf];
	remcomOutBuffer[2] =  hexchars[signum % 16];
	remcomOutBuffer[3] = 0;
#else /* CONFIG_KGDB_THREAD */
	int threadid;
	threadref thref;
	char *out = remcomOutBuffer;
	const char *tstring = "thread";

	*out++ = 'T';
	*out++ = hexchars[(signum >> 4) & 0xf];
	*out++ = hexchars[signum % 16];

	while (*tstring) {
                *out++ = *tstring++;
	}
	*out++ = ':';

	threadid = trapped_thread->pid;
	if (threadid == 0) threadid = PID_MAX;
	int_to_threadref(&thref, threadid);
	pack_threadid(out, &thref);
	out += BUF_THREAD_ID_SIZE;
	*out++ = ';';

	*out = 0;
#endif /* CONFIG_KGDB_THREAD */
	kgdb_put_packet(remcomOutBuffer);
}

static void send_regs_msg(void)
{
#ifdef CONFIG_KGDB_THREAD
	if (!current_thread)
		kregs2gregs(&kgdb_regs, gdb_regs);
	else
		tregs2gregs(current_thread, gdb_regs);
#else
	kregs2gregs(&kgdb_regs, gdb_regs);
#endif
	mem2hex((const char *)gdb_regs, remcomOutBuffer, GDB_REGBYTES);
	kgdb_put_packet(remcomOutBuffer);
}

/* Set register contents - currently can't set other thread's registers */
static void set_regs_msg(void)
{
#ifdef CONFIG_KGDB_THREAD
	if (!current_thread) {
#endif
		kregs2gregs(&kgdb_regs, gdb_regs);
		hex2mem(&remcomInBuffer[1], (char *) gdb_regs, GDB_REGBYTES);
		gregs2kregs(gdb_regs, &kgdb_regs);
		send_ok_msg();
#ifdef CONFIG_KGDB_THREAD
	} else
		send_err_msg();
#endif
}

/* The following provides support for recovery from bus
 * errors during kgdb memory read/write operations.
 */
void kgdb_handle_bus_error(void)
{
	kgdb_longjmp(fault_jmp_buf, 1);
}

/* Read memory due to 'm' message */
static void read_mem_msg(void)
{
	char *ptr;
	int addr;
	int length;

	/* Jmp, disable bus error handler */
	if (kgdb_setjmp(fault_jmp_buf) == 0) {

		kgdb_fault_expected = 1;

		/* Walk through, have m<addr>,<length> */
		ptr = &remcomInBuffer[1];
		if (hex2int(&ptr, &addr) && (*ptr++ == ','))
			if (hex2int(&ptr, &length)) {
				ptr = 0;
				if (length * 2 > BUFMAX)
					length = BUFMAX / 2;
				mem2hex((char *) addr, remcomOutBuffer, length);
			}
		if (ptr)
			send_err_msg();
		else
			kgdb_put_packet(remcomOutBuffer);
	} else
		send_err_msg();

	/* Restore bus error handler */
	kgdb_fault_expected = 0;
}

/* Write memory due to 'M' or 'X' message */
static void write_mem_msg(int binary)
{
	char *ptr;
	int addr;
	int length;

	if (kgdb_setjmp(fault_jmp_buf) == 0) {

		kgdb_fault_expected = 1;

		/* Walk through, have M<addr>,<length>:<data> */
		ptr = &remcomInBuffer[1];
		if (hex2int(&ptr, &addr) && (*ptr++ == ','))
			if (hex2int(&ptr, &length) && (*ptr++ == ':')) {
				if (binary)
					ebin2mem(ptr, (char*)addr, length);
				else
					hex2mem(ptr, (char*)addr, length);
				ptr = 0;

				/*
				 * Trap breakpoints
				 */
				if(length == 4 && !(addr & 0x3) &&
					*((unsigned *)addr) == GDB_BREAKINST)
					*((unsigned *)addr) = KGDB_BREAKINST;

				send_ok_msg();
			}
		if (ptr)
			send_err_msg();
	} else
		send_err_msg();

	/* Restore bus error handler */
	kgdb_fault_expected = 0;
}

static void continue_msg(void)
{
        /* Try to read optional parameter, PC unchanged if none */
        char *ptr = &remcomInBuffer[1];
        int addr;

        if (hex2int(&ptr, &addr))
                kgdb_regs.ARM_pc = addr;

	strcpy(remcomOutBuffer, "OK");
	kgdb_put_packet(remcomOutBuffer);
}

static void continue_with_sig_msg(void)
{
        int signal;
        char *ptr = &remcomInBuffer[1];
        int addr;

        /* Report limitation */
        kgdb_msg("Cannot force signal in kgdb, continuing anyway.\n");

        /* Signal */
        hex2int(&ptr, &signal);
        if (*ptr == ';')
                ptr++;

        /* Optional address */
        if (hex2int(&ptr, &addr))
                kgdb_regs.ARM_pc = addr;

	strcpy(remcomOutBuffer, "OK");
	kgdb_put_packet(remcomOutBuffer);
}

void do_single_step(void)
{
	step_addr = (unsigned int *) get_next_pc(&kgdb_regs);
	step_instr = *step_addr;
	*step_addr = KGDB_BREAKINST;
}

static void undo_single_step(void)
{
	if (step_addr != NULL) {
		*step_addr = step_instr;
		step_addr = NULL;
		step_instr = 0;
	}
}

static void step_msg(void)
{
        continue_msg();
        do_single_step();
}

static void step_with_sig_msg(void)
{
        continue_with_sig_msg();
        do_single_step();
}

/* Set the status for a thread */
void set_thread_msg(void)
{
#ifndef CONFIG_KGDB_THREAD
	strcpy(remcomOutBuffer, "OK");
	kgdb_put_packet(remcomOutBuffer);
#else
	int threadid;
	struct task_struct *thread = NULL;
	char *ptr;

	switch (remcomInBuffer[1]) {

       	/* To select which thread for gG etc messages, i.e. supported */
	case 'g':
		ptr = &remcomInBuffer[2];
		hex2int(&ptr, &threadid);
		thread = get_thread(threadid);

		/* If we haven't found it */
		if (!thread) {
			send_err_msg();
			break;
		}

		/* Set current_thread (or not) */
		if (thread == trapped_thread)
			current_thread = NULL;
		else
			current_thread = thread;
		send_ok_msg();
		break;

	/* To select which thread for cCsS messages, i.e. unsupported */
	case 'c':
		send_ok_msg();
		break;

	default:
		send_empty_msg();
		break;
	}
#endif
}

#ifdef CONFIG_KGDB_THREAD
/* Is a thread alive? */
static void thread_status_msg(void)
{
	char *ptr;
	int threadid;
	struct task_struct *thread = NULL;

	ptr = &remcomInBuffer[1];
	hex2int(&ptr, &threadid);
	thread = get_thread(threadid);
	if (thread)
		send_ok_msg();
	else
		send_err_msg();
}

/* Send the current thread ID */
static void thread_id_msg(void)
{
	int threadid;
	threadref thref;

	remcomOutBuffer[0] = 'Q';
	remcomOutBuffer[1] = 'C';

	if (current_thread)
		threadid = current_thread->pid;
	else if (trapped_thread)
		threadid = trapped_thread->pid;
	else /* Impossible, but just in case! */
	{
		send_err_msg();
		return;
	}

	/* Translate pid 0 to PID_MAX for gdb */
	if (threadid == 0) threadid = PID_MAX;

	int_to_threadref(&thref, threadid);
	pack_threadid(remcomOutBuffer + 2, &thref);
	remcomOutBuffer[2 + BUF_THREAD_ID_SIZE] = '\0';
	kgdb_put_packet(remcomOutBuffer);
}

/* Send thread info */
static void thread_info_msg(void)
{
	struct task_struct *thread = NULL;
	int threadid;
	char *pos;
	threadref thref;

	/* Start with 'm' */
	remcomOutBuffer[0] = 'm';
	pos = &remcomOutBuffer[1];

	/* For all possible thread IDs - this will overrun if > 44 threads! */
	/* Start at 1 and include PID_MAX (since GDB won't use pid 0...) */
	for (threadid = 1; threadid <= PID_MAX; threadid++) {

		read_lock(&tasklist_lock);
		thread = get_thread(threadid);
		read_unlock(&tasklist_lock);

		/* If it's a valid thread */
		if (thread) {
			int_to_threadref(&thref, threadid);
			pack_threadid(pos, &thref);
			pos += BUF_THREAD_ID_SIZE;
			*pos++ = ',';
		}
	}
	*--pos = 0;		/* Lose final comma */
	kgdb_put_packet(remcomOutBuffer);

}

/* Return printable info for gdb's 'info threads' command */
static void thread_extra_info_msg(void)
{
	int threadid;
	struct task_struct *thread = NULL;
	char buffer[20], *ptr;
	int i;

	/* Extract thread ID */
	ptr = &remcomInBuffer[17];
	hex2int(&ptr, &threadid);
	thread = get_thread(threadid);

	/* If we don't recognise it, say so */
	if (thread == NULL)
		strcpy(buffer, "(unknown)");
	else
		strcpy(buffer, thread->comm);

	/* Construct packet */
	for (i = 0, ptr = remcomOutBuffer; buffer[i]; i++)
		ptr = pack_hex_byte(ptr, buffer[i]);

#if	0 /* REVISIT! */
	if (thread->thread.pc == (unsigned long)ret_from_fork) {
		strcpy(buffer, "<new fork>");
		for (i = 0; buffer[i]; i++)
			ptr = pack_hex_byte(ptr, buffer[i]);
	}
#endif

	*ptr = '\0';
	kgdb_put_packet(remcomOutBuffer);
}

/* Handle all qFooBarBaz messages - have to use an if statement as
   opposed to a switch because q messages can have > 1 char id. */
static void query_msg(void)
{
	const char *q_start = &remcomInBuffer[1];

	/* qC = return current thread ID */
	if (strncmp(q_start, "C", 1) == 0)
		thread_id_msg();

	/* qfThreadInfo = query all threads (first) */
	else if (strncmp(q_start, "fThreadInfo", 11) == 0)
		thread_info_msg();

	/* qsThreadInfo = query all threads (subsequent). We know we have sent
	   them all after the qfThreadInfo message, so there are no to send */
	else if (strncmp(q_start, "sThreadInfo", 11) == 0)
		kgdb_put_packet("l");	/* el = last */

	/* qThreadExtraInfo = supply printable information per thread */
	else if (strncmp(q_start, "ThreadExtraInfo", 15) == 0)
		thread_extra_info_msg();

	/* Unsupported - empty message as per spec */
	else
		send_empty_msg();
}
#endif /* CONFIG_KGDB_THREAD */

/*
 * This function does all command procesing for interfacing to gdb.
 */
void do_kgdb(struct pt_regs *trap_regs, unsigned char sigval)
{
	unsigned long flags;

	save_flags_cli(flags);

	kgdb_regs = *trap_regs; /* Quick and dirty clone of pt_regs */

	/*
	 * reply to host that an exception has occurred
	 *
	 * We don't do this on the first call as it would cause a sync problem.
	 */
	if (kgdb_initialized) {
		send_signal_msg(sigval);
	} else {
		/*
		 * This is the first breakpoint, called from
		 * start_kernel or elsewhere. We need to
		 * (re-)initialize the I/O subsystem.
		 */

		printk("Breaking into KGDB\n");

		if(kgdb_io_init())
		{
			printk("KGB I/O INIT FAILED...HALTING!");
			while(1){ };
		}
	}

	kgdb_initialized = 1;

#ifdef CONFIG_KGDB_THREAD
        /* Until GDB specifies a thread */
        current_thread = NULL;
        trapped_thread = current;
#endif

	/*
	 * Check to see if this is a compiled in breakpoint
	 * (sysrq-G or initial breakpoint).  If so, we
	 * need to increment the PC to the next instruction
	 * so that we don't infinite loop.
	 *
	 * NOTE: THIS COULD BE BAD.  We're reading the PC and
	 * if the cause of the fault is a bad PC, we're going
	 * to suffer massive death. Need to find some way to
	 * validate the PC address or use our setjmp/longjmp.
	 */
	if (sigval == SIGTRAP) {
		/* Only do this check for the SIGTRAP case! */
		if (*(unsigned int *)(kgdb_regs.ARM_pc) == KGDB_COMPILED_BREAK)
			kgdb_regs.ARM_pc += 4;
	}


	undo_single_step(); /* handles cleanup upon step/stepi return */

	while (1) {
		remcomOutBuffer[0] = 0;

		kgdb_get_packet(remcomInBuffer, BUFMAX);

		switch (remcomInBuffer[0])
		{
		case '?':	/* Report most recent signal */
			send_signal_msg(sigval);
			break;

		case 'g':	/* return the values of the CPU registers */
			send_regs_msg();
               		break;

     		case 'G':	/* set the values of the CPU registers */
			set_regs_msg();
			break;

		case 'm':	/* Read LLLL bytes address AA..AA */
			read_mem_msg();
	          	break;

		case 'M':	/* Write LLLL bytes address AA.AA ret OK */
			write_mem_msg(0);
                	break;

		case 'X':       /* Write LLLL bytes esc bin address AA..AA */
			/* WARNING: UART must be configured for 8-bit chars */
			write_mem_msg(1); /* 1 = data in binary */
			break;

		case 'C':	/* Continue, signum included, we ignore it */
			continue_with_sig_msg();
			goto exit_kgdb;

		case 'c':	/* Continue at address AA..AA (optional) */
			continue_msg();
			goto exit_kgdb;

		case 'S':	/* Step, signum included, we ignore it */
			step_with_sig_msg();
			goto exit_kgdb;

		case 's':	/* Step one instruction from AA..AA */
			step_msg();
			goto exit_kgdb;

		case 'H':	/* Task related */
			set_thread_msg();
			break;

#ifdef CONFIG_KGDB_THREAD
		case 'T':	/* Query thread status */
			thread_status_msg();
			break;

		case 'q':	/* Handle query - currently thread-related */
			query_msg();
			break;
#endif

		case 'k':	/* kill the program - do nothing */
			break;

		case 'D':	/* Detach from program, send reply OK */
			kgdb_enabled = 0;
			send_ok_msg();
			kgdb_serial_getchar();
			goto exit_kgdb;

		case 'd':	/* toggle debug flag */
			remote_debug = !(remote_debug);
               		break;

		default:
			send_empty_msg();
			break;
		}
    	}

exit_kgdb:
	*trap_regs = kgdb_regs; /* Copy back any register updates */
	cpu_cache_clean_invalidate_all();
	restore_flags(flags);
}

/*
 * TODO: If remote GDB disconnects from us, we need to return
 * 0 as we're no longer active.  Does GDB send us a disconnect
 * message??
 */
int kgdb_active(void)
{
	return kgdb_enabled;
}


/* Trigger a breakpoint by function */
void breakpoint(void)
{
        if (!kgdb_enabled) {
                kgdb_enabled = 1;
        }
        BREAKPOINT();
}


/*
 * Code to determine next PC based on current PC address.
 * Taken from GDB source code.  Open Source is good. :)
 */
#define read_register(x) regs->uregs[x]

#define addr_bits_remove(x) (x & 0xfffffffc)

#define submask(x) ((1L << ((x) + 1)) - 1)
#define bit(obj,st) (((obj) >> (st)) & 1)
#define bits(obj,st,fn) (((obj) >> (st)) & submask ((fn) - (st)))
#define sbits(obj,st,fn) \
  ((long) (bits(obj,st,fn) | ((long) bit(obj,fn) * ~ submask (fn - st))))
#define BranchDest(addr,instr) \
  ((unsigned) (((long) (addr)) + 8 + (sbits (instr, 0, 23) << 2)))


/* Instruction condition field values.  */
#define INST_EQ         0x0
#define INST_NE         0x1
#define INST_CS         0x2
#define INST_CC         0x3
#define INST_MI         0x4
#define INST_PL         0x5
#define INST_VS         0x6
#define INST_VC         0x7
#define INST_HI         0x8
#define INST_LS         0x9
#define INST_GE         0xa
#define INST_LT         0xb
#define INST_GT         0xc
#define INST_LE         0xd
#define INST_AL         0xe
#define INST_NV         0xf

#define FLAG_N          0x80000000
#define FLAG_Z          0x40000000
#define FLAG_C          0x20000000
#define FLAG_V          0x10000000

#define error(x)

static unsigned long
shifted_reg_val (unsigned long inst, int carry, unsigned long pc_val,
		 unsigned long status_reg, struct pt_regs* regs)
{
  unsigned long res = 0, shift = 0;
  int rm = bits (inst, 0, 3);
  unsigned long shifttype = bits (inst, 5, 6);

  if (bit (inst, 4))
    {
      int rs = bits (inst, 8, 11);
      shift = (rs == 15 ? pc_val + 8 : read_register (rs)) & 0xFF;
    }
  else
    shift = bits (inst, 7, 11);

  res = (rm == 15
	 ? ((pc_val | (1 ? 0 : status_reg))
	    + (bit (inst, 4) ? 12 : 8)) : read_register (rm));

  switch (shifttype)
    {
    case 0:			/* LSL */
      res = shift >= 32 ? 0 : res << shift;
      break;

    case 1:			/* LSR */
      res = shift >= 32 ? 0 : res >> shift;
      break;

    case 2:			/* ASR */
      if (shift >= 32)
	shift = 31;
      res = ((res & 0x80000000L)
	     ? ~((~res) >> shift) : res >> shift);
      break;

    case 3:			/* ROR/RRX */
      shift &= 31;
      if (shift == 0)
	res = (res >> 1) | (carry ? 0x80000000L : 0);
      else
	res = (res >> shift) | (res << (32 - shift));
      break;
    }

  return res & 0xffffffff;
}

/* Return number of 1-bits in VAL.  */

static int
bitcount (unsigned long val)
{
  int nbits;
  for (nbits = 0; val != 0; nbits++)
    val &= val - 1;		/* delete rightmost 1-bit in val */
  return nbits;
}

static int
condition_true (unsigned long cond, unsigned long status_reg)
{
	if (cond == INST_AL || cond == INST_NV)
		return 1;

	switch (cond)
	{
		case INST_EQ:
		  return ((status_reg & FLAG_Z) != 0);
		case INST_NE:
		  return ((status_reg & FLAG_Z) == 0);
		case INST_CS:
		  return ((status_reg & FLAG_C) != 0);
		case INST_CC:
		  return ((status_reg & FLAG_C) == 0);
		case INST_MI:
		  return ((status_reg & FLAG_N) != 0);
		case INST_PL:
		  return ((status_reg & FLAG_N) == 0);
		case INST_VS:
		  return ((status_reg & FLAG_V) != 0);
		case INST_VC:
		  return ((status_reg & FLAG_V) == 0);
		case INST_HI:
		  return ((status_reg & (FLAG_C | FLAG_Z)) == FLAG_C);
		case INST_LS:
		  return ((status_reg & (FLAG_C | FLAG_Z)) != FLAG_C);
		case INST_GE:
		  return (((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0));
		case INST_LT:
		  return (((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0));
		case INST_GT:
		  return (((status_reg & FLAG_Z) == 0) &&
					(((status_reg & FLAG_N) == 0) == ((status_reg & FLAG_V) == 0)));
		case INST_LE:
		  return (((status_reg & FLAG_Z) != 0) ||
					(((status_reg & FLAG_N) == 0) != ((status_reg & FLAG_V) == 0)));
	}
	return 1;
}

unsigned long
get_next_pc(struct pt_regs *regs)
{
  unsigned long pc_val;
  unsigned long this_instr;
  unsigned long status;
  unsigned long nextpc;

  pc_val = regs->ARM_pc;
  this_instr = *((unsigned long *)regs->ARM_pc);
  status = regs->ARM_cpsr;
  nextpc = pc_val + 4;	/* Default case */

  if (condition_true (bits (this_instr, 28, 31), status))
  {
	  switch (bits (this_instr, 24, 27))
	  {
		case 0x0:
		case 0x1:		/* data processing */
		case 0x2:
		case 0x3:
		{
			unsigned long operand1, operand2, result = 0;
		 	unsigned long rn;
			int c;

			if (bits (this_instr, 12, 15) != 15)
				break;

			if (bits (this_instr, 22, 25) == 0
					&& bits (this_instr, 4, 7) == 9)
				/* multiply */
				error ("Illegal update to pc in instruction");

			/* Multiply into PC */
			c = (status & FLAG_C) ? 1 : 0;
			rn = bits (this_instr, 16, 19);
			operand1 = (rn == 15) ? pc_val + 8 : read_register (rn);

			if (bit (this_instr, 25))
			{
				unsigned long immval = bits (this_instr, 0, 7);
				unsigned long rotate = 2 * bits (this_instr, 8, 11);
				operand2 = ((immval >> rotate) | (immval << (32 - rotate)))
									& 0xffffffff;
			}
			else		/* operand 2 is a shifted register */
				operand2 = shifted_reg_val (this_instr, c, pc_val, status, regs);

			switch (bits (this_instr, 21, 24))
			{
				case 0x0:	/*and */
					result = operand1 & operand2;
					break;

				case 0x1:	/*eor */
					result = operand1 ^ operand2;
					break;

				case 0x2:	/*sub */
					result = operand1 - operand2;
					break;

				case 0x3:	/*rsb */
					result = operand2 - operand1;
					break;

				case 0x4:	/*add */
					result = operand1 + operand2;
					break;

				case 0x5:	/*adc */
					result = operand1 + operand2 + c;
					break;

				case 0x6:	/*sbc */
					result = operand1 - operand2 + c;
					break;

				case 0x7:	/*rsc */
					result = operand2 - operand1 + c;
					break;

				case 0x8:
				case 0x9:
				case 0xa:
				case 0xb:	/* tst, teq, cmp, cmn */
					result = (unsigned long) nextpc;
					break;

				case 0xc:	/*orr */
					result = operand1 | operand2;
					break;

				case 0xd:	/*mov */
					/* Always step into a function.  */
					result = operand2;
					break;

				case 0xe:	/*bic */
					result = operand1 & ~operand2;
					break;

				case 0xf:	/*mvn */
					result = ~operand2;
					break;
			}
			nextpc = addr_bits_remove(result);

			break;
	  }

		case 0x4:
		case 0x5:		/* data transfer */
		case 0x6:
		case 0x7:
		if (bit (this_instr, 20))
		{
			/* load */
			if (bits (this_instr, 12, 15) == 15)
			{
				/* rd == pc */
				unsigned long rn;
				unsigned long base;

				if (bit (this_instr, 22))
					error ("Illegal update to pc in instruction");

				/* byte write to PC */
				rn = bits (this_instr, 16, 19);
				base = (rn == 15) ? pc_val + 8 : read_register (rn);
				if (bit (this_instr, 24))
				{
					/* pre-indexed */
					int c = (status & FLAG_C) ? 1 : 0;
					unsigned long offset =
							  (bit (this_instr, 25)
								? shifted_reg_val (this_instr, c, pc_val, status, regs)
								: bits (this_instr, 0, 11));

					if (bit (this_instr, 23))
						base += offset;
					else
						base -= offset;
				}
				nextpc = *((unsigned long *) base);

				nextpc = addr_bits_remove (nextpc);

				if (nextpc == regs->ARM_pc)
						  error ("Infinite loop detected");
			}
		}
		break;

		case 0x8:
		case 0x9:		/* block transfer */
		if (bit (this_instr, 20))
		{
			/* LDM */
			if (bit (this_instr, 15))
			{
				/* loading pc */
				int offset = 0;

				if (bit (this_instr, 23))
				{
					/* up */
					unsigned long reglist = bits (this_instr, 0, 14);
					offset = bitcount (reglist) * 4;
					if (bit (this_instr, 24))		/* pre */
						offset += 4;
				}
				else if (bit (this_instr, 24))
					offset = - 4;

				{
					unsigned long rn_val =
						read_register (bits (this_instr, 16, 19));
					nextpc = *((unsigned int *) (rn_val + offset));
				}

				nextpc = addr_bits_remove (nextpc);
				if (nextpc == regs->ARM_pc)
						  error ("Infinite loop detected");
			}
		}
		break;

		case 0xb:		/* branch & link */
		case 0xa:		/* branch */
		{
			nextpc = BranchDest (regs->ARM_pc, this_instr);
			nextpc = addr_bits_remove (nextpc);

			if (nextpc == regs->ARM_pc)
				error ("Infinite loop detected");

			break;
		}

		case 0xc:
		case 0xd:
		case 0xe:		/* coproc ops */
		case 0xf:		/* SWI */
			break;

		default:
			error("Bad bit-field extraction");
			return (regs->ARM_pc);
	  }
  }

  return nextpc;
}
