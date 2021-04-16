#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "main_target.h"

void CommandWriter_ctor(CommandWriter* ptr)
{
    ptr->length=1+1+1;///CC, LenH, LenL
    ptr->BUF_SIZE=128;
    for (uint8_t* c = ptr->buffer+ptr->BUF_SIZE-1; c >= ptr->buffer; --c)
    {
        *c=0;
    }
}

//    template<typename T>
#define T uint8_t
void append_var_8(CommandWriter* ptr, T var)
{
//    assert(ptr->length + sizeof(var) < ptr->BUF_SIZE); ///TODO Error Handle
    *(T*)(ptr->buffer + ptr->length) = var;
    ptr->length += sizeof(var);
//    assert(ptr->buffer[LenL]<128)///TODO Error Handle
    ptr->buffer[LenL] += sizeof(var);
}
#undef T

#define T uint16_t
void append_var_16(CommandWriter* ptr, T var)
{
//    assert(ptr->length + sizeof(var) < ptr->BUF_SIZE); ///TODO Error Handle
    *(T*)(ptr->buffer + ptr->length) = var;
    ptr->length += sizeof(var);
//    assert(ptr->buffer[LenL]<128)///TODO Error Handle
    ptr->buffer[LenL] += sizeof(var);
}
#undef T

void prepare_for_sending(CommandWriter* ptr, uint8_t command_code, bool if_ok)///TODO Delete this todo?
{
    uint8_t sum = SS_OFFSET;
    for(uint8_t* c = ptr->buffer + CC + 1; c < ptr->buffer + ptr->length; ++c)
    { sum += *c; }
    ptr->buffer[ptr->length]=sum;///SS
    ptr->length+=sizeof(uint8_t);
    ptr->buffer[CC]= command_code + 128 * !if_ok;/// 128=1<<7
}

typedef struct CommandReader
{
    uint8_t* buffer;
    size_t length;
    size_t read_length;
}CommandReader;

    bool CommandReader_ctor(CommandReader* ptr, const uint8_t* cmd)
    {
        ptr->buffer=(typeof(ptr->buffer))(cmd);
        ptr->length=3;
        ptr->read_length=0;
        const uint16_t n = (*(uint8_t*)(ptr->buffer + LenH) << 8) + *(uint8_t*)(ptr->buffer + LenL);
        if(n > 64)///USB packet size
            return false;
        ptr->length += n;
        uint8_t sum = SS_OFFSET;
        for(uint8_t* c = ptr->buffer + CC+1; c < ptr->buffer + ptr->length; ++c)
        { sum += *c; }
        if (sum != (uint8_t) ptr->buffer[ptr->length])
        {
            return false;
//            assert(false);///TODO Error handle
        }
ptr->length += sizeof (uint8_t);
ptr->read_length= LenH + 1;
        return true;
    }

    bool is_empty(CommandReader* ptr)
    {
        return ptr->length==3+1 || ptr->length==0;
    }

//    template<typename T>
#ifdef T
#error T already defined
#endif
#define T uint8_t
    T get_param_8(CommandReader* ptr, T* param)
    {
//        assert(ptr->length!=0);///TODO Error handle x2
//       assert(read_length+sizeof(T)+1<=length);
        T param_l;
        if (param)
            *param = *(T*) (ptr->buffer + ptr->read_length);
        else
            param_l= *(T*) (ptr->buffer + ptr->read_length);
        ptr->read_length += sizeof(T);
        return param ? *param : param_l;
    }
#undef T
#define T uint16_t
T get_param_16(CommandReader* ptr, T* param)
{
//        assert(ptr->length!=0);///TODO Error handle x2
//       assert(read_length+sizeof(T)+1<=length);
    *param = *(T*)(ptr->buffer+ptr->read_length);
    ptr->read_length+=sizeof(T);
    return *param;
}
#undef T

