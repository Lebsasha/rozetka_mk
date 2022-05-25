#ifndef HEARING
#define HEARING

#include "notes.h"
#include "general.h"

/// Carrying frequency
#define TONE_FREQ 60000
#define DAC_MAX_VALUE ((1<<12) - 1)
#define HEARING_VOLUME_MAX  DAC_MAX_VALUE
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->ARR_SIZE*(freq)<<8)/TONE_FREQ)
/// Defines number of tones, which can be played simultaneously on dynamic
#define CHANNELS_NUM 1

#define REACT_SURVEYS_COUNT 3

typedef enum HearingStates
{
    PlayingConstantVolume, StartingMeasuringReaction, WaitingBeforeMeasuringReaction, MeasuringReaction, Sending, Idle
} HearingStates;

/// Possible hearing test states from high level program (i. e. from PC) point of view
typedef enum HearingStatesOnHighLevel
{
    MeasuringHearingThreshold = 0, /*SendingThresholdResult=1, */MeasuringReactionTime = 1, SendingResults = 2
} HearingStatesOnHighLevel;

/// Left and right dynamics mapping to channel bit on DAC
/// @warning this enum only hints what dynamic mapped to what pin, but for determining which dynamic mapped to which physical pin could be done in main() function near HearingTester's constructor
typedef enum HearingDynamics{LeftDynamic=0, RightDynamic=1} HearingDynamics;

typedef struct HearingTester
{
    volatile HearingDynamics dynamic;
    volatile uint16_t ampl;
    volatile uint16_t elapsed_time;
    volatile HearingStates state;
    bool is_results_on_curr_pass_captured;
    volatile bool is_volume_need_to_be_changed;
    Timer timer;
    volatile uint16_t react_times[REACT_SURVEYS_COUNT];
    volatile uint8_t react_surveys_elapsed;
    volatile uint16_t amplitude_for_react_survey; //!< Tone volume for estimating reaction time
    volatile uint16_t freq[CHANNELS_NUM];
    volatile uint16_t new_volume;
}HearingTester;
void HearingTester_ctor(HearingTester* ptr);

void hearing_start(HearingTester* ptr);
void hearing_stop(HearingTester* ptr);
void hearing_handle(HearingTester* ptr);
void set_new_tone_volume(HearingTester *ptr, uint16_t volume);



typedef struct TonePin
{
    HearingDynamics channel_bit;
    int16_t* sine_table;
    uint16_t ARR_SIZE;
    volatile uint16_t volume;
    volatile uint32_t dx[CHANNELS_NUM];
    uint32_t curr_phase[CHANNELS_NUM];
}TonePin;

void TonePin_ctor(TonePin* ptr, HearingDynamics channel_bit);

//uint32_t freq_to_dx(TonePin* ptr, uint16_t frequency);

/// This is the function that generates sine wave of given frequency (or frequencies)
void make_tone(TonePin* tonePin);

///Play some melody with notes and durations
/// @param durations - must be array of positive uints
void play(TonePin* ptr, const uint16_t* notes, const uint8_t* durations, int n);

#endif
