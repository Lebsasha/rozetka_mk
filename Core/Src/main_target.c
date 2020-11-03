//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include <assert.h>

const int MY_FREQ=100;
//extern const int DETAILYTY_1;
extern TIM_HandleTypeDef htim1;

void my_delay(int mc_s)
{
    if(mc_s<=0)
        return;
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < mc_s)
    {}
}

void soft_glow(GPIO_TypeDef *port, int pin, int duty_cycle, int mc_s, int detailyty)
{
    assert(duty_cycle >=0 && duty_cycle < detailyty + 1);
    static const int TIME= 1000000 / MY_FREQ;// 10000
    while((mc_s-=TIME) >= 0)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);//on
        my_delay(duty_cycle * TIME / detailyty);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);//off
        my_delay((detailyty - duty_cycle) * TIME / detailyty);
    }
}


