#ifndef MAIN_TARGET
#define MAIN_TARGET

#define TONE_FREQ 40000
#define COUNTER_PERIOD 1800
#define freq_to_dx(tone_pin_ptr, freq)  (((tone_pin_ptr)->arr_size*(freq)<<8)/TONE_FREQ)
#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))

#include "notes.h"
//#include <assert.h>

typedef struct Tone_pin
{
    volatile uint32_t* duty_cycle;
    int16_t* f_dots;
    uint16_t arr_size;///TODO Maybe inline?
//    int16_t sine_ampl;///TODO Maybe inline?
    uint8_t volume;
    volatile uint32_t dx[5];
    uint32_t curr[5];
}Tone_pin;

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t*);

/// This is the function that handles changes of tone
void make_tone(Tone_pin* tone_pin);

///Play some melody with notes and durations
/// @param durations - array of !positive! uint8's
void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n);


/// If start_time!=0 && stop_time==0 time is measured.
///     Then if stop_time!=0 time is sended
/// Otherwise (start_time==0) measuring is off
typedef struct Button
{
    volatile uint32_t start_time;
    volatile uint32_t stop_time;
}Button;

///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum Commands
{
    CC = 0, LenL = 1, LenH = 2
};

static const uint8_t SS_OFFSET = 42;///TODO Write documentation

typedef struct Command_writer
{
    uint8_t buffer[128];
    size_t length;
    size_t BUF_SIZE;
} Command_writer;

void Command_writer_ctor(Command_writer* ptr);

void send_command(Command_writer* ptr);


typedef enum States{Measuring_reaction, Measiring_freq, Sending, Idle}States;
typedef struct Tester
{
    volatile States states;
    volatile uint16_t ampl;
    volatile uint16_t react_time;
    volatile uint8_t react_time_size;
    Button button;
    volatile uint8_t port;
    volatile uint16_t freq[sizeof_arr(((Tone_pin*)(NULL))->dx)];
}Tester;
void Tester_ctor(Tester* ptr);


void process_cmd(const uint8_t* command, const uint32_t* len);

void my_delay(int mc_s);
#endif //MAIN_TARGET
