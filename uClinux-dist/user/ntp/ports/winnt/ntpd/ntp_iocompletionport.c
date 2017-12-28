#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined (HAVE_IO_COMPLETION_PORT)

#include <stddef.h>
#include <stdio.h>
#include <process.h>
#include "ntp_stdlib.h"
#include "syslog.h"
#include "ntp_machine.h"
#include "ntp_fp.h"
#include "ntp.h"
#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_iocompletionport.h"
#include "transmitbuff.h"

/*
 * Request types
 */
enum {
	SOCK_RECV,
	SOCK_SEND,
	CLOCK_READ,
	CLOCK_WRITE
};


typedef struct IoCompletionInfo {
	OVERLAPPED			overlapped;
	int				request_type;
	union {
		recvbuf_t		*rbuf;
		transmitbuf_t		*tbuf;
	} buff_space;
} IoCompletionInfo;

#define	recv_buf	buff_space.rbuf
#define	trans_buf	buff_space.tbuf

/*
 * local function definitions
 */
static int QueueIORead( struct refclockio *, recvbuf_t *buff, IoCompletionInfo *lpo);

static int OnSocketRecv(DWORD, IoCompletionInfo *, DWORD, int);
static int OnIoReadComplete(DWORD, IoCompletionInfo *, DWORD, int);
static int OnWriteComplete(DWORD, IoCompletionInfo *, DWORD, int);


#define BUFCHECK_SECS	10
static void	TransmitCheckThread(void *NotUsed);
static HANDLE hHeapHandle = NULL;

static HANDLE hIoCompletionPort = NULL;

static HANDLE WaitableIoEventHandle = NULL;
static HANDLE WaitableExitEventHandle = NULL;

#define MAXHANDLES 3
HANDLE WaitHandles[MAXHANDLES] = { NULL, NULL, NULL };

#define USE_HEAP

IoCompletionInfo *
GetHeapAlloc(char *fromfunc)
{
	IoCompletionInfo *lpo;

#ifdef USE_HEAP
	lpo = (IoCompletionInfo *) HeapAlloc(hHeapHandle,
			     HEAP_ZERO_MEMORY,
			     sizeof(IoCompletionInfo));
#else
	lpo = (IoCompletionInfo *) calloc(1, sizeof(IoCompletionInfo));
#endif
#ifdef DEBUG
	if (debug > 3) {
		printf("Allocation %d memory for %s, ptr %x\n", sizeof(IoCompletionInfo), fromfunc, lpo);
	}
#endif
	return (lpo);
}

void
FreeHeap(IoCompletionInfo *lpo, char *fromfunc)
{
#ifdef DEBUG
	if (debug > 3)
	{
		printf("Freeing memory for %s, ptr %x\n", fromfunc, lpo);
	}
#endif

#ifdef USE_HEAP
	HeapFree(hHeapHandle, 0, lpo);
#else
	free(lpo);
#endif
}

transmitbuf_t *
get_trans_buf()
{
	transmitbuf_t *tb  = calloc(sizeof(transmitbuf_t), 1);
	tb->wsabuf.len = 0;
	tb->wsabuf.buf = (char *) &tb->pkt;
	return (tb);
}

void
free_trans_buf(transmitbuf_t *tb)
{
	free(tb);
}

HANDLE
get_io_event()
{
	return( WaitableIoEventHandle );
}
HANDLE
get_exit_event()
{
	return( WaitableExitEventHandle );
}

/*  This function will add an entry to the I/O completion port
 *  that will signal the I/O thread to exit (gracefully)
 */
static void
signal_io_completion_port_exit()
{
	if (!PostQueuedCompletionStatus(hIoCompletionPort, 0, 0, 0)) {
		msyslog(LOG_ERR, "Can't request service thread to exit: %m");
		exit(1);
	}
}

