//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include <assert.h>

const int MY_FREQ=100;
extern const int DETAILYTY;
extern TIM_HandleTypeDef htim1;

void my_delay(int mc_s)
{
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < mc_s)
    {}
}

void soft_glow(GPIO_TypeDef *port, int pin, int duty_cycle, int mc_s)
{
    assert(duty_cycle >=0 && duty_cycle<DETAILYTY+1);
    static const int time= 1000000 / MY_FREQ;// 10000
    while((mc_s-=time) >= 0)
    {
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);//on
        my_delay(duty_cycle * time / DETAILYTY);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);//off
        my_delay((DETAILYTY - duty_cycle) * time / DETAILYTY);
    }
}

