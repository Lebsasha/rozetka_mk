#include "main.h"
#include "main_target.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <usbd_cdc_if.h>
#include <assert.h>

extern TIM_HandleTypeDef htim1;

unsigned char transm(char*);

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)
    {
        if (*command == '2' && *(command + 1) == '1' && *(command + 2) == '1')
        {
            char* next_num = NULL;
            int n_size = strtol((char*) command + 3, &next_num, 10);
            int packet_size = strtol(next_num, NULL, 10);
            uint8_t* x = (uint8_t*) "1234567890";//LONG_STRING;
            uint32_t count = 0;
            CDC_Transmit_FS((uint8_t*) &n_size, sizeof(n_size));
            CDC_Transmit_FS((uint8_t*) &packet_size, sizeof(packet_size));
            transm(x);
            transm("Before errno");
            if (errno == ERANGE || packet_size > strlen((char*) x))
            {
                transm("In errno");
//                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//                HAL_Delay(200);
//                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            }
            transm("After errno");
            for (int i = 0; i < n_size; ++i)
            {
                if (__HAL_TIM_GET_FLAG(&htim1, TIM_FLAG_UPDATE) != RESET)
                {
                    __HAL_TIM_CLEAR_IT(&htim1, TIM_IT_UPDATE);
                    ++count;
//                    if (count == 0)
//                        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
                }
                transm("In for");
                CDC_Transmit_FS(x, packet_size);
            }
            transm("After for");
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//            HAL_Delay(200);
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//            HAL_Delay(200);
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//            HAL_Delay(1000);
            CDC_Transmit_FS((uint8_t*) &count, sizeof(count));
            CDC_Transmit_FS((uint8_t*) "end", sizeof("end"));
        }
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        if (command[0] == '1')
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            CDC_Transmit_FS((uint8_t*) "1 is pressed", sizeof("1 is pressed"));
            CDC_Transmit_FS((uint8_t*) "11 is pressed", sizeof("11 is pressed"));
        }
    }
}

unsigned char transm(char* s)
{ return CDC_Transmit_FS((uint8_t*) s, strlen(s)); }

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
