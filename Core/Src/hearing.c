#include <string.h>
#include "hearing.h"

//TODO Write for both ports
//TODO Add release config
//TODO Sometime cleanup code
//TODO Split system and custom functions
//TODO ETR instead in clock

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
const int16_t sine_ampl = (1U << (sizeof(sine_ampl) * 8 - 1)) - 1;//max val of int16_t
const uint16_t arr_size = 1024; /*!<for optimization purposes*/
int16_t f_dots[1024];

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t* CCR)
{
    for (int i = 0; i < arr_size; ++i)
        f_dots[i] = (int16_t)(sine_ampl / 2.0 - sine_ampl / 2.0 * sin(i * 2 * M_PI / arr_size));
    ptr->f_dots = f_dots;
    ptr->arr_size = arr_size;
    ptr->volume = 0;
    memset((void*)ptr->dx, 0, sizeof(ptr->dx));
    memset((void*)ptr->curr, 0, sizeof(ptr->curr));
    ptr->duty_cycle = CCR;
    *ptr->duty_cycle = 0;
}

void make_tone(Tone_pin* tone_pin)
{
    if (tone_pin->volume != 0)
        for (uint8_t i = 0; i < (uint8_t)sizeof_arr(tone_pin->dx); ++i)
        {
            if (i == 0)
                *tone_pin->duty_cycle = (uint32_t)(tone_pin->f_dots[tone_pin->curr[i] >> 8]) * COUNTER_PERIOD / sine_ampl / sizeof_arr(tone_pin->dx);
            else
                *tone_pin->duty_cycle += (uint32_t)(tone_pin->f_dots[tone_pin->curr[i] >> 8]) * COUNTER_PERIOD / sine_ampl / sizeof_arr(tone_pin->dx);
            tone_pin->curr[i] += tone_pin->dx[i];
            if (tone_pin->curr[i] >= arr_size << 8)
            {
                tone_pin->curr[i] -= arr_size << 8;
            }
        }
    *tone_pin->duty_cycle = (*tone_pin->duty_cycle * tone_pin->volume) >> 16;
}

void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n)
{
    volatile uint32_t wait;///volatile need to shut up compiler warning about "infinit" loop below
    uint32_t start_tick = HAL_GetTick();
    for (int i = 0; i < n; ++i)
    {
        pin->dx[0] = freq_to_dx(pin, notes[i]);
        wait = 1000 / durations[i];
        while ((HAL_GetTick() - start_tick) < wait)
        {
        }
        start_tick += wait;
    }
    pin->dx[0] = 0;
}


void HearingTesterCtor(HearingTester* ptr)
{
    ptr->states = Idle;
    memset((void*)ptr->freq, 0, sizeof(ptr->freq));
    ptr->react_time = 0;
    ptr->react_time_size = 0;
    ptr->port=0;
    ptr->mseconds_to_max=2000;
    ptr->max_volume=6000;
}

void HearingStart(HearingTester* ptr)
{
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);///start sound at A10
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);///start sound at A9
    HAL_TIM_Base_Start_IT(&htim1);
    HAL_TIM_Base_Start_IT(&htim4);
}

void HearingStop(HearingTester* ptr)
{
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_3);///stop sound at A10
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);///stop sound at A9
    HAL_TIM_Base_Stop_IT(&htim1);
    HAL_TIM_Base_Stop_IT(&htim4);
}
