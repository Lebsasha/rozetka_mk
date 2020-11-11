#include <limits.h>
//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include "main_target.h"


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

void process_cmd(uint8_t* Buf, uint32_t* Len)
{
    if(Len)
    {
        if(Buf[0]=='1')
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        }
        if(Buf[0]=='0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

    }
}

void calc_up(struct LED* led)
{
    ++led->i;
    *led->pin=COUNTER_PERIOD-(COUNTER_PERIOD * (sin((double) (led->i) / led->detailyty * M_PI - M_PI_2) + 1) / 2);
    if (led->i == led->detailyty && led->curr_step == calc_up)
    {
        led->curr_step = calc_middle;
        led->i = 0;
    }
}

void calc_middle(struct LED* led)
{
    ++led->i;
    *led->pin=0;
    if (led->i == led->detailyty && led->curr_step == calc_middle)
        led->curr_step = calc_down;
}

void calc_down(struct LED* led)
{
    --led->i;
    if(led->i>2)
        *led->pin=COUNTER_PERIOD-(COUNTER_PERIOD * (sin((double) (led->i) / led->detailyty * M_PI - M_PI_2) + 1) / 2);
    else
        *led->pin=COUNTER_PERIOD-0;
    if (led->i == 0 && led->curr_step == calc_down)
        led->curr_step = calc_up;
}
