#include "hearing.h"
#include <string.h>
#include <stdlib.h>

//TODO Write for both ports
//TODO Add release config
//TODO Sometime cleanup code
//TODO Split system and custom functions
//TODO ETR instead in clock
//TODO STM32CubeMX cleanup

extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern Button button;
extern Tone_pin* tone_pins;
const int16_t SINE_AMPL = (1U << (sizeof(SINE_AMPL) * 8 - 1)) - 1;//max val of int16_t  //TODO change to 0x7fff
const uint16_t ARR_SIZE = 1024; /*!<for optimization purposes*/
int16_t sine_table[1024];

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t* CCR, uint8_t channel_bit, Pin CS_pin)
{
    for (int i = 0; i < ARR_SIZE; ++i)
        sine_table[i] = (int16_t)(SINE_AMPL / 2.0 - SINE_AMPL / 2.0 * sin(i * 2 * M_PI / ARR_SIZE));
    ptr->sine_table = sine_table;
    ptr->ARR_SIZE = ARR_SIZE;
    ptr->volume = 0;
    memset((void*)ptr->dx, 0, sizeof(ptr->dx));
    memset((void*)ptr->curr_phase, 0, sizeof(ptr->curr_phase));
    ptr->PWM_register = CCR;
    *ptr->PWM_register = 0;
    ptr->channel_bit = channel_bit;
    ptr->CS_pin = CS_pin;
}

