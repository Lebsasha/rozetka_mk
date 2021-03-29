
#include "main.h"
#include "main_target.h"
using namespace std;





void CommandWriter_ctor(CommandWriter* ptr)
{
    ptr->length=0;
    ptr->BUF_SIZE=128;
    ptr->buffer=new uint8_t[ptr->BUF_SIZE];
}

//    template<typename T>
#define T uint8_t
void append_var(CommandWriter* ptr, T var)
{
    assert(ptr->length + sizeof(var) < ptr->BUF_SIZE);
    *(T*)(ptr->buffer + ptr->length) = var;
    ptr->length += sizeof(var);
    ptr->buffer[LenL] += sizeof(var);///TODO!
}
#undef T


void prepare_for_sending(CommandWriter* ptr, uint8_t command_code, bool if_ok)///TODO
{
    ptr->buffer[CC]= command_code + 128 * !if_ok;
    ptr->buffer[LenL]=0;
    ptr->buffer[LenH]=0;
    ptr->length= 1 + 1 + 1;
    uint8_t sum = SS_OFFSET;
    for(uint8_t* c = ptr->buffer + CC + 1; c < ptr->buffer + ptr->length; ++c)
    { sum += *c; }
    ptr->buffer[LenH + 1]=sum;///SS
}

class CommandReader
{
    uint8_t* buffer;
    size_t length;
    size_t read_lehgth;
public:
    explicit CommandReader(const uint8_t* cmd) : buffer(const_cast<uint8_t*>(cmd)), length(0), read_lehgth(0)
    {
//        dev.read(buffer, length = LenH + 1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint8_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint8_t*>(buffer + LenL);
        assert(n < 64);///USB packet size
//        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
//        dev.read(buffer + length, sizeof (uint8_t)); /// SS
        uint8_t sum = SS_OFFSET;
        for(uint8_t* c = buffer + LenL; c < buffer + length; ++c)
        { sum += *c; }
        if (sum != (uint8_t) buffer[length])
        {
            assert(false);///TODO Error handle
        }
        length += sizeof (uint8_t);
        read_lehgth=LenH+1;
    }

    template<typename T>
    T get_param(T& param)
    {
        assert(length!=0);
        param = *reinterpret_cast<T*>(buffer+read_lehgth);
        read_lehgth+=sizeof(T);
        return param;
    }

    uint8_t get_command()
    {
        return buffer[CC];
    }
};
extern "C" {
#include <string.h>
#include <stdlib.h>

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern struct LED leds[3];
extern volatile uint32_t count;
extern volatile uint32_t time;
extern volatile int measure_one_sine;
extern Button button;
extern Tone_pin* tone_pins;
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
        CommandReader reader(command);
        CommandWriter writer;
        if(reader.get_command()==0x10)
        {
            uint8_t port;
            uint16_t freq;
            reader.get_param(port);
            reader.get_param(freq);
            if(port < 2 && freq < TONE_FREQ/2)
            {
                tone_pins[port].dx = (tone_pins[port].arr_size * freq << 8) / TONE_FREQ;
                prepare_for_sending(&writer, reader.get_command(), true);
            }
            else
            {

            }
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