static void
iocompletionthread(void *NotUsed)
{
	BOOL bSuccess = FALSE;
	int errstatus = 0;
	DWORD BytesTransferred = 0;
	DWORD Key = 0;
	IoCompletionInfo * lpo = NULL;

	/*	Set the thread priority high enough so I/O will
	 *	preempt normal recv packet processing, but not
	 * 	higher than the timer sync thread.
	 */
	if (!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL)) {
		msyslog(LOG_ERR, "Can't set thread priority: %m");
	}

	while (TRUE) {
		bSuccess = GetQueuedCompletionStatus(hIoCompletionPort, 
					  &BytesTransferred, 
					  &Key, 
					  & (LPOVERLAPPED) lpo, 
					  INFINITE);
		if (lpo == NULL)
		{
#ifdef DEBUG
			if (debug > 2) {
				printf("Overlapped IO Thread Exits: \n");
			}
#endif
			break; /* fail */
		}
		
		/*
		 * Deal with errors
		 */
		errstatus = 0;
		if (!bSuccess)
		{
			errstatus = GetLastError();
			if (BytesTransferred == 0 && errstatus == WSA_OPERATION_ABORTED)
			{
#ifdef DEBUG
				if (debug > 2) {
					printf("Transfer Operation aborted\n");
				}
#endif
			}
			else
			{
				msyslog(LOG_ERR, "Error transferring packet after %d bytes: %m", BytesTransferred);
			}
		}

		/*
		 * Invoke the appropriate function based on
		 * the value of the request_type
		 */
		switch(lpo->request_type)
		{
		case CLOCK_READ:
			OnIoReadComplete(Key, lpo, BytesTransferred, errstatus);
			break;
		case SOCK_RECV:
			OnSocketRecv(Key, lpo, BytesTransferred, errstatus);
			break;
		case SOCK_SEND:
		case CLOCK_WRITE:
			OnWriteComplete(Key, lpo, BytesTransferred, errstatus);
			break;
		default:
#if DEBUG
			if (debug > 2) {
				printf("Unknown request type %d found in completion port\n",
					lpo->request_type);
			}
#endif
			break;
		}
	}
}

/*  Create/initialise the I/O creation port
 */
extern void
init_io_completion_port(
	void
	)
{

	/*
	 * Create a handle to the Heap
	 */
	hHeapHandle = HeapCreate(0, 20*sizeof(IoCompletionInfo), 0);
	if (hHeapHandle == NULL)
	{
		msyslog(LOG_ERR, "Can't initialize Heap: %m");
		exit(1);
	}


	/* Create the event used to signal an IO event
	 */
	WaitableIoEventHandle = CreateEvent(NULL, FALSE, FALSE, "WaitableIoEventHandle");
	if (WaitableIoEventHandle == NULL) {
		msyslog(LOG_ERR,
		"Can't create I/O event handle: %m - another process may be running - EXITING");
		exit(1);
	}
	/* Create the event used to signal an exit event
	 */
	WaitableExitEventHandle = CreateEvent(NULL, FALSE, FALSE, "WaitableExitEventHandle");
	if (WaitableExitEventHandle == NULL) {
		msyslog(LOG_ERR,
		"Can't create exit event handle: %m - another process may be running - EXITING");
		exit(1);
	}

	/* Create the IO completion port
	 */
	hIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (hIoCompletionPort == NULL) {
		msyslog(LOG_ERR, "Can't create I/O completion port: %m");
		exit(1);
	}
	
	/*
	 * Initialize the Wait Handles
	 */
	WaitHandles[0] = get_io_event();
	WaitHandles[1] = get_exit_event(); /* exit request */
	WaitHandles[2] = get_timer_handle();

	/* Have one thread servicing I/O - there were 4, but this would 
	 * somehow cause NTP to stop replying to ntpq requests; TODO
 	 */
	_beginthread(iocompletionthread, 0, NULL);
}
	

extern void
uninit_io_completion_port(
	void
	)
{
	if (hIoCompletionPort != NULL) {
		/*  Get each of the service threads to exit
		*/
		signal_io_completion_port_exit();
	}
}


static int QueueIORead( struct refclockio *rio, recvbuf_t *buff, IoCompletionInfo *lpo) {

	memset(lpo, 0, sizeof(IoCompletionInfo));
	memset(buff, 0, sizeof(recvbuf_t));

	lpo->request_type = CLOCK_READ;
	lpo->recv_buf = buff;

	buff->fd = rio->fd;
	if (!ReadFile((HANDLE) buff->fd, &buff->recv_buffer, sizeof(buff->recv_buffer), NULL, (LPOVERLAPPED) lpo)) {
		DWORD Result = GetLastError();
		switch (Result) {				
		case NO_ERROR :
		case ERROR_HANDLE_EOF :
		case ERROR_IO_PENDING :
			break ;

		/*
		 * Something bad happened
		 */
		default:
			msyslog(LOG_ERR, "Can't read from Refclock: %m");
			freerecvbuf(buff);
			return 0;
		}
	}
	return 1;
}



