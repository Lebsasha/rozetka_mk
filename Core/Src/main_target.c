#include <limits.h>
//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include "main_target.h"

extern TIM_HandleTypeDef htim3;

void calc_up(struct LED* led);

void calc_middle(struct LED* led);

void calc_down(struct LED* led);

void my_delay(int mc_s)
{
    if (mc_s <= 0)
        return;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < mc_s)
    {}
}

void ctor_LED(struct LED* led, int detailyty, int pin)
{
    led->detailyty = detailyty;
    led->pin = pin;
    led->counter = 0;
    led->i = 0;
    led->ampl = 0;
    led->curr_step = calc_up;
}

void calc_down(struct LED* led)
{
    ++led->counter;
    if (led->counter == 100)
    {
        --led->i;
        led->ampl = (unsigned char) (100 * (sin((double) (led->i) / led->detailyty * M_PI - M_PI_2) + 1) / 2);
        //        100 ticks per 10^-4
        led->counter = 0;
        if (led->i == 0)
            led->curr_step = calc_up;
    }
    if (led->counter < led->ampl)// 100 ticks per 10^-4
        HAL_GPIO_WritePin(GPIOA, led->pin, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(GPIOA, led->pin, GPIO_PIN_SET);
}

void calc_middle(struct LED* led)
{
    ++led->counter;
    if (led->counter == 100)
    {
        ++led->i;
        led->counter = 0;
        if (led->i == led->detailyty)
            led->curr_step = calc_down;
    }
    HAL_GPIO_WritePin(GPIOA, led->pin, GPIO_PIN_RESET);
}

void calc_up(struct LED* led)
{
    ++led->counter;
    if (led->counter == 100)
    {
        ++led->i;
        led->ampl = (unsigned char) (100 * (sin((double) (led->i) / led->detailyty * M_PI - M_PI_2) + 1) / 2);
        //        100 ticks per 10^-4
        led->counter = 0;
        if (led->i == led->detailyty)
        {
            led->curr_step = calc_middle;
            led->i = 0;
        }
    }
    if (led->counter < led->ampl)// 100 ticks per 10^-4
        HAL_GPIO_WritePin(GPIOA, led->pin, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(GPIOA, led->pin, GPIO_PIN_SET);
}
