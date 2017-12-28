#include <linux/module.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/errno.h>

#include <asm/dma.h>

#include <asm/mach/dma.h>

#include <asm/arch/stm32f10x_conf.h>
#if 0
struct dma_struct {
	void		*addr;		/* single DMA address		*/
	unsigned long	count;		/* single DMA size		*/
	struct scatterlist buf;		/* single DMA			*/
	int		sgcount;	/* number of DMA SG		*/
	struct scatterlist *sg;		/* DMA Scatter-Gather List	*/

	unsigned int	active:1;	/* Transfer active		*/
	unsigned int	invalid:1;	/* Address/Count changed	*/

	dmamode_t	dma_mode;	/* DMA mode			*/
	int		speed;		/* DMA speed			*/

	unsigned int	lock;		/* Device is allocated		*/
	const char	*device_id;	/* Device name			*/

	unsigned int	dma_base;	/* Controller base address	*/
	int		dma_irq;	/* Controller IRQ		*/
	struct scatterlist cur_sg;	/* Current controller buffer	*/
	unsigned int	state;

	struct dma_ops	*d_ops;
};
//#endif
#endif
#error incomplete implementation
void dma_enable(dmach_t channel, dma_t * dma_chan)
{
}

void dma_disable(dmach_t channel, dma_t * dma_chan)
{
	/* N/A */
}

struct dma_ops stm_dma_ops = {
	.enable		= dma_enable,		/* mandatory */
	.disable	= dma_disable,		/* mandatory */
	.type		= "shared.",
};

static dma_t  stm32_dma_channels[MAX_DMA_CHANNELS];

struct dma_struct stm_dma2_chan_4_5 = {
	.device_id	= "stm_dma2_chan_4_5",
	.dma_irq	= DMA2_Channel4_5_IRQn,
	.dma_base	= DMA2_Channel4_BASE,
};

static void __init init_dma_channel(void){
	stm32_dma_channels[0] = stm_dma2_chan_4_5;
}

void __init arch_dma_init(dma_t* dma_chan){
	init_dma_channel();
	dma_chan = stm32_dma_channels;
}

#define ValidChan(x) ((x < 0) || (x < MAX_DMA_CHANNELS ))

static dma_t get_dma_channel_data(int dma_ch)
{
//	if(ValidChan(dma_ch))
		return	stm32_dma_channels[dma_ch];
}

int set_dma_callback(int dma_ch, irq_handler_t sdh_dma_irq, void *host)
{
	dma_t dma_channel;
	dma_channel = get_dma_channel_data(dma_ch);
	if(dma_channel)
		return request_irq((dma_channel->dma_irq),sdh_dma_irq,0,dma_channel->device_id,host);
	return -1;
}