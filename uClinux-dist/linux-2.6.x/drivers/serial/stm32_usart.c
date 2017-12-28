
/*
 * Simple STM3210E-EVAL USART driver.
 *
 * Licensed under the GPL.
 */

#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/console.h>
#include <linux/serial.h>
#include <linux/serial_core.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/io.h>

#include <asm/arch/stm32f10x_conf.h>

#define USARTx  ((USART_TypeDef*)port->mapbase)


#define USART_STATUS_OVERRUN	(USART_GetFlagStatus(USARTx,USART_FLAG_ORE)==SET)
#define USART_STATUS_FRAME	(USART_GetFlagStatus(USARTx,USART_FLAG_FE)==SET)
#define USART_STATUS_PARITY	(USART_GetFlagStatus(USARTx,USART_FLAG_PE)==SET)
#define USART_STATUS_RXVALID	(USART_GetFlagStatus(USARTx,USART_FLAG_RXNE)==SET)

#define STM32_NR_USARTS		1
#define STM32_USART_NAME	"ttyS"
#define STM32_USART_MAJOR	56
#define STM32_USART_MINOR	5


/*------------------------------------------------------------------------------------
*usart ops prototype
*/
static unsigned int stm3210e_eval_tx_empty(struct uart_port *port);
static unsigned int stm3210e_eval_get_mctrl(struct uart_port *port);
static void stm3210e_eval_set_mctrl(struct uart_port *port, unsigned int mctrl);
static void stm3210e_eval_stop_tx(struct uart_port *port);
static void stm3210e_eval_start_tx(struct uart_port *port);
static void stm3210e_eval_stop_rx(struct uart_port *port);
static void stm3210e_eval_enable_ms(struct uart_port *port);
static void stm3210e_eval_break_ctl(struct uart_port *port, int ctl);
static int stm3210e_eval_startup(struct uart_port *port);
static void stm3210e_eval_shutdown(struct uart_port *port);
static void stm3210e_eval_set_termios(struct uart_port *port, struct ktermios *termios,
			      struct ktermios *old);
static const char *stm3210e_eval_type(struct uart_port *port);
static void stm3210e_eval_release_port(struct uart_port *port);
static int stm3210e_eval_request_port(struct uart_port *port);
static void stm3210e_eval_config_port(struct uart_port *port, int flags);
static int stm3210e_eval_verify_port(struct uart_port *port, struct serial_struct *ser);

/* ---------------------------------------------------------------------
 * Core USART driver operations
 */
static struct uart_ops stm3210e_eval_ops = {
	.tx_empty	= stm3210e_eval_tx_empty,
	.set_mctrl	= stm3210e_eval_set_mctrl,
	.get_mctrl	= stm3210e_eval_get_mctrl,
	.stop_tx	= stm3210e_eval_stop_tx,
	.start_tx	= stm3210e_eval_start_tx,
	.stop_rx	= stm3210e_eval_stop_rx,
	.enable_ms	= stm3210e_eval_enable_ms,
	.break_ctl	= stm3210e_eval_break_ctl,
	.startup	= stm3210e_eval_startup,
	.shutdown	= stm3210e_eval_shutdown,
	.set_termios	= stm3210e_eval_set_termios,
	.type		= stm3210e_eval_type,
	.release_port	= stm3210e_eval_release_port,
	.request_port	= stm3210e_eval_request_port,
	.config_port	= stm3210e_eval_config_port,
	.verify_port	= stm3210e_eval_verify_port
};

static unsigned int stm3210e_eval_tx_empty(struct uart_port *port)
{
	unsigned long flags;
	unsigned int ret;

	spin_lock_irqsave(&port->lock, flags);
	ret = USART_GetFlagStatus(USARTx,USART_FLAG_TXE);
	spin_unlock_irqrestore(&port->lock, flags);

	return ret ? TIOCSER_TEMT : 0;
}

