
#include <cassert>
#include <algorithm>
#include <fstream>
using namespace std;
///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum
{
    CC = 0, LenL = 1, LenH = 2
};

static const uint8_t SS_OFFSET = 42;

class CommandWriter
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    CommandWriter() : length(LenH + 1)
    {
        buffer[CC] = 0x10;///TODO enum
    }

    template<typename T>
    void append_var(T var)
    {
        assert(length + sizeof(var) < BUF_SIZE);
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
        buffer[LenL] += sizeof(var);///TODO!
    }

    void write(ostream& dev)
    {
        uint8_t sum = SS_OFFSET;
        for_each(buffer + CC + 1, buffer + length, [&sum](char c)
        { sum += c; });
        buffer[length] = sum;
        length += sizeof(sum);
        dev.write(buffer, length);
        dev.flush();
    }
};

class CommandReader
{
    static const size_t BUF_SIZE = 1024;
    char buffer[BUF_SIZE] = {0};
    size_t length;
    size_t read_lehgth;
public:
    CommandReader() : length(0), read_lehgth(0)
    {}

    char* read(istream& dev)
    {
        dev.read(buffer, length = LenH + 1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint8_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint8_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        dev.read(buffer + length, sizeof (uint8_t)); /// SS
        length += sizeof (uint8_t);
        uint8_t sum = SS_OFFSET;
        for_each(buffer + LenL, buffer + length - 1, [&sum](char c)
        { sum += c; });
        if (sum /*+ SS_OFFSET*/ != (uint8_t) buffer[length - 1])///(weak TODO) Why?
        {
            assert(false);
        }
        read_lehgth=LenH+1;
        return buffer;
    }

    template<typename T>
    T get_param(T& param)
    {
        assert(length!=0);
        param = *reinterpret_cast<T*>(buffer+read_lehgth);
        read_lehgth+=sizeof(T);
        return param;
    }

    uint8_t get_command(uint8_t& cmd)
    {
        return cmd=buffer[CC];
    }
};
extern "C" {
#include "main.h"
#include "main_target.h"
#include <string.h>
#include <stdlib.h>

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern struct LED leds[3];
extern volatile uint32_t count;
extern volatile uint32_t time;
extern volatile int measure_one_sine;
extern Button button;
//extern const int16_t sine_ampl;//TODO Remove some global vars
//extern const uint16_t arr_size;//TODO Sometime cleanup code
//extern int16_t f_dots[];//TODO Measure 0 and 1
//volatile uint32_t dx=(1024*500<<8)/TONE_FREQ;//TODO Move target.h -> Inc
//uint32_t curr=0; // TIM_TRGO_UPDATE; TODO View @ref in docs

int str_cmp(const uint8_t*, const char*);

void toggle_led(const uint8_t* command, size_t i);

void always_glow(struct LED*);

void always_zero(struct LED*);

void calc_up(struct LED*);

void calc_middle(struct LED*);

void calc_down(struct LED*);

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)//TODO Add elses
    {
        if (str_cmp(command, "measure") == 0)
        {
            button.start_time = HAL_GetTick();
        }
        if (str_cmp(command, "first") == 0)
        {
            toggle_led(command + sizeof("first ") - 1, 0);
        }
        if (str_cmp(command, "second") == 0)
        {
            toggle_led(command + sizeof("second ") - 1, 1);
        }
        if (str_cmp(command, "third") == 0)
        {
            toggle_led(command + sizeof("third ") - 1, 2);
        }
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        if (command[0] == '1')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void toggle_led(const uint8_t* command, const size_t i)
{
    if (str_cmp(command, "on") == 0)
    {
        if (*(command + sizeof("on") - 1) == 'f')//TODO Add braces
        {
            uint32_t freq = strtol((const char*) command + sizeof("onf ") - 1, NULL, 10);
            if (freq > 0 && freq < TONE_FREQ)
                (leds + i)->detailyty = TONE_FREQ / freq;
        }
        //(leds + i)->curr_step = always_glow; //TODO Uncomment with else
        if (*(command + (sizeof("on") - 1)) == 'm')/// music is on
        {
            if (*(command + (sizeof("onm") - 1)) == '0')
            {}
//                dx=(1024*500<<8)/TONE_FREQ;
        }
    }
    if (str_cmp(command, "off") == 0)
    {
        (leds + i)->curr_step = always_zero;
    }
    if (str_cmp(command, "resume") == 0)
    {
        (leds + i)->curr_step = calc_up;
    }
}

void ctor_LED(struct LED* led, uint16_t detailyty, volatile uint32_t* duty_cycle, char num)
{
    led->duty_cycle = duty_cycle;
    led->detailyty = detailyty;
    led->i = 0;
    led->num = num;
    led->curr_step = calc_up;
}

void always_glow(struct LED* led)
{
    *led->duty_cycle = 0;
}

void always_zero(struct LED* led)
{
    *led->duty_cycle = COUNTER_PERIOD;
}

void calc_up(struct LED* led)
{
//    *led->duty_cycle = (uint32_t)(f_dots[curr>>8])*COUNTER_PERIOD/sine_ampl;
//    curr+=dx;
//    if (curr >= arr_size<<8)
//    {
//       curr-=arr_size<<8;
//    }
}

void calc_middle(struct LED* led)
{
    ++led->i;
    *led->duty_cycle = 0;
    if (led->i == led->detailyty && led->curr_step == calc_middle)
        led->curr_step = calc_down;
}

void calc_down(struct LED* led)
{
    --led->i;
//    if (led->i > 2)
    *led->duty_cycle =
            COUNTER_PERIOD / led->detailyty * led->i;//COUNTER_PERIOD - (COUNTER_PERIOD * (sin((double) (led->i) / led->detailyty *
    // M_PI - M_PI_2) + 1) / 2);
//    else
//        *led->duty_cycle = COUNTER_PERIOD - 0;
    if (led->i == 0 && led->curr_step == calc_down)
        led->curr_step = calc_up;
}

void my_delay(int mc_s)
{
    if (mc_s <= 0)
        return;
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < mc_s)
    {}
}

void make_tone(Tone_pin* tone_pin)
{///This can be optimised
///TODO Why silence comes when dx==0?
    *tone_pin->duty_cycle = (uint32_t) (tone_pin->f_dots[tone_pin->curr >> 8]) * COUNTER_PERIOD / tone_pin->sine_ampl;
    tone_pin->curr += tone_pin->dx;
    if (tone_pin->curr >= tone_pin->arr_size << 8)
    {
        tone_pin->curr -= tone_pin->arr_size << 8;
    }

}

void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n)
{
    volatile uint32_t wait;///TODO remove volatile
    uint32_t start_tick = HAL_GetTick();
    for (int i = 0; i < n; ++i)
    {
        pin->dx = (pin->arr_size << 8) * notes[i] / TONE_FREQ;
        wait = 1000 / durations[i];
        while ((HAL_GetTick() - start_tick) < wait)
        {
        }
        start_tick += wait;
    }
    pin->dx = 0;
}

int str_cmp(const uint8_t* command, const char* str)
{
    size_t n = strlen(str) + 1;
    while (--n)
    {
        if (*command++ != *str++)
            return 1;
    }
    return 0;
}

///End extern "C"
}
