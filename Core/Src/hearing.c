#include "hearing.h"
#include <string.h>
#include <stdlib.h>

//TODO 3 Make possibility of testing hearing on both dynamics
//TODO 3(2) Test release config
//TODO ? ETR instead in clock (?)
//TODO 2 STM32CubeMX cleanup
//TODO 1 Change type of sine_table
//TODO 5 Replace strings with error codes, e. g.: UnsupportedCommand, CurrentMeasureTypeIsNone, RequiredCommandParameterMissing, CommandParameterOutOfRange, OtherError
//TODO 2 Set last amplitude on DAC to 0
//TODO 1 Try disabling SPI between tests
//TODO 3 Make possible that reaction won't be measured
//TODO 1 Remove len ptr in process_command
//TODO Pin A6 in ScinConductivity measure don't work

//TODO 2(1) Hаndling reaction_time >= elapsed_time in C++ high level program
//TODO 1 Make possible decreasing curr_volume to new_volume, not only increasing

extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern Button button;
extern TonePin* tone_pins;
const int16_t SINE_AMPL = 0x7fff;//max val of int16_t
const uint16_t ARR_SIZE = 1024; /*!<for optimization purposes*/
int16_t sine_table[1024];

void TonePin_ctor(TonePin* ptr, volatile uint32_t* CCR, HearingDynamics channel_bit, Pin CS_pin)
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

//inline uint32_t freq_to_dx(TonePin *ptr, uint16_t frequency)
//{
//    return ptr->ARR_SIZE * frequency << 8 / TONE_FREQ;
//}

