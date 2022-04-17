#ifndef GENERAL
#define GENERAL

#include <math.h>
#include <stdbool.h>
#include "main.h"

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))
/// This macro is used for performing some kind of variable type deduction
#define static_access(type) ((type*)NULL)

#define reinterpret_cast(type, expression) ((type)(expression))

typedef enum Measures
{
    None, Hearing, SkinConduction
}Measures;

/// For simplifying pin transmitting via constructors in objects
typedef struct Pin {GPIO_TypeDef* GPIOx; uint16_t pin;} Pin;

/// If start_time!=0 && stop_time==0 time is measured.
///     Then if stop_time!=0 time has been measured
/// Otherwise (i. e. start_time==0) measuring is off
typedef struct Button
{
    volatile uint32_t start_time;
    volatile uint32_t stop_time;
    volatile enum ButtonStates {WaitingForPress, Pressed, Timeout, ButtonIdle} state;
    GPIO_TypeDef* GPIOx;
    uint16_t pin;
}Button;
void Button_ctor(Button* button, Pin pin);
void ButtonStart(Button* button);
void ButtonStop(Button* button);

typedef struct Timer
{
    uint32_t endTick;
}Timer;
void timer_start(Timer* ptr, uint32_t delay);
bool is_time_passed(Timer* ptr);
void timer_reset(Timer* ptr);

typedef struct RandInitializer
{
    bool isInitialised;
}RandInitializer;

void RandInitializer_ctor(RandInitializer* randInitializer);
void InitRand(RandInitializer* randInitializer);

void write_pin_if_in_debug(GPIO_TypeDef* GPIOx, uint16_t pin, GPIO_PinState pinState);

void my_delay(int us);

#endif //GENERAL
