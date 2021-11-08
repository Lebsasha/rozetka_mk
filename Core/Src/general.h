#ifndef GENERAL
#define GENERAL


#include <math.h>
#include <stdbool.h>
#include "main.h"

#ifndef DEBUG
#define DEBUG
//Inform that IDE does not define debug
#warning DEBUG defined manually
#endif

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))
#define static_access(type) ((type*)NULL)
#define reinterpret_cast(type, expression) ((type)(expression))

/// If start_time!=0 && stop_time==0 time is measured.
///     Then if stop_time!=0 time has been measured
/// Otherwise (i. e. start_time==0) measuring is off
typedef struct Button
{
    volatile uint32_t start_time;
    volatile uint32_t stop_time;
}Button;
void Button_ctor(Button* button);

typedef enum Measures
{
    None, Hearing, SkinConduction
}Measures;

void my_delay(int us);

#endif //GENERAL
