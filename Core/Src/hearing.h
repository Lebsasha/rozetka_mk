#ifndef HEARING
#define HEARING

#include "notes.h"
#include "general.h"

#define TONE_FREQ 40000
#define COUNTER_PERIOD 1800
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->arr_size*(freq)<<8)/TONE_FREQ)
#define CHANNELS_NUM 1

typedef struct Tone_pin
{
    volatile uint32_t* duty_cycle;
    int16_t* f_dots;
    uint16_t arr_size;///TODO Maybe inline? Yes, i can define this and change freq_dx and f_dots[arr_size]
    uint16_t volume;
    volatile uint32_t dx[CHANNELS_NUM];
    uint32_t curr[CHANNELS_NUM];
}Tone_pin;

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t*);

/// This is the function that handles changes of tone
void make_tone(Tone_pin* tone_pin);

///Play some melody with notes and durations
/// @param durations - array of !positive! uint8's
void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n);

typedef enum HearingStates{Measuring_freq=0, Measuring_reaction=1, Sending=2, Idle}HearingStates;

typedef struct HearingTester
{
    volatile HearingStates states;
    volatile uint16_t freq[CHANNELS_NUM];
    volatile uint16_t react_time;
    volatile uint8_t react_time_size;
    volatile uint8_t port;
    volatile typeof(static_access(Tone_pin)->volume) ampl;
    volatile uint16_t elapsed_time;
    volatile uint16_t mseconds_to_max;
    volatile typeof(static_access(Tone_pin)->volume) max_volume;
}HearingTester;
void HearingTesterCtor(HearingTester* ptr);

void HearingStart(HearingTester* ptr);
void HearingStop(HearingTester* ptr);

#endif