static unsigned int stm3210e_eval_get_mctrl(struct uart_port *port)
{
	return TIOCM_CTS | TIOCM_DSR | TIOCM_CAR;
}

static void stm3210e_eval_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* N/A */
}

static void stm3210e_eval_stop_tx(struct uart_port *port)
{
	USART_ITConfig(USARTx,USART_IT_TXE,DISABLE);
}

static void stm3210e_eval_enable_ms(struct uart_port *port)
{
	/* N/A */
}

static void stm3210e_eval_break_ctl(struct uart_port *port, int ctl)
{
	/* N/A */
}

static int stm3210e_eval_transmit(struct uart_port *port);

static void stm3210e_eval_start_tx(struct uart_port *port)
{
	USART_ITConfig(USARTx,USART_IT_TXE,ENABLE);
	stm3210e_eval_transmit(port);
}

static void stm3210e_eval_stop_rx(struct uart_port *port)
{
	/*TODO: don't forward any more data (like !CREAD) */
	USART_ITConfig(USARTx,USART_IT_RXNE,DISABLE);
	USART_ClearITPendingBit(USARTx,USART_IT_RXNE);
}

static irqreturn_t stm3210e_eval_isr(int irq, void *dev_id);

static int stm3210e_eval_startup(struct uart_port *port)
{
	int ret;
	ret = request_irq(port->irq, stm3210e_eval_isr,
			  IRQF_DISABLED | IRQF_SAMPLE_RANDOM, "STM32 USART1 Port", port);
	if (ret)
	{
		printk("enable to request_irq for usart port\n");
		return ret;
	}
	USART_Cmd(USARTx, ENABLE);
	USART_ClearITPendingBit(USARTx,USART_IT_TXE);
	USART_ClearITPendingBit(USARTx,USART_IT_RXNE);	
	USART_ITConfig(USARTx,USART_IT_RXNE,ENABLE);

	return 0;
}

static void stm3210e_eval_shutdown(struct uart_port *port)
{
	USART_ITConfig(USARTx,USART_IT_RXNE,DISABLE);
	USART_ITConfig(USARTx,USART_IT_TXE,DISABLE);
	USART_ClearITPendingBit(USARTx,USART_IT_TXE);
	USART_ClearITPendingBit(USARTx,USART_IT_RXNE);	
	USART_Cmd(USARTx, DISABLE);
	USART_ClearFlag(USARTx,0xffff);//all flags
	free_irq(port->irq, port);
}

static const char *stm3210e_eval_type(struct uart_port *port)
{
	return "STM32 USART1 Port";
}

static void stm3210e_eval_set_termios(struct uart_port *port, struct ktermios *termios,
			      struct ktermios *old)
{
#if 0
	unsigned long flags;
	//unsigned int baud;

	spin_lock_irqsave(&port->lock, flags);

	port->read_status_mask = STATUS_RXVALID | STATUS_OVERRUN
		| STATUS_TXFULL;

	if (termios->c_iflag & INPCK)
		port->read_status_mask |=
			STATUS_PARITY | SSTATUS_FRAME;

	port->ignore_status_mask = 0;
	if (termios->c_iflag & IGNPAR)
		port->ignore_status_mask |= STATUS_PARITY
			| STATUS_FRAME | STATUS_OVERRUN;

	/* ignore all characters if CREAD is not set */
	if ((termios->c_cflag & CREAD) == 0)
		port->ignore_status_mask |=
			STATUS_RXVALID | STATUS_PARITY
			| STATUS_FRAME | STATUS_OVERRUN;

	/* update timeout */
	baud = uart_get_baud_rate(port, termios, old, 0, 460800);
	uart_update_timeout(port, termios->c_cflag, baud);

	spin_unlock_irqrestore(&port->lock, flags);
#endif
}

static int stm3210e_eval_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* we don't want the core code to modify any port params */
	return -EINVAL;
}

