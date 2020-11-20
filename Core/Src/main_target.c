#include "main.h"
#include "main_target.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <usbd_cdc_if.h>
#include <assert.h>

extern TIM_HandleTypeDef htim1;
extern char* cmd;

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)
    {
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '1')
        {
            cmd=(char*) command+3;
        }
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        if (command[0] == '1')
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            CDC_Transmit_FS((uint8_t*) "1 is pressed", sizeof("1 is pressed"));
        }
    }
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
