#include <limits.h>
//
// Created by alexander on 01/11/2020.
//

#include "main.h"
#include "main_target.h"
#include <string.h>

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern struct LED led[3];

int str_cmp(uint8_t*, char*, size_t);
void always_glow(struct LED*);
void always_zero(struct LED*);
void calc_up(struct LED* led);

void calc_middle(struct LED* led);

void calc_down(struct LED* led);

void process_cmd(uint8_t* Buf, uint32_t* Len)
{
    char* cmd=(char*)Buf;
    if(*Len)
    {
        if(strcmp(cmd, "first")>=0)
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            if(strcmp(cmd+sizeof("first "), "on") == 0)
            {
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
                led->curr_step=always_glow;
            }
            if(strcmp(cmd+sizeof("first "), "off") == 0)
            {
                led->curr_step=always_zero;
            }
            if(strcmp(cmd+sizeof("first "), "resume")==0)
                led->curr_step=calc_up;
        }
//        if(strcmp(cmd, "second") == 0)
//        {
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//            if(strcmp(cmd+sizeof("second "), "on") == 0)
//            {
//                led->curr_step=always_glow;
//            }
//            if(strcmp(cmd+sizeof("second "), "off") == 0)
//            {
//                led->curr_step=always_zero;
//            }
//        }
//        if(strcmp(cmd, "third") == 0)
//        {
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
//            if(strcmp(cmd+sizeof("third "), "on") == 0)
//            {
//                led->curr_step=always_glow;
//            }
//            if(strcmp(cmd+sizeof("third "), "off") == 0)
//            {
//                led->curr_step=always_zero;
//            }
//        }
        if(cmd[0]=='0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        if(cmd[0]=='1')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void always_glow(struct LED* led)
{
    *led->pin=0;
}

void always_zero(struct LED* led)
{
    *led->pin=COUNTER_PERIOD;
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

void my_delay(int mc_s)
{
    if (mc_s <= 0)
        return;
    __HAL_TIM_SET_COUNTER(&htim3, 0);
    while (__HAL_TIM_GET_COUNTER(&htim3) < mc_s)
    {}
}
int str_cmp(uint8_t* buf, char* str, size_t n)
{
    ++n;
    while (--n)
    {
    }
}