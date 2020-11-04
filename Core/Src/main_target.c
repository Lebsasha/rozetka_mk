//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include "main_target.h"
#include <assert.h>

const int MY_FREQ=100;
extern const int DETAILYTY_1;
extern const int DETAILYTY_2;
extern const int DETAILYTY_3;
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

void calc_1()
{
    static int counter=0;
    static int i=0;
    static int ampl=0;
    ++counter;
    if (counter < 10000)// 0-1s
    {
        if (!(counter % 100))// DETAILYTY_1
        {
            ++i;
            ampl=(int) (100*(sin((double) (i) / 100 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_1
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        }
    }
    else if (counter == 10000)// 1-2s
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
    }
    else if (counter >=20000 && counter < 30000)// 2-3s
    {
        if (!(counter % 100))// DETAILYTY_1
        {
            --i;
            ampl=(int) (100*(sin((double) (i) / 100 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_1
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET);
        }
    }
    else if (counter == 30000)
        counter-=30000+1;//because above ++counter;
}

/**
 * @note 100 ticks per 10^-4 * DETAILYTY_1 = 1 s
 * @note 100 ticks per 10^-4 * DETAILYTY_2 = 1.3 s
 * @note 100 ticks per 10^-4 * DETAILYTY_3 = 1.7 s
 */
void calc_2()
{
    static int counter=0;
    static int i=0;
    static int ampl=0;
    ++counter;
    if (counter < 13000)// 0-1.3s
    {
        if (!(counter % 100))// some strange number
        {
            ++i;
            ampl=(int) (100*(sin((double) (i) / 130 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_2
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        }
    }
    else if (counter == 13000)// 1.3-2.6s
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
    }
    else if (counter >=26000 && counter < 39000)// 2.6-3.9s
    {
        if (!(counter % 100))// some strange number
        {
            --i;
            ampl=(int) (100*(sin((double) (i) / 130 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_2
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
        }
    }
    else if (counter == 39000)
        counter-=39000+1;//because above ++counter;
}

void calc_3()
{
    static int counter=0;
    static int i=0;
    static int ampl=0;
    ++counter;
    if (counter < 17000)// 0-1.7s
    {
        if (!(counter % 100))// DETAILYTY_3
        {
            ++i;
            ampl=(int) (100*(sin((double) (i) / 170 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_3
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        }
    }
    else if (counter == 17000)// 1.7-3.4s
    {
        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
    }
    else if (counter >=34000 && counter < 51000)// 3.4-5.1s
    {
        if (!(counter % 100))// some strange number
        {
            --i;
            ampl=(int) (100*(sin((double) (i) / 170 * M_PI - M_PI_2) + 1) / 2);
            //        100 ticks per 10^-4      DETAILYTY_3
        }
        if(counter%100<ampl)// 100 ticks per 10^-4
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);
        }
        else
        {
            HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        }
    }
    else if (counter == 51000)
        counter-=51000+1;//because above ++counter;
}