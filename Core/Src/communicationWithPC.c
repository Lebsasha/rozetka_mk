#include <string.h>
#include "main.h"
#include "communicationWithPC.h"
#include "skinConduction.h"
#include "hearing.h"

const char* const FIRMWARE_VERSION =  "tone + pulse";
static const uint8_t SS_OFFSET = 42;

void Command_writer_ctor(Command_writer* ptr)
{
    ptr->length = 3*sizeof(uint8_t);///CC, LenH, LenL
    ptr->BUF_SIZE = 128;///This constant can be changed if needed, but with appropriate changes with buffer array //TODO
    ptr->buffer[LenL]=0;
    ptr->buffer[LenH]=0;
    ptr->ifSending = false;
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
    if (command_code < 128)
        ptr->buffer[CC] = command_code + 128 * !if_ok;/// 128 == 1<<7
    else
        ptr->buffer[CC] = command_code; /// Error bit already set in command_code
    ptr->ifSending = true;
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
    if(param == NULL || ptr->length==0 || (ptr->read_length+sizeof(*param) > ptr->length))\
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

extern Command_writer writer;
//TODO ASK if main() is busy, is it good for usb

#ifdef NDEBUG
#define usb_assert(cond) ((void)0)
#else
#define usb_assert(cond) do{if(!(cond)) \
{                                   \
    assertion_fail(#cond, cmd);   \
    return;                         \
}}while(false)
#endif

extern volatile Measures currMeasure;
extern RandInitializer randInitializer;
extern SkinConductionTester skinTester;
extern Button button;
extern HearingTester hearingTester;
extern Tone_pin* tone_pins;//TODO Move tone_pins from this file to hearingTester

void assertion_fail(const char*, uint8_t cmd);

void process_cmd(const uint8_t* command, const uint32_t* len)
{
    if (*len)
    {
        Command_reader reader;
        if(Command_reader_ctor(&reader, command, *len))
        {
#ifdef DEBUG
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
#endif
            if (!randInitializer.isInitialised)
                InitRand(&randInitializer);

            const uint8_t cmd = get_command(&reader);
            if (cmd == 0x1)
            {
                append_var_16(&writer, PROTOCOL_VERSION);
                append_var_16(&writer, PROGRAM_VERSION);
                const char* const firmware_string_end = FIRMWARE_VERSION + strlen(FIRMWARE_VERSION);
                for (const char* c = FIRMWARE_VERSION; c <= firmware_string_end; ++c)//with '/0' including
                    append_var_8(&writer, *c);
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x4)
            {
                if(currMeasure==Hearing)
                {
                    currMeasure = None;
                    tone_pins[hearingTester.port].volume = 0;
                    HearingStop(&hearingTester);
                    hearingTester.react_surveys_elapsed = 0;
//                    hearingTester.ampl=0; //not necessary but maybe needed in some cases
                }
                else if (currMeasure == SkinConduction)
                {
                    SkinConductionStop(&skinTester);
                }
                currMeasure = None;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x10)
            {
                usb_assert(currMeasure == None);
//                usb_assert(hearingTester.states == Idle);
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
                tone_pins[port].volume = volume;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x11)
            {
                usb_assert(currMeasure == None);
//                usb_assert(hearingTester.states == Idle);///Теоретически не нужно проверять, но на всякий случай пусть будет
                get_param_8(&reader, (uint8_t*) &hearingTester.port);
                usb_assert(hearingTester.port < 2);
//                tone_pins[hearingTester.port].volume=0;//TODO Move to HearingTesterStart()
                usb_assert(get_param_16(&reader, (uint16_t*) &hearingTester.max_volume));
                usb_assert(get_param_16(&reader, (uint16_t*) &hearingTester.mseconds_to_max));
                usb_assert(hearingTester.mseconds_to_max > 0);
                for (volatile uint16_t* freq = hearingTester.freq; freq < hearingTester.freq + sizeof_arr(hearingTester.freq); ++freq)
                {
                    if (!get_param_16(&reader, (uint16_t*) freq))
                        *freq = 0;
                    usb_assert(*freq <= 3400);
                }
                for (size_t i = 0; i < sizeof_arr(hearingTester.freq); ++i)
                    tone_pins[hearingTester.port].dx[i] = freq_to_dx(&tone_pins[hearingTester.port], hearingTester.freq[i]);
                currMeasure = Hearing;
                hearingTester.algorithm = ConstantTone;
                hearingTester.tone_step = hearingTester.mseconds_to_max / 10;
//                hearingTester.max_volume = 0x100;
                HearingStart(&hearingTester);
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x12)//TODO ASK
            {
                usb_assert(currMeasure == Hearing);
                usb_assert(hearingTester.states != Idle);
                if (hearingTester.states == Measuring_freq)
                {
                    append_var_8(&writer, Measuring_freq);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                }
                else if (hearingTester.states == Measuring_reaction)
                {
                    append_var_8(&writer, Measuring_reaction);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                    append_var_16(&writer, 0);
                }
                else if(hearingTester.states == Sending)
                {
                    append_var_8(&writer, Sending);
                    append_var_16(&writer, hearingTester.react_time);
                    append_var_16(&writer, hearingTester.elapsed_time);
                    append_var_16(&writer, hearingTester.ampl);
                    HearingStop(&hearingTester);
                    currMeasure = None;
                }
                prepare_for_sending(&writer, cmd, true);
            }
            else if(cmd == 0x18)
            {
                usb_assert(currMeasure == None);

                usb_assert(get_param_16(&reader, &skinTester.channel[0].amplitude));
                usb_assert(get_param_16(&reader, (uint16_t*) &skinTester.burstPeriod));
                usb_assert(get_param_16(&reader, &skinTester.numberOfBursts));
                usb_assert(get_param_16(&reader, &skinTester.numberOfMeandrs));
                usb_assert(get_param_16(&reader, &skinTester.maxReactionTime));

//TODO Даник, напиши здесь проверки на диапазон допустимых значений для параметров, например, как у меня "usb_assert(hearingTester.port < 2);"
                skinTester.numberOfBursts -= 1;
                SkinConductionStart(&skinTester);
                currMeasure = SkinConduction;
//                prepare_for_sending(&writer, cmd, true);
            }
//            else if (cmd == 0x20)//TODO ASK Why?
            else
                assertion_fail("Command not recognised", cmd);
        }
//        if (command[0] == '0')
//            HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}

void SkinConductionSendResultToPC(SkinConductionTester* skinTester)
{
    if (skinTester->reactionTime == 0)
    {
        append_var_8(&writer, false);
        append_var_16(&writer, 0);
        append_var_16(&writer, 0);
    }
    else
    {
        append_var_8(&writer, true);
        append_var_16(&writer, skinTester->reactionTime);
        append_var_16(&writer, skinTester->channel[0].amplitude);
    }
    prepare_for_sending(&writer, 0x18, true);
    currMeasure = None;
}

void assertion_fail(const char* cond, const uint8_t cmd)
{
    writer.length=3;///clean writer if I already have appended something; this line replaces Command_writer_ctor() call
    size_t length = strlen(cond)+3*sizeof(uint8_t)+1*sizeof (uint8_t)+1 >= writer.BUF_SIZE ? writer.BUF_SIZE-3*sizeof(uint8_t)-1*sizeof(uint8_t)-1 : strlen(cond);///3 - CC, LenL, LenH; 1 - SS (checksum byte); 1 - size of '\0'
    for (const char* c = cond; c < cond + length; ++c)
        append_var_8(&writer, *c);
    append_var_8(&writer, '\0');
    prepare_for_sending(&writer, cmd, false);
}


