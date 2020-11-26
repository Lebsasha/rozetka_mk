#include "main.h"
#include "main_target.h"
#include <usbd_cdc_if.h>

extern TIM_HandleTypeDef htim1;
extern char* cmd;
extern volatile uint32_t count;
bool if_ping_req=false;
size_t need_length=0;
size_t arrived_length=0;
uint8_t data[4096];

void process_cmd(const uint8_t* command, const uint32_t len)
{
    if (len)
    {
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '1')
        {
            cmd=(char*) command+3;
        } else
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '2' && *(command + 3) == 's')///start
        {
            count=0;
        } else
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '2' && *(command + 3) == 'e')///end
        {
            int time=count;
            CDC_Transmit_FS((uint8_t*)&time, sizeof(int));
        } else
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '3')
        {
            count=0;
            need_length=strtol((char*)command+sizeof("213 ")-1, NULL, 10);
            if (errno == ERANGE)
            {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            }
        } else
        if (need_length)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            strncpy((char*)data+arrived_length, (char*)command, len);
            arrived_length+=len;
            if(arrived_length>=need_length)
            {
                if_ping_req=true;
            }
        } else
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        else
        if (command[0] == '1')
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            CDC_Transmit_FS((uint8_t*) "1 is pressed", sizeof("1 is pressed"));
        }
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