uint8_t get_command(CommandReader* ptr)
    {
        return ptr->buffer[CC];
    }

    void Tester_ctor(Tester* ptr)
    {
        ptr->states=Idle;
        ptr->freq=0;
        ptr->react_time=0;
        ptr->react_time_size=0;
        ptr->start_time=0;
        ptr->stop_time=0;
        ptr->port=0;
    }

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim3;
extern struct LED leds[3];
extern volatile uint32_t count;
extern volatile uint32_t time;
extern volatile int measure_one_sine;
extern Button button;
extern Tone_pin* tone_pins;
extern CommandWriter writer;
extern Tester tester;
//extern const int16_t sine_ampl;//TODO Remove some global vars
//extern const uint16_t arr_size;//TODO Sometime cleanup code
//extern int16_t f_dots[];//TODO Measure 0 and 1
//volatile uint32_t dx=(1024*500<<8)/TONE_FREQ;//TODO Move target.h -> Inc
//uint32_t curr=0; // TIM_TRGO_UPDATE; TODO View @ref in docs
//TODO Reformat code
//TODO Unknown command resend (if it needed?)
//TODO What is restrict
//TODO If volatile always needed?
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
        CommandReader reader;
        if(CommandReader_ctor(&reader, command))
        {
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            const uint8_t cmd = get_command(&reader);
            if (cmd == 0x10)
            {
                if(tester.states==Idle)
                {
                    uint8_t port;
                    uint8_t size;
                    uint16_t freq = 0;
                    get_param_8(&reader, &port);
                    get_param_8(&reader, &size);
                    //assert(size<3) //TODO Error Handle x2
                    //assert(port<2);
                    for (volatile uint32_t* c =tone_pins[port].dx;c<tone_pins->dx+size;++c)
                    {
                        get_param_16(&reader, &freq);
                        //assert(*c < TONE_FREQ / 2) //TODO Error handle
                        *c=(tone_pins[port].arr_size * freq << 8) / TONE_FREQ;
                    }
                    prepare_for_sending(&writer, cmd, true);
                }
                else
                   prepare_for_sending(&writer, cmd, false);
            } else if(cmd == 0x1)
            {
                    append_var_8(&writer, 1);
                    const char* VER="Tone";
                    for (const char* c = VER; c <= VER+strlen(VER); ++c)
                    {
                       append_var_8(&writer, *c);
                    }
                    prepare_for_sending(&writer, cmd, true);
            } else if(cmd == 0x11)
            {
                if(tester.states==Idle)
                {
                    get_param_8(&reader, (uint8_t*)&tester.port);
                    get_param_16(&reader, (uint16_t*)&tester.freq);
//                    assert(tester.freq<TONE_FREQ/2); //TODO Error handle
                    tester.states = Measuring_reaction;
                    tone_pins[tester.port].dx[0] = (tone_pins[0].arr_size * tester.freq << 8) / TONE_FREQ;
                    tone_pins[tester.port].dx[1]=0;
                    tone_pins[tester.port].dx[2]=0;
                    tester.start_time = HAL_GetTick();
                    prepare_for_sending(&writer, cmd, true);
                }
                else
                    prepare_for_sending(&writer, cmd, false);
            } else if(cmd == 0x12)
            {
                if(tester.states==Measiring_freq)
                {
                    append_var_16(&writer, tester.react_time);
                    tester.states=Idle;
                    prepare_for_sending(&writer, cmd, true);
                }
                else
                    prepare_for_sending(&writer, cmd, false);
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
{///TODO This can be optimised
///TODO Why silence comes when dx==0?
    *tone_pin->duty_cycle = (uint32_t) (tone_pin->f_dots[tone_pin->curr >> 8]) * COUNTER_PERIOD / tone_pin->sine_ampl;
    tone_pin->curr += tone_pin->dx[0] + tone_pin->dx[1] + tone_pin->dx[2];
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
        pin->dx[0] = (pin->arr_size << 8) * notes[i] / TONE_FREQ;///TODO dx2, dx3 unneeded in this context?
        wait = 1000 / durations[i];
        while ((HAL_GetTick() - start_tick) < wait)
        {
        }
        start_tick += wait;
    }
    pin->dx[0] = 0;///TODO Same as upper TODO
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
