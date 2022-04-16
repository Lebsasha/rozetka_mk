#ifndef HEARING
#define HEARING

#include "notes.h"
#include "general.h"

#define TONE_FREQ 40000
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->ARR_SIZE*(freq)<<8)/TONE_FREQ) //TODO Change to function
//TODO Change channels number to 2 or higher
#define CHANNELS_NUM 1 //TODO Change to higher number

//#define TONE_PWM_GENERATION
#define TONE_SPI_GENERATION

typedef struct TonePin
{
    volatile uint32_t* PWM_register;
    Pin CS_pin;
    uint8_t channel_bit;
    int16_t* sine_table;
    uint16_t ARR_SIZE;/// const
    volatile uint16_t volume;
    volatile uint32_t dx[CHANNELS_NUM];
    uint32_t curr_phase[CHANNELS_NUM];
}TonePin;

void TonePin_ctor(TonePin* ptr, volatile uint32_t* CCR, uint8_t channel_bit, Pin CS_pin);

/// This is the function that handles changes of tone
void make_tone(TonePin* tonePin);

///Play some melody with notes and durations
/// @param durations - array of !positive! uint8's
void play(TonePin* ptr, const uint16_t* notes, const uint8_t* durations, int n);

/// First three state' values (0, 1, 2) used in high level program for observing measure process
typedef enum HearingStates{Starting=3, PlayingConstantVolume=0, ChangingVolume=4, WaitingBeforeMeasuringReaction=5, MeasuringReaction=1, Sending=2, Idle} HearingStates;

typedef struct HearingTester
{
    volatile uint8_t port;
    volatile typeof(static_access(TonePin)->volume) ampl;
    volatile uint16_t elapsed_time;
    volatile HearingStates states;
    Timer timer;
    volatile uint16_t react_time;
    volatile uint8_t react_surveys_elapsed;
    uint8_t REACT_SURVEYS_COUNT; //const
    uint8_t REACT_VOLUME_KOEF; //const
    uint8_t curr_react_volume_coef;
    volatile uint16_t freq[CHANNELS_NUM];
    volatile typeof(static_access(TonePin)->volume) curr_volume;
    volatile typeof(static_access(TonePin)->volume) new_volume;
}HearingTester;
void HearingTester_ctor(HearingTester* ptr);

void hearing_start(HearingTester* ptr);
void hearing_stop(HearingTester* ptr);
void hearing_handle(HearingTester* ptr);

#endif