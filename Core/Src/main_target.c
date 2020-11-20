#include "main.h"
#include "main_target.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <usbd_cdc_if.h>
#include <assert.h>

extern TIM_HandleTypeDef htim1;

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)
    {
        if (*command == '1' && *(command + 1) == '1' && *(command + 2) == '1')
        {
            char* next_num = NULL;
            int n_size = strtol((char*) command + 3, &next_num, 10);
            int packet_size = strtol(next_num, NULL, 10);
            uint8_t* x = (uint8_t*) LONG_STRING;
            uint32_t count=0;
            if (errno == ERANGE || packet_size > strlen((char*)x))
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            for (int i = 0; i < n_size; ++i)
            {
                if (__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE) != RESET)
                {
                    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
                    ++count;
                    if(count==0)
                        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
                }
                CDC_Transmit_FS(x, packet_size);
            }
            CDC_Transmit_FS((uint8_t*)&count, sizeof(count));
        }
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        if (command[0] == '1')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void my_delay(int mc_s)
{
    if (mc_s <= 0)
        return;
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < mc_s)
    {}
}
