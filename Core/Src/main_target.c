#include "main.h"
#include "main_target.h"
#include <usbd_cdc_if.h>

extern TIM_HandleTypeDef htim1;
extern char* cmd;

char RxBuff[1024];
volatile uint16_t WrPtr = 0;

void process_cmd(uint8_t* command, uint32_t len)
{
    if (!len)
    {
    	return;
    }
    command[len] = 0;
	if (WrPtr == 0)
	{
		if (command[0] == '0')
		{
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			return;
		}
		if (command[0] == '1')
		{
			HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
			CDC_Transmit_FS((uint8_t*) "1 is pressed\n", sizeof("1 is pressed"));
			return;
		}
		if (*command == '2' && command[1] == '1' && command[2] == '1')
		{
			command = command+3;
			len -= 3;
		}
		else
			return;
	}
	memcpy(RxBuff + WrPtr, command, len);
	WrPtr += len;
	RxBuff[WrPtr] = 0;
}

void my_delay(int mc_s)
{
    if (htim1.Instance->ARR < mc_s || mc_s <= 0)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        return;
    }
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < mc_s)
    {}
}
