#include "main.h"
#include "main_target.h"
#include <usbd_cdc_if.h>

extern TIM_HandleTypeDef htim1;
extern char* cmd;
extern volatile uint32_t period;
const uint16_t a = -20;
const uint16_t b = 8008;

void process_cmd(const uint8_t* command, const uint32_t len)
{
    if (len)
    {
        if (*command == 's' && *(command + 1) == 'n' && *(command + 2) == 'd')
        {
            long per = strtol((char*) command + sizeof("snd ") - 1, NULL, 10);
            period=(72e6 / per / htim1.Instance->PSC/2);// /a*8+b)/2;
           if (errno == ERANGE)
            {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
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
}

void my_delay(int takts)
{
    if (htim1.Instance->ARR < takts || takts <= 0)
    {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        return;
    }
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < takts)
    {}
}