/* Return 1 on Successful Read */
static int 
OnIoReadComplete(DWORD i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	recvbuf_t *buff;
	recvbuf_t *newbuff;
	struct refclockio * rio = (struct refclockio *) i;

	/*
	 * Get the recvbuf pointer from the overlapped buffer.
	 */
	buff = (recvbuf_t *) lpo->recv_buf;
	/*
	 * Get a new recv buffer for the next packet
	 */
	newbuff = get_free_recv_buffer();
	if (newbuff == NULL) {
		/*
		 * recv buffers not available so we drop the packet
		 * and reuse the buffer.
		 */
		newbuff = buff;
	}
	else 
	{
		/*
		 * ignore 0 bytes read due to timeout's and closure on fd
		 */
		if (Bytes > 0 && errstatus != WSA_OPERATION_ABORTED) {
			get_systime(&buff->recv_time);
			buff->recv_length = (int) Bytes;
			buff->receiver = rio->clock_recv;
			buff->dstadr = NULL;
			buff->recv_srcclock = rio->srcclock;
			add_full_recv_buffer(buff);
			if( !SetEvent( WaitableIoEventHandle ) ) {
#ifdef DEBUG
				if (debug > 3) {
					printf( "Error %d setting IoEventHandle\n", GetLastError() );
				}
#endif
			}
		}
		else
		{
			freerecvbuf(buff);
		}
	}

	QueueIORead( rio, newbuff, lpo );
	return 1;
}

/*  Add a reference clock data structures I/O handles to
 *  the I/O completion port. Return 1 if any error.
 */  
int
io_completion_port_add_clock_io(
	struct refclockio *rio
	)
{
	IoCompletionInfo *lpo;
	recvbuf_t *buff;

	if (NULL == CreateIoCompletionPort((HANDLE) rio->fd, hIoCompletionPort, (DWORD) rio, 0)) {
		msyslog(LOG_ERR, "Can't add COM port to i/o completion port: %m");
		return 1;
	}

	lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_add_clock_io");
	if (lpo == NULL)
	{
		msyslog(LOG_ERR, "Can't allocate heap for completion port: %m");
		return 1;
	}

	buff = get_free_recv_buffer();

	if (buff == NULL)
	{
		msyslog(LOG_ERR, "Can't allocate memory for clock socket: %m");
		FreeHeap(lpo, "io_completion_port_add_clock_io");
		return 1;
	}
	QueueIORead( rio, buff, lpo );
	return 0;
}

/* Queue a receiver on a socket. Returns 0 if no buffer can be queued */

static unsigned long QueueSocketRecv(SOCKET s, recvbuf_t *buff, IoCompletionInfo *lpo) {
	
	int AddrLen;

	lpo->request_type = SOCK_RECV;
	lpo->recv_buf = buff;

	if (buff != NULL) {
		DWORD BytesReceived = 0;
		DWORD Flags = 0;
		buff->fd = s;
		AddrLen = sizeof(struct sockaddr_in);
		buff->src_addr_len = sizeof(struct sockaddr);

		if (SOCKET_ERROR == WSARecvFrom(buff->fd, &buff->wsabuff, 1, 
						&BytesReceived, &Flags, 
						(struct sockaddr *) &buff->recv_srcadr, (LPINT) &buff->src_addr_len, 
						(LPOVERLAPPED) lpo, NULL)) {
			DWORD Result = WSAGetLastError();
			switch (Result) {
				case NO_ERROR :
				case WSA_IO_INCOMPLETE :
				case WSA_WAIT_IO_COMPLETION :
				case WSA_IO_PENDING :
					break ;

				case WSAENOTSOCK :
					netsyslog(LOG_ERR, "Can't read from socket, because it isn't a socket: %m");
					/* return the buffer */
					freerecvbuf(buff);
					return 0;
					break;

				case WSAEFAULT :
					netsyslog(LOG_ERR, "The buffers parameter is incorrect: %m");
					/* return the buffer */
					freerecvbuf(buff);
					return 0;
				break;

				default :
				  /* nop */ ;
			}
		}
	}
	else 
		return 0;
	return 1;
}


