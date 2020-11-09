#include <limits.h>
//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include "main_target.h"

const uint16_t COUNTER_PERIOD=100;

extern TIM_HandleTypeDef htim1;
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

void ctor_LED(struct LED* led, uint16_t detailyty, volatile uint32_t* pin, char num)
{
    led->pin=pin;
    led->detailyty = detailyty;
    led->i = 0;
    led->num=num;
    led->curr_step = calc_up;
}

void calc_up(struct LED* led)
{
    ++led->i;

    *led->pin=COUNTER_PERIOD-led->i*COUNTER_PERIOD/led->detailyty;

//    if(led->num==0)
//        htim1.Instance->CCR1=led->i*COUNTER_PERIOD/led->detailyty;

    if (led->i == led->detailyty)
    {
        led->curr_step = calc_middle;
        led->i = 0;
    }
}

void calc_middle(struct LED* led)
{
    ++led->i;
    *led->pin=0;
//    if(led->num==0)
//        htim1.Instance->CCR1=COUNTER_PERIOD;
    if (led->i == led->detailyty)
        led->curr_step = calc_down;
}

void calc_down(struct LED* led)
{
    --led->i;
    *led->pin=COUNTER_PERIOD-led->i*COUNTER_PERIOD/led->detailyty;
//    if(led->num==0)
//        htim1.Instance->CCR1=led->i*COUNTER_PERIOD/led->detailyty;

    if (led->i == 0)
        led->curr_step = calc_up;
}