static void stm3210e_eval_xmit_char(USART_TypeDef* _USARTx,unsigned char c)
{
	 
	 USART_SendData(_USARTx,c);

 	 while(USART_GetFlagStatus(_USARTx, USART_FLAG_TXE) == RESET);
}

static int stm3210e_eval_transmit(struct uart_port *port)
{
	struct circ_buf *xmit  = &port->info->xmit;
	
		
	if (port->x_char) {
		stm3210e_eval_xmit_char(USARTx,port->x_char);
		port->x_char = 0;
		port->icount.tx++;
		return 1;
	}

	if (uart_circ_empty(xmit) || uart_tx_stopped(port))
	{
		stm3210e_eval_stop_tx(port);
		return 0;
	}

	stm3210e_eval_xmit_char(USARTx,xmit->buf[xmit->tail]);
	xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE-1);
	port->icount.tx++;
	
	/* wake up */
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);

	return 1;
}

static int stm3210e_eval_receive(struct uart_port *port)
{
	struct tty_struct *tty = port->info->tty;
	unsigned char ch = 0;
	char flag = TTY_NORMAL;
	uint16_t  STATUS=(uint16_t)(USARTx->SR);
	while(((uint16_t)(USARTx->SR)&(uint16_t)USART_FLAG_RXNE))
	{
		flag = TTY_NORMAL;
		ch = USART_ReceiveData(USARTx);
		port->icount.rx++;
			
		/* stats */
		if(
			(STATUS & (
			        (uint16_t)(
				((uint16_t)USART_FLAG_ORE)|
				((uint16_t)USART_FLAG_PE)|
				((uint16_t)USART_FLAG_FE))
			          )
		  	)
		  )
		{
			if ((STATUS&(uint16_t)USART_FLAG_ORE))
			{
				tty_insert_flip_char(tty, ch, flag);
				port->icount.overrun++;
				flag=TTY_OVERRUN;
			}
			if ((STATUS&(uint16_t)USART_FLAG_PE))
			{
				port->icount.parity++;
				flag=TTY_PARITY;
			}
			if ((STATUS&(uint16_t)USART_FLAG_FE))
			{
				port->icount.frame++;
				flag=TTY_FRAME;
			}
		}
		tty_insert_flip_char(tty, (flag==TTY_NORMAL)?ch:0, flag);
		STATUS=(uint16_t)(USARTx->SR);
	}
	tty_flip_buffer_push(port->info->tty);
	return 1;
}

static irqreturn_t stm3210e_eval_isr(int irq, void *dev_id)
{
	struct uart_port *port = dev_id;

	if(USART_GetITStatus(USARTx,USART_IT_RXNE)==SET)
	{	
		stm3210e_eval_receive(port);
	}
	if(USART_GetITStatus(USARTx,USART_IT_TXE)==SET)
	{
		USART_ClearITPendingBit(USARTx,USART_IT_TXE);
		stm3210e_eval_transmit(port);
	}

	return IRQ_HANDLED;
}

static void stm3210e_eval_release_port(struct uart_port *port)
{

}

static int stm3210e_eval_request_port(struct uart_port *port)
{
	return 0;
}

static void stm3210e_eval_config_port(struct uart_port *port, int flags)
{
	if (!stm3210e_eval_request_port(port))
		port->type = PORT_STMUSART;
}


 
static struct uart_port stm3210e_eval_usart1_port = {
	.membase = (u8*)USART1,
	.iotype = SERIAL_IO_MEM,
	.irq = USART1_IRQn,
	.fifosize = 0,
	.flags = UPF_BOOT_AUTOCONF,
	.line = 0,
	.ops = &stm3210e_eval_ops
	};
	

#ifdef CONFIG_SERIAL_STM3210E_EVAL_CONSOLE
/* ---------------------------------------------------------------------
 * Console driver && operations
 */

static void stm3210e_eval_console_putchar(struct uart_port *port, int ch)
{	 
	 USART_SendData(USART1,ch);
 	 while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	
}