/* Returns 0 if any Error */
static int 
OnSocketRecv(DWORD i, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	struct recvbuf *buff = NULL;
	recvbuf_t *newbuff;
	struct interface * inter = (struct interface *) i;
	
	/*  Convert the overlapped pointer back to a recvbuf pointer.
	*/
	
	/*
	 * Check returned structures
	 */
	if (lpo == NULL)
		return (1); /* Nothing to do */

	buff = lpo->recv_buf;
	/*
	 * Make sure we have a buffer
	 */
	if (buff == NULL) {
//		FreeHeap(lpo, "OnSocketRecv: Socket Closed");
		return (1);
	}

	/*
	 * If the socket is closed we get an Operation Aborted error
	 * Just clean up
	 */
	if (errstatus == WSA_OPERATION_ABORTED)
	{
		freerecvbuf(buff);
		FreeHeap(lpo, "OnSocketRecv: Socket Closed");
		return (1);
	}


	/*
	 * Get a new recv buffer for the next packet
	 */
	newbuff = get_free_recv_buffer();
	if (newbuff == NULL) {
		/*
		 * recv buffers not available so we drop the packet
		 * and reuse the buffer.
		 */
		newbuff = buff;
	}
	else 
	{
		/*
		 * If we keep it add some info to the structure
		 */
		if (Bytes > 0 && inter->ignore_packets == ISC_FALSE) {
			get_systime(&buff->recv_time);	
			buff->recv_length = (int) Bytes;
			buff->receiver = receive; 
			buff->dstadr = inter;
#ifdef DEBUG
			if (debug > 3)
  				printf("Received %d bytes in buffer %x from %s\n", Bytes, buff, stoa(&buff->recv_srcadr));
#endif
			add_full_recv_buffer(buff);
			/*
			 * Now signal we have something to process
			 */
			if( !SetEvent( WaitableIoEventHandle ) ) {
#ifdef DEBUG
				if (debug > 1) {
					printf( "Error %d setting IoEventHandle\n", GetLastError() );
				}
#endif
			}
		}
		else {
			freerecvbuf(buff);
		}
	}
	if (newbuff != NULL)
		QueueSocketRecv(inter->fd, newbuff, lpo);
	return 1;
}


/*  Add a socket handle to the I/O completion port, and send an I/O
 *  read request to the kernel.
 *
 *  Note: As per the winsock documentation, we use WSARecvFrom. Using
 *        ReadFile() is less efficient.
 */
extern int
io_completion_port_add_socket(SOCKET fd, struct interface *inter)
{
	IoCompletionInfo *lpo;
	recvbuf_t *buff;

	if (fd != INVALID_SOCKET) {
		if (NULL == CreateIoCompletionPort((HANDLE) fd, hIoCompletionPort,
						   (DWORD) inter, 0)) {
			msyslog(LOG_ERR, "Can't add socket to i/o completion port: %m");
			return 1;
		}
	}

	lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_add_socket");
	if (lpo == NULL)
	{
		msyslog(LOG_ERR, "Can't allocate heap for completion port: %m");
		return 1;
	}

	buff = get_free_recv_buffer();

	if (buff == NULL)
	{
		msyslog(LOG_ERR, "Can't allocate memory for network socket: %m");
		FreeHeap(lpo, "io_completion_port_add_socket");
		return 1;
	}

	QueueSocketRecv(fd, buff, lpo);
	return 0;
}

static int 
OnWriteComplete(DWORD Key, IoCompletionInfo *lpo, DWORD Bytes, int errstatus)
{
	transmitbuf_t *buff;
	(void) Bytes;
	(void) Key;

	buff = lpo->trans_buf;

	free_trans_buf(buff);

	if (errstatus == WSA_OPERATION_ABORTED)
		FreeHeap(lpo, "OnWriteComplete: Socket Closed");
	else
		FreeHeap(lpo, "OnWriteComplete");
	return 1;
}


DWORD	
io_completion_port_sendto(
	struct interface *inter,	
	struct pkt *pkt,	
	int len, 
	struct sockaddr_storage* dest)
{
	transmitbuf_t *buff = NULL;
	DWORD Result = ERROR_SUCCESS;
	int errval;
	int AddrLen;
	IoCompletionInfo *lpo;
	DWORD BytesSent = 0;
	DWORD Flags = 0;

	lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_sendto");

	if (lpo == NULL)
		return ERROR_OUTOFMEMORY;

