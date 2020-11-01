//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include <assert.h>
#include <math.h>

const int DETAILYTY=100;
static const int MY_FREQ=100;
void soft_glow(GPIO_TypeDef *port, int pin, int duty_cycle, int mc_s);
extern TIM_HandleTypeDef htim1;
void main_f()
{

    //assert(1000/MY_FREQ*DETAILYTY==1000);
    HAL_TIM_Base_Start(&htim1);
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, 1);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 0);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, 0);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, 1);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, 0);
    HAL_Delay(500);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, 1);
    while (1)
    {
        for (int i = 0; i < DETAILYTY; i += 1)
            soft_glow(GPIOA, GPIO_PIN_10, (int) (DETAILYTY * (sin((double) (i) / DETAILYTY * M_PI - M_PI_2) + 1) / 2), 10000);

        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        HAL_Delay(1000);

        for (int i = DETAILYTY; i >= 0; i -= 1)
            soft_glow(GPIOA, GPIO_PIN_10, (int) (DETAILYTY * (sin((double) (i) / DETAILYTY * M_PI - M_PI_2) + 1) / 2), 10000);
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
    }
}
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