static void stm3210e_eval_console_write(struct console *co, const char *s,
				unsigned int count)
{
	struct uart_port *port = &stm3210e_eval_usart1_port;
	unsigned long flags;
	unsigned char irxne,itxe;
	int locked = 1;

	if (oops_in_progress) {
		locked = spin_trylock_irqsave(&port->lock, flags);
	} else
		spin_lock_irqsave(&port->lock, flags);

	/* save and disable interrupt */
	irxne=(USART_GetITStatus(USARTx,USART_IT_RXNE)==SET);
	itxe=(USART_GetITStatus(USARTx,USART_IT_TXE)==SET);
	if(irxne)
		USART_ITConfig(USARTx,USART_IT_RXNE,DISABLE);
	if(itxe)	
		USART_ITConfig(USARTx,USART_IT_TXE,DISABLE);
	

	uart_console_write(port, s, count, stm3210e_eval_console_putchar);


	/* restore interrupt state */
	if(irxne)
		USART_ITConfig(USARTx,USART_IT_RXNE,ENABLE);
	if(itxe)
		USART_ITConfig(USARTx,USART_IT_TXE,ENABLE);
		

	if (locked)
		spin_unlock_irqrestore(&port->lock, flags);
}

/*static __init int stm3210e_eval_console_setup(struct console *co, char *options), 
which should initialize the serial port hardware according to the given options. 
If your serial hardware is already initialized by the Firmware,
 then you don't need to do anything in this function. */
 static int __init stm3210e_eval_console_setup(struct console *co, char *options)
{
	return 0;
}
 
static struct uart_driver stm3210e_eval_uart_driver;

static struct console stm3210e_eval_console = {
	.name	= STM32_USART_NAME,
	.write	= stm3210e_eval_console_write,
	.device	= uart_console_device,
	.setup	= stm3210e_eval_console_setup,
	.flags	= CON_PRINTBUFFER,
	.index	= -1, /* Specified on the cmdline (e.g. console=ttyST0 ) */
	.data	= &stm3210e_eval_uart_driver,
};

static int __init stm3210e_eval_console_init(void)
{
	register_console(&stm3210e_eval_console);
	return 0;
}
 
console_initcall(stm3210e_eval_console_init);

#endif /* SERIAL_STM32_USART_CONSOLE */

static struct uart_driver stm3210e_eval_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= "STM32 USART DRIVER",
	.dev_name	= STM32_USART_NAME,
	.major		= STM32_USART_MAJOR,
	.minor		= STM32_USART_MINOR,
	.nr		= STM32_NR_USARTS,
#ifdef CONFIG_SERIAL_STM3210E_EVAL_CONSOLE
	.cons		= &stm3210e_eval_console,
#endif
};




/* ---------------------------------------------------------------------
 * Module setup/teardown
 */
 


int __init stm3210e_eval_usart_driver_init(void)
{
	int ret;
	stm3210e_eval_usart1_port.mapbase = (u32) stm3210e_eval_usart1_port.membase;
	ret = uart_register_driver(&stm3210e_eval_uart_driver);
	if (ret)
		goto err_uart;
	
	ret = uart_add_one_port(&stm3210e_eval_uart_driver, &stm3210e_eval_usart1_port);
	if (ret)
		goto err_uart;
		
	return 0;
err_uart:
	printk(KERN_ERR "registering STM32 usart[1] driver failed: err=%i\n",ret);
	return ret;
}

void __exit stm3210e_eval_usart_driver_exit(void)
{
	uart_unregister_driver(&stm3210e_eval_uart_driver);
}

module_init(stm3210e_eval_usart_driver_init);
module_exit(stm3210e_eval_usart_driver_exit);

MODULE_AUTHOR("MCD Application Team");
MODULE_DESCRIPTION("STM32 serial driver");
MODULE_LICENSE("GPL");