	if (len <= sizeof(buff->pkt)) {
		buff = get_trans_buf();

		if (buff == NULL) {
			msyslog(LOG_ERR, "No more transmit buffers left - data discarded");
			FreeHeap(lpo, "io_completion_port_sendto");
			return ERROR_OUTOFMEMORY;
		}



		memcpy(&buff->pkt, pkt, len);
		buff->wsabuf.buf = buff->pkt;
		buff->wsabuf.len = len;

		AddrLen = sizeof(struct sockaddr_in);
		lpo->request_type = SOCK_SEND;
		lpo->trans_buf = buff;

		Result = WSASendTo(inter->fd, &buff->wsabuf, 1, &BytesSent, Flags, (struct sockaddr *) dest, AddrLen, (LPOVERLAPPED) lpo, NULL);

		if(Result == SOCKET_ERROR)
		{
			errval = WSAGetLastError();
			switch (errval) {

			case NO_ERROR :
			case WSA_IO_INCOMPLETE :
			case WSA_WAIT_IO_COMPLETION :
			case WSA_IO_PENDING :
				Result = ERROR_SUCCESS;
				break ;

			/*
			 * Something bad happened
			 */
			default :
				netsyslog(LOG_ERR, "WSASendTo - error sending message: %m");
				free_trans_buf(buff);
				FreeHeap(lpo, "io_completion_port_sendto");
				break;
			}
		}
#ifdef DEBUG
		if (debug > 3)
			printf("WSASendTo - %d bytes to %s : %d\n", len, stoa(dest), Result);
#endif
		return (Result);
	}
	else {
#ifdef DEBUG
		if (debug) printf("Packet too large: %d Bytes\n", len);
#endif
		return ERROR_INSUFFICIENT_BUFFER;
	}
}


/*
 * Async IO Write
 */
DWORD	
io_completion_port_write(
	HANDLE fd,	
	char *pkt,	
	int len)
{
	DWORD errval;
	transmitbuf_t *buff = NULL;
	DWORD lpNumberOfBytesWritten;
	DWORD Result = ERROR_INSUFFICIENT_BUFFER;
	IoCompletionInfo *lpo;

	lpo = (IoCompletionInfo *) GetHeapAlloc("io_completion_port_write");

	if (lpo == NULL)
		return ERROR_OUTOFMEMORY;

	if (len <= sizeof(buff->pkt)) {
		buff = get_trans_buf();
		if (buff == NULL) {
			msyslog(LOG_ERR, "No more transmit buffers left - data discarded");
			FreeHeap(lpo, "io_completion_port_write");
		}

		lpo->request_type = CLOCK_WRITE;
		lpo->trans_buf = buff;
		memcpy(&buff->pkt, pkt, len);

		Result = WriteFile(fd, buff->pkt, len, &lpNumberOfBytesWritten, (LPOVERLAPPED) lpo);

		if(Result == SOCKET_ERROR)
		{
			errval = WSAGetLastError();
			switch (errval) {

			case NO_ERROR :
			case WSA_IO_INCOMPLETE :
			case WSA_WAIT_IO_COMPLETION :
			case WSA_IO_PENDING :
				Result = ERROR_SUCCESS;
				break ;

			default :
				netsyslog(LOG_ERR, "WriteFile - error sending message: %m");
				free_trans_buf(buff);
				FreeHeap(lpo, "io_completion_port_write");
				break;
			}
		}
#ifdef DEBUG
			if (debug > 2) {
				printf("WriteFile - %d bytes %d\n", len, Result);
			}
#endif
			if (Result) return len;
	}
	else {
#ifdef DEBUG
		if (debug) printf("Packet too large: %d Bytes\n", len);
#endif
	}
	return Result;
}

/*
 * GetReceivedBuffers
 * Note that this is in effect the main loop for processing requests
 * both send and receive. This should be reimplemented
 */
int GetReceivedBuffers()
{
	isc_boolean_t have_packet = ISC_FALSE;
	while (!have_packet) {
		DWORD Index = WaitForMultipleObjects(MAXHANDLES, WaitHandles, FALSE, INFINITE);
		switch (Index) {
		case WAIT_OBJECT_0 + 0 : /* Io event */
# ifdef DEBUG
			if ( debug > 3 )
			{
				printf( "IoEvent occurred\n" );
			}
# endif
			have_packet = ISC_TRUE;
			break;
		case WAIT_OBJECT_0 + 1 : /* exit request */
			exit(0);
			break;
		case WAIT_OBJECT_0 + 2 : /* timer */
			timer();
			break;
		case WAIT_IO_COMPLETION : /* loop */
		case WAIT_TIMEOUT :
			break;
		case WAIT_FAILED:
			msyslog(LOG_ERR, "ntpd: WaitForMultipleObjects Failed: Error: %m");
			break;

			/* For now do nothing if not expected */
		default:
			break;		
				
		} /* switch */
	}

	return (full_recvbuffs());	/* get received buffers */
}

#else
  static int NonEmptyCompilationUnit;
#endif

