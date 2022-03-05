#ifndef HEARING
#define HEARING

#include "notes.h"
#include "general.h"

#define TONE_FREQ 40000
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->arr_size*(freq)<<8)/TONE_FREQ) //TODO Change to function
//TODO Change channels number to 2 or higher
#define CHANNELS_NUM 1 //TODO Change to higher number

//#define TONE_PWM_GENERATION
#define TONE_SPI_GENERATION

typedef struct Tone_pin
{
    volatile uint32_t* PWM_register;
    Pin CS_pin;
    uint8_t channel_bit;
    int16_t* sine_table;
    uint16_t arr_size;/// const TODO Remove this comment Maybe inline? No, as after inlining this will convert to a magic number
    uint16_t volume;
    volatile uint32_t dx[CHANNELS_NUM];
    uint32_t curr_phase[CHANNELS_NUM];
}Tone_pin;

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t*, uint8_t, Pin);

/// This is the function that handles changes of tone
void make_tone(Tone_pin* tone_pin);

///Play some melody with notes and durations
/// @param durations - array of !positive! uint8's
void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n);

typedef enum HearingStates{Measuring_freq=0, Measuring_reaction=1, Sending=2, Idle}HearingStates;

typedef struct HearingTester
{
    volatile HearingStates states;
    enum Algorithms{LinearTone, ConstantTone, LinearStepTone} algorithm;
    volatile uint16_t freq[CHANNELS_NUM];
    volatile uint16_t react_time;
    volatile uint8_t react_surveys_elapsed;
    uint8_t react_surveys_count; //const
    uint8_t react_volume_koef; //const
    volatile uint8_t port;
    volatile typeof(static_access(Tone_pin)->volume) ampl;
    volatile uint16_t elapsed_time;
    volatile uint16_t mseconds_to_max;
    volatile typeof(static_access(Tone_pin)->volume) max_volume;
    volatile uint16_t tone_step;
}HearingTester;
void HearingTesterCtor(HearingTester* ptr);

void HearingStart(HearingTester* ptr);
void HearingStop(HearingTester* ptr);
void HearingHandle(HearingTester* ptr);

#endif