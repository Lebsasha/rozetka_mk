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
//TODO 1 make NDebug for release config
//TODO 2 Make soft changing of volume

extern SPI_HandleTypeDef hspi1;
extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim4;
extern Button button;
extern TonePin* tone_pins;
const int16_t SINE_AMPL = INT16_MAX;
const uint16_t ARR_SIZE = 1024;
const uint32_t SHIFTED_ARR_SIZE = ARR_SIZE << 8; /*!<for optimization purposes*/
int16_t sine_table[1024];

void TonePin_ctor(TonePin* ptr, HearingDynamics channel_bit)
{
    for (int i = 0; i < ARR_SIZE; ++i)
        sine_table[i] = (int16_t)(SINE_AMPL * sin(i * 2 * M_PI / ARR_SIZE));
    ptr->sine_table = sine_table;
    ptr->ARR_SIZE = ARR_SIZE;
    ptr->volume = 0;
    memset((void*)ptr->dx, 0, sizeof(ptr->dx));
    memset((void*)ptr->curr_phase, 0, sizeof(ptr->curr_phase));
    ptr->channel_bit = channel_bit;
}

//inline uint32_t freq_to_dx(TonePin *ptr, uint16_t frequency)
//{
//    return ptr->ARR_SIZE * frequency << 8 / TONE_FREQ;
//}

void make_tone(TonePin* tonePin)
{
    if (tonePin->volume != 0)
    {
// This constant need for accessing values for curr_phase and dx variables in Tone_pin struct
        const uint8_t i = 0;

        uint32_t curr_tone_value;
        const int32_t NEGATIVE_TO_ZERO_SHIFT = SINE_AMPL * HEARING_VOLUME_MAX;
        curr_tone_value = ((tonePin->sine_table[tonePin->curr_phase[i] >> 8] * tonePin->volume + NEGATIVE_TO_ZERO_SHIFT)/(2*SINE_AMPL));
//        curr_tone_value = ((tonePin->sine_table[tonePin->curr_phase[i]>>8] * (tonePin->volume>>4) + 0x7ff8000)>>16);

        tonePin->curr_phase[i] += tonePin->dx[i];
        if (tonePin->curr_phase[i] >= SHIFTED_ARR_SIZE)
        {
            tonePin->curr_phase[i] -= SHIFTED_ARR_SIZE;
        }

        uint16_t command = curr_tone_value;
        command &= (1<<12)-1; /// If command > 4095, we reset all bits higher 12th
        command |= tonePin->channel_bit<<15; /// DAC channel (!A/B)
        command |= 0<<14; /// BUF
        command |= 1<<13; /// !GA
        command |= 1<<12; /// !SHDN
        GPIOB->BSRR = GPIO_PIN_10<<16; // RESET B10 (!CS) pin
        HAL_SPI_Transmit(&hspi1, reinterpret_cast(uint8_t*, &command), sizeof(command)/sizeof(uint16_t), HAL_MAX_DELAY); // NOLINT(bugprone-sizeof-expression)
        // TODO Test Maybe delay for at least 2 operations for Release config
        GPIOB->BSRR = GPIO_PIN_10; // SET B10 (!CS) pin
    }
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
    ptr->is_results_on_curr_pass_captured = false;
    ptr->is_volume_need_to_be_changed = false;
    Timer t = {0};
    ptr->timer = t;
    memset((void*)ptr->react_times, 0, sizeof(ptr->react_times));
    ptr->react_surveys_elapsed = 0;
    ptr->amplitude_for_react_survey = 0;
    memset((void*)ptr->freq, 0, sizeof(ptr->freq));
    ptr->new_volume = 0;
}

void hearing_start(HearingTester* ptr)
{
    htim1.Instance->PSC = 0;
    const uint16_t COUNTER_RANGE = 72000000/TONE_FREQ;
    htim1.Instance->ARR = COUNTER_RANGE - 1;
    __HAL_SPI_ENABLE(&hspi1);
    HAL_TIM_Base_Start_IT(&htim1);
}

void hearing_stop(HearingTester* ptr)
{
    HAL_TIM_Base_Stop_IT(&htim1);
    __HAL_SPI_DISABLE(&hspi1);
    ptr->state = Idle;
    ButtonStop(&button);
}

void hearing_handle(HearingTester* const ptr)
{
    if(ptr->state == PlayingConstantVolume)
    {
        return; // Nothing need to be done in this state in this function
    }
    else if (ptr->state == StartingMeasuringReaction) /// This state need, as starting timer directly in process_cmd() function lead to freezing device, therefore timer need to be started from main() function
    {
        ptr->react_surveys_elapsed = 0;
        uint32_t randDelay = rand()%1500 + 1000;
        timer_start(&ptr->timer, randDelay);

        ptr->state = WaitingBeforeMeasuringReaction;
    }
    else if (ptr->state == WaitingBeforeMeasuringReaction)
    {
        if (!is_time_passed(&ptr->timer))
            return;
        timer_reset(&ptr->timer);
        set_new_tone_volume(ptr, ptr->amplitude_for_react_survey);
        ButtonStart(&button, WaitingForPress);
        ptr->state = MeasuringReaction;
    }
    else if (ptr->state == MeasuringReaction && button.state == Pressed)
    {
        ptr->react_times[ptr->react_surveys_elapsed] = button.stop_time - button.start_time;
        set_new_tone_volume(ptr, 0);
        if (ptr->react_surveys_elapsed < REACT_SURVEYS_COUNT - 1)
        {
            ++ptr->react_surveys_elapsed;
            uint16_t randDelay = rand() % 800 + 1000;
            timer_start(&ptr->timer, randDelay);
            ptr->state = WaitingBeforeMeasuringReaction;
        }
        else
        {
            ButtonStop(&button);
            ptr->state = Sending;
        }
    }
    //else {;} // Nothing needs to be done, waiting until something happens
}

/// We cannot write directly to @code tone_pins[hearingTester.dynamic].volume = volume @endcode as it will cause a click on the speaker
void set_new_tone_volume(HearingTester *ptr, uint16_t volume)
{
    ptr->new_volume = volume;
    ptr->is_volume_need_to_be_changed = true;
}