void make_tone(Tone_pin* tone_pin)
{
#if defined(TONE_PWM_GENERATION)
    if (tone_pin->volume != 0)
    {
        const uint16_t COUNTER_PERIOD = 72000000/TONE_FREQ;
        for (uint8_t i = 0; i < (uint8_t) sizeof_arr(tone_pin->dx); ++i)
        {
            if (i == 0)
                *tone_pin->PWM_register = (uint32_t) (tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tone_pin->dx);
            else
                *tone_pin->PWM_register += (uint32_t) (tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tone_pin->dx);
            tone_pin->curr_phase[i] += tone_pin->dx[i];
            if (tone_pin->curr_phase[i] >= ARR_SIZE << 8)
            {
                tone_pin->curr_phase[i] -= ARR_SIZE << 8;
            }
        }
    }
    *tone_pin->PWM_register = (*tone_pin->PWM_register * tone_pin->volume) >> 16;
#elif defined(TONE_SPI_GENERATION)
    if (tone_pin->volume != 0)
    {
        const uint16_t MAX_VAL = (1<<12)-1;
        uint16_t command;

        for (uint8_t i = 0; i < (uint8_t) sizeof_arr(tone_pin->dx); ++i)
        {
            if (i == 0)
                command = (uint32_t) (tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * MAX_VAL / SINE_AMPL / sizeof_arr(tone_pin->dx);
            else
                command += (uint32_t) (tone_pin->sine_table[tone_pin->curr_phase[i] >> 8]) * MAX_VAL / SINE_AMPL / sizeof_arr(tone_pin->dx);
            tone_pin->curr_phase[i] += tone_pin->dx[i];
            if (tone_pin->curr_phase[i] >= ARR_SIZE << 8)
            {
                tone_pin->curr_phase[i] -= ARR_SIZE << 8;
            }
        }
        command = (command * tone_pin->volume)>>16;
//       tone_pin->curr_phase[0]++;
//       if (tone_pin->curr_phase[0] >= 400000)
//            {
//                tone_pin->curr_phase[0] = 0;
//            }
//       command = tone_pin->curr_phase[0]*4096/400000;
//        command = 2048;
        command &= (1<<12)-1; /// If command > 4095, we reset all bits higher 12th
        command |= tone_pin->channel_bit<<15; /// Port (!A/B)
        command |= 0<<14; /// BUF
        command |= 1<<13; /// !GA
        command |= 1<<12; /// !SHDN
        HAL_GPIO_WritePin(tone_pin->CS_pin.GPIOx, tone_pin->CS_pin.pin, GPIO_PIN_RESET);
        HAL_SPI_Transmit(&hspi1, reinterpret_cast(uint8_t*, &command), sizeof(command)/sizeof(uint16_t), HAL_MAX_DELAY); // NOLINT(bugprone-sizeof-expression)
        // TODO ASK Maybe delay for at least 2 operations for Release config
        HAL_GPIO_WritePin(tone_pin->CS_pin.GPIOx, tone_pin->CS_pin.pin, GPIO_PIN_SET);
    }
#else
#error Not defined generation type for make_tone()
#endif
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


void HearingTester_ctor(HearingTester* ptr)
{
    ptr->states = Idle;
    ptr->algorithm = ConstantTone;
    memset((void*)ptr->freq, 0, sizeof(ptr->freq));
    ptr->react_time = 0;
    ptr->react_surveys_elapsed = 0;
    ptr->REACT_SURVEYS_COUNT = 3;
    ptr->REACT_VOLUME_KOEF = 4;
    ptr->port = 0;
    ptr->mseconds_to_max = 0;
    ptr->max_volume = 0;
    ptr->tone_step = 0;
}

void HearingStart(HearingTester* ptr)
{
    htim1.Instance->PSC = 0;
    htim1.Instance->ARR = 1799;
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
    if(ptr->states == Measuring_freq) //TODO Delay before measuring_freq
    {
        if (button.state == Pressed)
        {
            ptr->elapsed_time = button.stop_time - button.start_time/* - ptr->react_time*/;///- react_time located higher, in Measuring_reaction
            ptr->ampl = tone_pins[ptr->port].volume;
//            if(ptr->algorithm == LinearTone)
//            {
//                ptr->ampl = ptr->max_volume * ptr->elapsed_time / ptr->mseconds_to_max;///This is needed for calculating volume for measuring react_time
//            }
//            else if (ptr->algorithm == ConstantTone)
//                ptr->ampl = ptr->max_volume;
//            else if (ptr->algorithm == LinearStepTone)
//                ptr->ampl = ptr->max_volume * ptr->elapsed_time / ptr->mseconds_to_max;
            tone_pins[ptr->port].volume = 0;

            HAL_Delay(rand() % 1500 + 1000);
            ptr->states = Measuring_reaction;
            uint8_t curr_koef = ptr->REACT_VOLUME_KOEF;
            while (curr_koef * ptr->ampl <= ptr->ampl)
                curr_koef -= 1;
            tone_pins[ptr->port].volume = curr_koef * ptr->ampl;//TODO Change reaction volume (Remove TODO?)
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
        if (ptr->react_surveys_elapsed < ptr->REACT_SURVEYS_COUNT - 1)
        {
            uint16_t randDelay = rand() % 800 + 1000;
            HAL_Delay(randDelay);//TODO Replace with other function
            ++ptr->react_surveys_elapsed;
            tone_pins[ptr->port].volume = prev_volume;
            ButtonStart(&button);
        }
        else
        {
            ptr->react_time /= ptr->REACT_SURVEYS_COUNT;
            ButtonStop(&button);
            ptr->react_surveys_elapsed = 0;

            ptr->elapsed_time -= ptr->react_time;
            /// As we changed elapsed_time, we have to recalculate amplitude
            if (ptr->states == LinearTone)
                ptr->ampl = ptr->max_volume * ptr->elapsed_time / ptr->mseconds_to_max;
            else if (ptr->states == ConstantTone)
            {}
            else if (ptr->states == LinearStepTone)
            {
                uint16_t stepNum = ptr->elapsed_time / ptr->tone_step;
                if (ptr->elapsed_time + ptr->tone_step < ptr->mseconds_to_max)
                    stepNum += 1;
                ptr->ampl = ptr->max_volume * stepNum * ptr->tone_step / ptr->mseconds_to_max;
            }
            ptr->states = Sending;
        }
    }
    //else {;} // Nothing needs to be done, waiting until something happens
}
