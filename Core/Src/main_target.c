#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "main_target.h"

void Command_writer_ctor(Command_writer* ptr)
{
    ptr->length = 3*sizeof(uint8_t);///CC, LenH, LenL
    ptr->BUF_SIZE = 128;///Can be changed if needed, but with appropriate changes with buffer array
    ptr->buffer[CC]=0;
    ptr->buffer[LenL]=0;
    ptr->buffer[LenH]=0;
}

//    template<typename T>
#define def_append_var(size) void append_var_##size(Command_writer* ptr, uint##size##_t var) \
{\
    if(ptr->length + sizeof(var) + sizeof(uint8_t) > ptr->BUF_SIZE)\
        return;\
    *reinterpret_cast(uint##size##_t*, ptr->buffer + ptr->length) = var;\
    ptr->length += sizeof(var);\
}
//TODO Rewrite with size and switch

def_append_var(8);
def_append_var(16);

void prepare_for_sending(Command_writer* ptr, uint8_t command_code, bool if_ok)
{
    *reinterpret_cast(uint16_t*, ptr->buffer+LenL)=ptr->length-3*sizeof(uint8_t);
    uint8_t checksum = SS_OFFSET;
    for(uint8_t* c = ptr->buffer + CC + 1; c < ptr->buffer + ptr->length; ++c)
        checksum += *c;
    *reinterpret_cast(typeof(checksum)*, ptr->buffer + ptr->length)=checksum;
    ptr->length+=sizeof(checksum);
    ptr->buffer[CC]= command_code + 128 * !if_ok;/// 128==1<<7
}

typedef struct Command_reader
{
    uint8_t* buffer;
    size_t length;
    size_t read_length;
}Command_reader;

bool Command_reader_ctor(Command_reader* ptr, const uint8_t* cmd, const uint32_t cmd_length)
{
    ptr->buffer = (typeof(ptr->buffer)) (cmd);
    if(cmd_length <= 3)
        return false;
    ptr->length = 3*sizeof(uint8_t);
    const uint16_t n = *reinterpret_cast(uint16_t*, ptr->buffer+LenL);
    uint8_t checksum = SS_OFFSET;
    if (cmd_length != 3 + n + sizeof(checksum))
        return false;
    ptr->length += n;
    for (uint8_t* c = ptr->buffer + CC + 1; c < ptr->buffer + ptr->length; ++c)
        checksum += *c;
    if (checksum != *reinterpret_cast(typeof(checksum)*, ptr->buffer + ptr->length))
        return false;
    ptr->read_length = LenH + 1;
    return true;
}

bool is_empty(Command_reader* ptr)
{
    return ptr->length == 3*sizeof(uint8_t) || ptr->length == 0;
}

//    template<typename T>
#define def_get_param(size) bool get_param_##size(Command_reader* ptr, uint##size##_t* param)\
{\
    if(ptr->length==0 || (ptr->read_length+sizeof(*param) > ptr->length))\
         return false;\
    *param = *reinterpret_cast(uint##size##_t*, ptr->buffer+ptr->read_length);\
    ptr->read_length+=sizeof(*param);\
    return true;\
}
///TODO Rewrite this
def_get_param(8);
def_get_param(16);

uint8_t get_command(Command_reader* ptr)
{
    return ptr->buffer[CC];
}

extern TIM_HandleTypeDef htim1;
extern Tone_pin* tone_pins;
extern Command_writer writer;
extern Tester tester;
//TODO Add release config
//TODO Sometime cleanup code
//TODO ASK if main() is busy, is it good for usb

const int16_t sine_ampl = (1U << (sizeof(sine_ampl) * 8 - 1)) - 1;
const uint16_t arr_size = 1024; /*!<for optimization purposes*/
int16_t f_dots[1024];

void tone_pin_ctor(Tone_pin* ptr, volatile uint32_t* CCR)
{
    for (int i = 0; i < arr_size; ++i)
        f_dots[i] = (int16_t)(sine_ampl / 2.0 - sine_ampl / 2.0 * sin(i * 2 * M_PI / arr_size));
    ptr->f_dots = f_dots;
    ptr->arr_size = arr_size;
    ptr->volume = 0;
    memset((void*)ptr->dx, 0, sizeof(ptr->dx));
    memset((void*)ptr->curr, 0, sizeof(ptr->curr));
    ptr->duty_cycle = CCR;//TODO Test this
}

void make_tone(Tone_pin* tone_pin)
{
    if (tone_pin->volume != 0)
        for (uint8_t i = 0; i < (uint8_t)sizeof_arr(tone_pin->dx); ++i)
        {
            if (i == 0)
                *tone_pin->duty_cycle = (uint32_t)(tone_pin->f_dots[tone_pin->curr[i] >> 8]) * COUNTER_PERIOD / sine_ampl / sizeof_arr(tone_pin->dx);
            else
                *tone_pin->duty_cycle += (uint32_t)(tone_pin->f_dots[tone_pin->curr[i] >> 8]) * COUNTER_PERIOD / sine_ampl / sizeof_arr(tone_pin->dx);
            tone_pin->curr[i] += tone_pin->dx[i];
            if (tone_pin->curr[i] >= arr_size << 8)
            {
                tone_pin->curr[i] -= arr_size << 8;
            }
        }
    *tone_pin->duty_cycle = (*tone_pin->duty_cycle * tone_pin->volume) >> 16;
}

void play(Tone_pin* pin, const uint16_t* notes, const uint8_t* durations, int n)
{
    volatile uint32_t wait;///volatile need to shut up compiler warning about "infinit" loop below
    uint32_t start_tick = HAL_GetTick();
    for (int i = 0; i < n; ++i)
    {
        pin->dx[0] = freq_to_dx(pin, notes[i]);
        wait = 1000 / durations[i];
        while ((HAL_GetTick() - start_tick) < wait)
        {
        }
        start_tick += wait;
    }
    pin->dx[0] = 0;
}


void Tester_ctor(Tester* ptr)
{
    ptr->states = Idle;
    memset((void*)ptr->freq, 0, sizeof(ptr->freq));
    ptr->react_time = 0;
    ptr->react_time_size = 0;
    ptr->button.start_time = 0;
    ptr->button.stop_time = 0;
    ptr->port=0;
    ptr->mseconds_to_max=2000;
    ptr->max_volume=6000;
}

#ifdef NDEBUG
#define usb_assert(cond) ((void)0)
#else
#define usb_assert(cond) do{if(!(cond)) \
{                                   \
    assertion_fail(#cond, cmd);   \
    return;                         \
}}while(false)
#endif

void assertion_fail(const char*, uint8_t cmd);

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)
    {
        Command_reader reader;
        if(Command_reader_ctor(&reader, command, *len))
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            const uint8_t cmd = get_command(&reader);
            if (cmd == 0x1)
            {
                append_var_16(&writer, 1);///protocol version
                append_var_16(&writer, 1);/// program version
                const char* VER = "tone";
//                const char* VER = "Sound tester";
                for (const char* c = VER; c <= VER + strlen(VER); ++c)
                    append_var_8(&writer, *c);
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x4)
            {
                tester.states=Idle;
                tone_pins[tester.port].volume=0;
                for (volatile uint32_t* c = tone_pins[tester.port].dx; c < tone_pins[tester.port].dx + sizeof_arr(tone_pins[tester.port].dx); ++c)
                    *c = 0;
                tester.button.start_time=0;
                tester.button.stop_time=0;
                tester.react_time=0;
                tester.react_time_size=0;

                ///next line not necessary but maybe needed in some cases
//                tester.ampl=0;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x10)
            {
                usb_assert(tester.states == Idle);
                uint8_t port;
                get_param_8(&reader, &port);
                usb_assert(port < 2);
                uint16_t volume;
                get_param_16(&reader, &volume);
                tone_pins[port].volume=0;
                uint16_t freq = 0;
                for (volatile uint32_t* c = tone_pins[port].dx; c < tone_pins[port].dx + sizeof_arr(tone_pins[port].dx); ++c)
                {
                    if (!get_param_16(&reader, &freq))
                        freq = 0;
                    usb_assert(freq <= 3400);
                    *c = freq_to_dx(&tone_pins[port], freq);
                }
                tone_pins[port].volume=volume;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x11)
            {
                usb_assert(tester.states == Idle);
                get_param_8(&reader, (uint8_t*) &tester.port);
                usb_assert(tester.port < 2);
                tone_pins[tester.port].volume=0;
                get_param_16(&reader, (uint16_t*) &tester.max_volume);
                get_param_16(&reader, (uint16_t*) &tester.mseconds_to_max);
                for (volatile uint16_t* freq = tester.freq; freq < tester.freq + sizeof_arr(tester.freq); ++freq)
                {
                    if (!get_param_16(&reader, (uint16_t*) freq))
                        *freq = 0;
                    usb_assert(*freq <= 3400);
                }
                for (size_t i = 0; i < sizeof_arr(tester.freq); ++i)
                    tone_pins[tester.port].dx[i] = freq_to_dx(&tone_pins[tester.port], tester.freq[i]);
                tester.states = Measuring_freq;
                tester.button.start_time = HAL_GetTick();
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x12)
            {
                usb_assert(tester.states != Idle);
                if (tester.states == Measuring_freq)
                {
                    append_var_8(&writer, Measuring_freq);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                }
                else if (tester.states == Measuring_reaction)
                {
                    append_var_8(&writer, Measuring_reaction);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                }
                else if(tester.states == Sending)
                {
                    append_var_8(&writer, Sending);
                    append_var_16(&writer, tester.react_time);
                    append_var_16(&writer, tester.elapsed_time);
                    append_var_16(&writer, tester.ampl);
                    tester.react_time=0;
//                    tester.ampl=0;
                    tester.states = Idle;
                }
                prepare_for_sending(&writer, cmd, true);
            }
            else
                assertion_fail("Command not recognised", cmd);
        }
        if (command[0] == '0')
            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void assertion_fail(const char* cond, const uint8_t cmd)
{
    writer.length=3;///clean writer if I already have appended something; this line replaces Command_writer_ctor() call for optimisation purposes
    size_t length = strlen(cond)+3+1+1>=writer.BUF_SIZE?writer.BUF_SIZE-3-1-1 : strlen(cond);///3 - CC, LenL, LenH; 1 - SS (checksum byte); 1 - size of '\0'
    for (const char* c = cond; c < cond + length; ++c)
        append_var_8(&writer, *c);
    append_var_8(&writer, '\0');
    prepare_for_sending(&writer, cmd, false);
}

/// \param us - time in microseconds
void delay_us(int us)
{
    if (us <= 0)//TODO Test for timer microseconds
        return;
    __HAL_TIM_SET_COUNTER(&htim1, 0);
    while (__HAL_TIM_GET_COUNTER(&htim1) < us)
    {}
}