void make_tone(TonePin* tonePin)
{
#if defined(TONE_PWM_GENERATION)
    if (tonePin->volume != 0)
    {
        const uint16_t COUNTER_PERIOD = 72000000/TONE_FREQ;
        for (uint8_t i = 0; i < (uint8_t) sizeof_arr(tonePin->dx); ++i)
        {
            if (i == 0)
                *tonePin->PWM_register = (uint32_t) (tonePin->sine_table[tonePin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tonePin->dx);
            else
                *tonePin->PWM_register += (uint32_t) (tonePin->sine_table[tonePin->curr_phase[i] >> 8]) * COUNTER_PERIOD / SINE_AMPL / sizeof_arr(tonePin->dx);
            tonePin->curr_phase[i] += tonePin->dx[i];
            if (tonePin->curr_phase[i] >= ARR_SIZE << 8)
            {
                tonePin->curr_phase[i] -= ARR_SIZE << 8;
            }
        }
    }
    *tonePin->PWM_register = (*tonePin->PWM_register * tonePin->volume) >> 16;
#elif defined(TONE_SPI_GENERATION)
    if (tonePin->volume != 0)
    {
        const uint16_t MAX_VAL = (1<<12)-1;
        uint16_t command;

        for (uint8_t i = 0; i < (uint8_t) sizeof_arr(tonePin->dx); ++i)
        {
            if (i == 0)
                command = (uint32_t) (tonePin->sine_table[tonePin->curr_phase[i] >> 8]) * MAX_VAL / SINE_AMPL / sizeof_arr(tonePin->dx);
            else
                command += (uint32_t) (tonePin->sine_table[tonePin->curr_phase[i] >> 8]) * MAX_VAL / SINE_AMPL / sizeof_arr(tonePin->dx);
            tonePin->curr_phase[i] += tonePin->dx[i];
            if (tonePin->curr_phase[i] >= ARR_SIZE << 8)
            {
                tonePin->curr_phase[i] -= ARR_SIZE << 8;
            }
        }
        command = (command * tonePin->volume)>>16;

        command &= (1<<12)-1; /// If command > 4095, we reset all bits higher 12th
        command |= tonePin->channel_bit<<15; /// DAC channel (!A/B)
        command |= 0<<14; /// BUF
        command |= 1<<13; /// !GA
        command |= 1<<12; /// !SHDN
        HAL_GPIO_WritePin(tonePin->CS_pin.GPIOx, tonePin->CS_pin.pin, GPIO_PIN_RESET);
        HAL_SPI_Transmit(&hspi1, reinterpret_cast(uint8_t*, &command), sizeof(command)/sizeof(uint16_t), HAL_MAX_DELAY); // NOLINT(bugprone-sizeof-expression)
        // TODO Test Maybe delay for at least 2 operations for Release config
        HAL_GPIO_WritePin(tonePin->CS_pin.GPIOx, tonePin->CS_pin.pin, GPIO_PIN_SET);
    }
#else
#error Not defined generation type for make_tone()
#endif
}

void play(TonePin* ptr, const uint16_t* notes, const uint8_t* durations, int n)
{
    volatile uint32_t wait;///volatile need to shut up compiler warning about "infinit" loop below
    for (int i = 0; i < n; ++i)
    {
        if (durations[i] <= 0)
            return;
    }
    uint32_t start_tick = HAL_GetTick();
    for (int i = 0; i < n; ++i)
    {
        ptr->dx[0] = freq_to_dx(ptr, notes[i]);
        wait = 1000 / durations[i];
        while ((HAL_GetTick() - start_tick) < wait)
        {
        }
        start_tick += wait;
    }
    ptr->dx[0] = 0;
}


void HearingTester_ctor(HearingTester* ptr)
{
    ptr->dynamic = LeftDynamic;
    ptr->ampl = 0;
    ptr->elapsed_time = 0;
    ptr->state = Idle;
    Timer t = {0};
    ptr->timer = t;
    ptr->react_time = 0;
    ptr->react_surveys_elapsed = 0;
    ptr->REACT_SURVEYS_COUNT = 3;
    ptr->REACT_VOLUME_KOEF = 4;
    memset((void*)ptr->freq, 0, sizeof(ptr->freq));
    ptr->curr_volume = 0;
    ptr->new_volume = 0;
    ptr->VOLUME_CHANGER_PRESCALER = 2;
}

void hearing_start(HearingTester* ptr)
{
    htim1.Instance->PSC = 0;
    htim1.Instance->ARR = 1799;
//    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);///start sound at A10
//    HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_2);///start sound at A9
    HAL_TIM_Base_Start_IT(&htim1);
    __HAL_SPI_ENABLE(&hspi1);
    ptr->state = Starting;
}

void hearing_stop(HearingTester* ptr)
{
//    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_3);///stop sound at A10
//    HAL_TIM_PWM_Stop(&htim1,TIM_CHANNEL_2);///stop sound at A9
    __HAL_SPI_DISABLE(&hspi1);
    HAL_TIM_Base_Stop_IT(&htim1);
    ptr->state = Idle;
    ButtonStop(&button);
}

void hearing_handle(HearingTester* ptr)
{
    if(ptr->state == PlayingConstantVolume) //TODO Maybe make delay before measuring hearing threshold
    {
        if (button.state == Pressed)
        {
            ptr->elapsed_time = button.stop_time - button.start_time/* - ptr->react_time*/;/// "- react_time" located in MeasuringReaction state
            ptr->ampl = tone_pins[ptr->dynamic].volume;
            tone_pins[ptr->dynamic].volume = 0;
            ptr->react_time = 0;
            ptr->curr_react_volume_coef = ptr->REACT_VOLUME_KOEF;// For preventing amplitude overflow
            while (ptr->curr_react_volume_coef * ptr->ampl <= ptr->ampl)
                ptr->curr_react_volume_coef -= 1;
            uint32_t randDelay = rand()%1500 + 1000;
            timer_start(&ptr->timer, randDelay);
            ptr->state = WaitingBeforeMeasuringReaction;
        }
        else if (button.state == Timeout) ///case elapsed time for sound testing exceed ptr->milliseconds_to_max
        {
            ptr->elapsed_time = 0;
            ptr->ampl = 0;
            tone_pins[ptr->dynamic].volume = 0;
            ButtonStop(&button);
            ptr->state = Sending;
        }
        else if (button.state == WaitingForPress)
            return;
//        else {;} /// If we come in this place then button is likely in state "ButtonIdle" and we need raise error (but error handling of this type is currently not supported)
    }
    else if (ptr->state == WaitingBeforeMeasuringReaction)
    {
        if (!is_time_passed(&ptr->timer))
            return;
        timer_reset(&ptr->timer);
        tone_pins[ptr->dynamic].volume = ptr->curr_react_volume_coef * ptr->ampl;
        ButtonStart(&button);
        ptr->state = MeasuringReaction;
    }
    else if (ptr->state == MeasuringReaction && button.state == Pressed)
    {
        ptr->react_time += button.stop_time - button.start_time;
        tone_pins[ptr->dynamic].volume = 0;
        if (ptr->react_surveys_elapsed < ptr->REACT_SURVEYS_COUNT - 1)
        {
            ++ptr->react_surveys_elapsed;
            uint16_t randDelay = rand() % 800 + 1000;
            timer_start(&ptr->timer, randDelay);
            ptr->state = WaitingBeforeMeasuringReaction;
        }
        else
        {
            ptr->react_surveys_elapsed = 0;
            ptr->react_time /= ptr->REACT_SURVEYS_COUNT;
            ButtonStop(&button);

            ptr->state = Sending;
        }
    }
    //else {;} // Nothing needs to be done, waiting until something happens
}
