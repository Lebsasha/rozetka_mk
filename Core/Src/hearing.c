#include "hearing.h"
#include <string.h>
#include <stdlib.h>

//TODO Write for both ports
//TODO Add release config
//TODO Sometime cleanup code
//TODO Split system and custom functions
//TODO ETR instead in clock

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern Button button;
extern Tone_pin* tone_pins;
const int16_t SINE_AMPL = (1U << (sizeof(SINE_AMPL) * 8 - 1)) - 1;//max val of int16_t
const uint16_t ARR_SIZE = 1024; /*!<for optimization purposes*/
int16_t sine_table[1024];

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t* CCR)
{
    for (int i = 0; i < ARR_SIZE; ++i)
        sine_table[i] = (int16_t)(SINE_AMPL / 2.0 - SINE_AMPL / 2.0 * sin(i * 2 * M_PI / ARR_SIZE));
    ptr->sine_table = sine_table;
    ptr->arr_size = ARR_SIZE;
    ptr->volume = 0;
    memset((void*)ptr->dx, 0, sizeof(ptr->dx));
    memset((void*)ptr->curr_phase, 0, sizeof(ptr->curr_phase));
    ptr->PWM_register = CCR;
    *ptr->PWM_register = 0;
}

void make_tone(Tone_pin* tone_pin)
{
    if (tone_pin->volume != 0)
        for (uint8_t i = 0; i < (uint8_t)sizeof_arr(tone_pin->dx); ++i)
        {
            if (i == 0)
                *tone_pin->PWM_register = (uint32_t)(tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tone_pin->dx);
            else
                *tone_pin->PWM_register += (uint32_t)(tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tone_pin->dx);
            tone_pin->curr_phase[i] += tone_pin->dx[i];
            if (tone_pin->curr_phase[i] >= ARR_SIZE << 8)
            {
                tone_pin->curr_phase[i] -= ARR_SIZE << 8;
            }
        }
    *tone_pin->PWM_register = (*tone_pin->PWM_register * tone_pin->volume) >> 16;
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
    ptr->react_surveys_elapsed = 0;
    ptr->react_surveys_count = 3;
    ptr->react_volume_koef = 4;
    ptr->port=0;
    ptr->mseconds_to_max=2000;
    ptr->max_volume=6000;
}

void HearingStart(HearingTester* ptr)
{
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);///start sound at A10
    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);///start sound at A9
    HAL_TIM_Base_Start_IT(&htim1);
    ptr->states = Measuring_freq;
    ButtonStart(&button);
}

void HearingStop(HearingTester* ptr)
{
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_3);///stop sound at A10
    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);///stop sound at A9
    HAL_TIM_Base_Stop_IT(&htim1);
    ptr->states = Idle;//TODO Remove this
    ptr->react_time = 0;
    ButtonStop(&button);
}

void HearingHandle(HearingTester* ptr)
{
    static uint16_t prev_volume;
    if(ptr->states == Measuring_freq)
    {
        if (button.state == Pressed)
        {
            ptr->elapsed_time = button.stop_time - button.start_time/* - ptr->react_time*/;///- react_time located higher, in Measuring_reaction
            ptr->ampl = ptr->max_volume * ptr->elapsed_time / ptr->mseconds_to_max;///This is needed for calculating volume for measuring react_time
            tone_pins[ptr->port].volume = 0;

            HAL_Delay(rand() % 1500 + 1000);
            ptr->states = Measuring_reaction;
            tone_pins[ptr->port].volume = ptr->react_volume_koef * ptr->ampl;
            ButtonStart(&button);
        }
        else if (button.state == Timeout) ///case elapsed time for sound testing exceed ptr->mseconds_to_max
        {
            ptr->elapsed_time = 0;
            ptr->ampl = 0;
            tone_pins[ptr->port].volume = 0;
            ptr->states = Sending;
            ButtonStop(&button);
        }
        else if (button.state == WaitingForPress)
            return;
        //else {;} /// If we come in this place then button is in state "ButtonIdle" and we need raise error (but error handling of this type is currently not supported)
    }
    else if (ptr->states == Measuring_reaction && button.state == Pressed)
    {
        ptr->react_time += button.stop_time - button.start_time;
        prev_volume = tone_pins[ptr->port].volume;
        tone_pins[ptr->port].volume = 0;
        if (ptr->react_surveys_elapsed < ptr->react_surveys_count - 1)
        {
            uint16_t rand_delay = rand() % 800 + 1000;
            HAL_Delay(rand_delay);//TODO Replace with other function
            ++ptr->react_surveys_elapsed;
            tone_pins[ptr->port].volume = prev_volume;
            ButtonStart(&button);
        }
        else
        {
            ptr->react_time /= ptr->react_surveys_count;
            ButtonStop(&button);
            ptr->react_surveys_elapsed = 0;

            ptr->elapsed_time -= ptr->react_time;
            ptr->ampl = ptr->max_volume * ptr->elapsed_time / ptr->mseconds_to_max;
            ptr->states = Sending;
        }
    }
    //else {;} // Nothing needs to be done, waiting until something happens
}
