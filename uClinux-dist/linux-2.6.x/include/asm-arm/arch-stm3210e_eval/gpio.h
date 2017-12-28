#include <asm/arch/stm32f10x_conf.h>
#include <linux/errno.h>

#define GPIOx	((GPIO_TypeDef*)((GPIOF_BASE)))

#define __gpio_get_value(gpio_pin)		GPIO_ReadInputDataBit(GPIOx,gpio_pin)

#define __gpio_get_output_value(gpio_pin)	GPIO_ReadOutputDataBit(GPIOx,gpio_pin)

#define __gpio_set_value(gpio_pin, value)	GPIO_WriteBit(GPIOx,1<<(gpio_pin),value)

#define gpio_get_value(gpio_pin)		__gpio_get_value(1<<(gpio_pin))

#define gpio_get_output_value(gpio_pin)		__gpio_get_output_value(1<<(gpio_pin))

#define gpio_set_value(gpio_pin, value)		__gpio_set_value(gpio_pin, value)

static inline void gpio_direction_input(unsigned gpio_pin, int value)
{
	GPIO_InitTypeDef * GPIO_Config;
	GPIO_Config = kmalloc(sizeof(GPIO_InitTypeDef),GFP_KERNEL);
	GPIO_Config->GPIO_Pin=(1<<(gpio_pin));
	switch(value)
	{
	case 0: GPIO_Config->GPIO_Speed=GPIO_Speed_50MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPD;
		break;
	case 1: GPIO_Config->GPIO_Speed=GPIO_Speed_50MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPU;
		break;
	case 2: GPIO_Config->GPIO_Speed=GPIO_Speed_2MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPD;
		break;
	case 3: GPIO_Config->GPIO_Speed=GPIO_Speed_2MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPU;
		break;
	case 4: GPIO_Config->GPIO_Speed=GPIO_Speed_10MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPD;
		break;	
	case 5: GPIO_Config->GPIO_Speed=GPIO_Speed_10MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_IPU;
		break;
	default:
		GPIO_Config->GPIO_Speed=GPIO_Speed_2MHz;
		GPIO_Config->GPIO_Mode=GPIO_Mode_IN_FLOATING;
	}
	GPIO_Init(GPIOx,GPIO_Config);
	kfree(GPIO_Config);
}

static inline void gpio_direction_output(unsigned gpio_pin, int value)
{
	GPIO_InitTypeDef * GPIO_Config;
	GPIO_Config = kmalloc(sizeof(GPIO_InitTypeDef),GFP_KERNEL);
	GPIO_Config->GPIO_Pin=(1<<(gpio_pin));
	switch(value)
	{
	case 0: GPIO_Config->GPIO_Speed=GPIO_Speed_50MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_PP;
		break;
	case 1: GPIO_Config->GPIO_Speed=GPIO_Speed_50MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_OD;
		break;
	case 2: GPIO_Config->GPIO_Speed=GPIO_Speed_2MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_PP;
		break;
	case 3: GPIO_Config->GPIO_Speed=GPIO_Speed_2MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_OD;
		break;
	case 4: GPIO_Config->GPIO_Speed=GPIO_Speed_10MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_PP;
		break;	
	case 5: GPIO_Config->GPIO_Speed=GPIO_Speed_10MHz;
  		GPIO_Config->GPIO_Mode=GPIO_Mode_Out_OD;
		break;
	default:
		kfree(GPIO_Config);
		return;
	}
	GPIO_Init(GPIOx,GPIO_Config);
	kfree(GPIO_Config);
}

int gpio_request(unsigned gpio_pin, char * name)
{
	return 0;
}

void gpio_free(unsigned gpio_pin)
{
	return;
}