#ifndef HEARING
#define HEARING

#include "notes.h"
#include "general.h"

/// Carrying frequency
#define TONE_FREQ 40000
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->ARR_SIZE*(freq)<<8)/TONE_FREQ)
//TODO Try to change number of channels to 2 or higher
#define CHANNELS_NUM 1

//#define TONE_PWM_GENERATION
#define TONE_SPI_GENERATION

/// Left and right dynamics mapping to channel bit on DAC
/// @warning this enum only states what dynamic mapped to what pin, but determining which dynamic use from HearingTester's
typedef enum HearingDynamics{LeftDynamic=0, RightDynamic=1} HearingDynamics;

typedef struct TonePin
{
    volatile uint32_t* PWM_register;
    Pin CS_pin;
    HearingDynamics channel_bit;
    int16_t* sine_table;
    uint16_t ARR_SIZE;
    volatile uint16_t volume;
    volatile uint32_t dx[CHANNELS_NUM];
    uint32_t curr_phase[CHANNELS_NUM];
}TonePin;

void TonePin_ctor(TonePin* ptr, volatile uint32_t* CCR, HearingDynamics channel_bit, Pin CS_pin);

//uint32_t freq_to_dx(TonePin* ptr, uint16_t frequency);

/// This is the function that generates sine wave of given frequency (or frequencies)
void make_tone(TonePin* tonePin);

///Play some melody with notes and durations
/// @param durations - must be array of positive uints
void play(TonePin* ptr, const uint16_t* notes, const uint8_t* durations, int n);


typedef enum HearingStates
{
    Starting, PlayingConstantVolume, ChangingVolume, WaitingBeforeMeasuringReaction, MeasuringReaction, Sending, Idle
} HearingStates;

/// Possible hearing test states from high level program (i. e. from PC) point of view
typedef enum HearingStatesOnHighLevel
{
    MeasuringHearingThreshold = 0, MeasuringReactionTime = 1, SendingResults = 2
} HearingStatesOnHighLevel;

typedef struct HearingTester
{
    volatile HearingDynamics dynamic;
    volatile uint16_t ampl;
    volatile uint16_t elapsed_time;
    volatile HearingStates state;
    Timer timer;
    volatile uint16_t react_time;
    volatile uint8_t react_surveys_elapsed;
    uint8_t REACT_SURVEYS_COUNT;
    uint8_t REACT_VOLUME_KOEF;
    uint8_t curr_react_volume_coef;
    volatile uint16_t freq[CHANNELS_NUM];
    volatile uint16_t curr_volume;
    volatile uint16_t new_volume;
    volatile uint8_t VOLUME_CHANGER_PRESCALER;
}HearingTester;
void HearingTester_ctor(HearingTester* ptr);

void hearing_start(HearingTester* ptr);
void hearing_stop(HearingTester* ptr);
void hearing_handle(HearingTester* ptr);

#endif