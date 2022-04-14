#ifndef GENERAL
#define GENERAL


#include <math.h>
#include <stdbool.h>
#include "main.h"

//#ifndef DEBUG
//#define DEBUG
// /// Inform that IDE does not define "DEBUG"
//#warning DEBUG defined manually
//#endif

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))
#define static_access(type) ((type*)NULL)
#define reinterpret_cast(type, expression) ((type)(expression))

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
void Button_ctor(Button* button, GPIO_TypeDef* GPIOx, uint16_t pin);
void ButtonStart(Button* button);
void ButtonStop(Button* button);

typedef enum Measures
{
    None, Hearing, SkinConduction
}Measures;

typedef struct RandInitializer
{
    bool isInitialised;
}RandInitializer;

void RandInitializer_ctor(RandInitializer* randInitializer);
void InitRand(RandInitializer* randInitializer);
void write_pin_if_in_debug(GPIO_TypeDef* GPIOx, uint16_t pin, GPIO_PinState pinState);

void my_delay(int us);

#endif //GENERAL
