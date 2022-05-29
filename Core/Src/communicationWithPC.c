#include <string.h>
#include <stdlib.h>
#include "main.h"
#include "communicationWithPC.h"
#include "skinConduction.h"
#include "hearing.h"

const char* const FIRMWARE_VERSION =  "tone + pulse";
static const uint8_t SS_OFFSET = 42;

void CommandWriter_ctor(CommandWriter* ptr)
{
    ptr->length = 3*sizeof(uint8_t);///CC, LenH, LenL
    ptr->BUF_SIZE = 128;///This constant can be changed if needed, but with appropriate changes with buffer array size
    ptr->buffer[LenL]=0;
    ptr->buffer[LenH]=0;
    ptr->is_sending = false;
}

bool append_var_8(CommandWriter* ptr, uint8_t var)
{
    if (ptr->length + sizeof(var) + sizeof(uint8_t) > ptr->BUF_SIZE)
        return false;
    *reinterpret_cast(uint8_t*, ptr->buffer + ptr->length) = var;
    ptr->length += sizeof(var);
    return true;
}
bool append_var_16(CommandWriter* ptr, uint16_t var)
{
    if (ptr->length + sizeof(var) + sizeof(uint8_t) > ptr->BUF_SIZE)
        return false;
    *reinterpret_cast(uint16_t*, ptr->buffer + ptr->length) = var;
    ptr->length += sizeof(var);
    return true;
}

void reset_buffer(CommandWriter* ptr)
{
    ptr->length = 3;
}

void prepare_for_sending(CommandWriter* ptr, uint8_t command_code, bool if_ok)
{
    *reinterpret_cast(uint16_t*, ptr->buffer+LenL) = ptr->length - 3*sizeof(uint8_t);
    uint8_t checksum = SS_OFFSET;
    for(uint8_t* c = ptr->buffer + CC + 1; c < ptr->buffer + ptr->length; ++c)
        checksum += *c;
    *reinterpret_cast(typeof(checksum)*, ptr->buffer + ptr->length) = checksum;
    ptr->length += sizeof(checksum);
    if ((command_code & 1<<7) == 0)
        ptr->buffer[CC] = command_code + (1<<7) * !if_ok;
    else
        ptr->buffer[CC] = command_code; /// Error bit already set in received from PC command_code, so we setting command as it arrived (with error code) even when @a if_ok == true (i. e. there are no errors occurred)
    ptr->is_sending = true;
}

typedef struct CommandReader
{
    uint8_t* buffer;
    /// @note length counted without SS byte
    size_t length;
    size_t read_length;
}CommandReader;

bool CommandReader_ctor(CommandReader* ptr, const uint8_t* cmd, const uint32_t cmd_length)
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

bool is_empty(CommandReader* ptr)
{
    return ptr->length == 3*sizeof(uint8_t) || ptr->length == 0;
}

bool get_param_8(CommandReader* ptr, uint8_t* param)
{
    if(param == NULL || ptr->length==0 || (ptr->read_length + sizeof(*param) > ptr->length))
         return false;
    *param = *reinterpret_cast(uint8_t*, ptr->buffer + ptr->read_length);
    ptr->read_length += sizeof(*param);
    return true;
}
bool get_param_16(CommandReader* ptr, uint16_t* param)
{
    if(param == NULL || ptr->length==0 || (ptr->read_length + sizeof(*param) > ptr->length))
         return false;
    *param = *reinterpret_cast(uint16_t*, ptr->buffer + ptr->read_length);
    ptr->read_length += sizeof(*param);
    return true;
}

uint8_t get_command(CommandReader* ptr)
{
    return ptr->buffer[CC];
}

extern CommandWriter writer;
extern volatile Measures currMeasure;
extern RandInitializer randInitializer;
extern Button button;
extern SkinConductionTester skinTester;
extern HearingTester hearingTester;
extern TonePin* tone_pins;//TODO Try to move tone_pins from this file to hearingTester

/// @note There is difference between standard @a assert implementation and this implementation of @a assert:
/// Even when defined NDEBUG, expression @a cond will still be evaluated
#ifdef NDEBUG
#define usb_assert(cond) ((void)(cond))
#else
#define usb_assert(cond) do{if(!(cond)) \
{                                   \
    assertion_fail(#cond, cmd);   \
    return;                         \
}}while(false)
#endif

void assertion_fail(const char*, uint8_t cmd);

void process_cmd(const uint8_t* command, const uint32_t len)
{
    if (len)
    {
        CommandReader reader;
        if(CommandReader_ctor(&reader, command, len))
        {
            write_pin_if_in_debug(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
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
                    set_new_tone_volume(&hearingTester, 0);
                    hearing_stop(&hearingTester);
                    hearingTester.react_surveys_elapsed = 0;
                }
                currMeasure = None;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x10)
            {
                usb_assert(currMeasure == None || currMeasure == Hearing);
                get_param_8(&reader, (uint8_t*) &hearingTester.dynamic);
                usb_assert(hearingTester.dynamic < 2);
                uint16_t volume;
                get_param_16(&reader, &volume);
                usb_assert(volume <= HEARING_VOLUME_MAX);
                uint16_t freq = 0;
                for (volatile uint32_t* c = tone_pins[hearingTester.dynamic].dx; c < tone_pins[hearingTester.dynamic].dx + sizeof_arr(tone_pins[hearingTester.dynamic].dx); ++c)
                {
                    if (!get_param_16(&reader, &freq))
                        freq = 0;
                    usb_assert(freq <= TONE_FREQ / 2);
                    *c = freq_to_dx(&tone_pins[hearingTester.dynamic], freq);
                }
                hearingTester.state = PlayingConstantToneForDebug; // as we need only to set unchanging tone and nothing else needs to be done, we make state Idle
                set_new_tone_volume(&hearingTester, volume);
                hearing_start(&hearingTester);
                currMeasure = Hearing;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x11)
            {
                usb_assert(currMeasure == None || (currMeasure == Hearing && hearingTester.state == PlayingConstantToneForDebug));
                usb_assert(get_param_8(&reader, (uint8_t*) &hearingTester.dynamic));
                usb_assert(hearingTester.dynamic < 2);
                for (volatile uint16_t* freq = hearingTester.freq; freq < hearingTester.freq + sizeof_arr(hearingTester.freq); ++freq)
                {
                    if (!get_param_16(&reader, (uint16_t*) freq))
                        *freq = 0;
                    usb_assert(*freq <= TONE_FREQ / 2);
                }
                for (size_t i = 0; i < sizeof_arr(hearingTester.freq); ++i)
                    tone_pins[hearingTester.dynamic].dx[i] = freq_to_dx(&tone_pins[hearingTester.dynamic], hearingTester.freq[i]);
                hearing_start(&hearingTester);
                hearingTester.state = PlayingConstantVolume;
                currMeasure = Hearing;
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x12)
            {
                usb_assert(currMeasure == Hearing);
                if(hearingTester.state == PlayingConstantVolume)
                {
                    /// We beginning new pass
                    ButtonStateOnHighLevel newButtonState;
                    usb_assert(get_param_8(&reader, (uint8_t*) &newButtonState));
                    usb_assert(newButtonState == HL_WaitingForPress || newButtonState == HL_WaitingForRelease);
                    ButtonStart(&button, newButtonState);
                    hearingTester.is_results_on_curr_pass_captured = false;

                    append_var_8(&writer, MeasuringHearingThreshold);
                }
                else if (hearingTester.state == StartingMeasuringReaction || hearingTester.state == WaitingBeforeMeasuringReaction
                         || hearingTester.state == MeasuringReaction)
                {
                    append_var_8(&writer, MeasuringReactionTime);
                }
                else // We shouldn't come to this section, but if this occurred, we must report about this as error to high level program
                {
                    append_var_8(&writer, MeasuringHearingThreshold);
                    append_var_8(&writer, hearingTester.state);
                    prepare_for_sending(&writer, cmd, false);
                    return;
                }
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x13)
            {
                usb_assert(currMeasure == Hearing);
                if(hearingTester.state == PlayingConstantVolume)
                {
                    uint16_t new_volume;
                    usb_assert(get_param_16(&reader, &new_volume));
                    usb_assert(new_volume <= HEARING_VOLUME_MAX);
                    set_new_tone_volume(&hearingTester, new_volume);
                    append_var_8(&writer, MeasuringHearingThreshold);

                    if (button.state == WaitingForPress)
                        append_var_8(&writer, HL_WaitingForPress);
                    else if (button.state == WaitingForRelease)
                        append_var_8(&writer, HL_WaitingForRelease); /// Simply wait until patient press or release button
                    else if (button.state == Pressed)
                    {
                        append_var_8(&writer, HL_Pressed);
                        append_var_16(&writer, hearingTester.elapsed_time);
                        append_var_16(&writer, hearingTester.ampl);
                    }
                    else if (button.state == Released)
                    {
                        append_var_8(&writer, HL_Released);
                        append_var_16(&writer, hearingTester.elapsed_time);
                        append_var_16(&writer, hearingTester.ampl);
                    }
                    else
                        assertion_fail("Wrong button state", cmd); //If we come in this place, state is likely Timeout or ButtonIdle, so this assertion should always fail
                }
                else if (hearingTester.state == StartingMeasuringReaction || hearingTester.state == WaitingBeforeMeasuringReaction
                            || hearingTester.state == MeasuringReaction)
                {
                    append_var_8(&writer, MeasuringReactionTime);
                }
                else // We shouldn't come to this section, but if this occurred, we must report about this as error to high level program
                {
                    append_var_8(&writer, MeasuringHearingThreshold);
                    append_var_8(&writer, hearingTester.state);
                    prepare_for_sending(&writer, cmd, false);
                    return;
                }
                prepare_for_sending(&writer, cmd, true);
            }
            else if (cmd == 0x14)
            {
                usb_assert(currMeasure == Hearing);
                if (hearingTester.state == PlayingConstantVolume)
                {
                    usb_assert(get_param_16(&reader, (uint16_t* ) &hearingTester.amplitude_for_react_survey));
                    usb_assert(hearingTester.amplitude_for_react_survey <= HEARING_VOLUME_MAX);
                    append_var_8(&writer, MeasuringReactionTime);
                    hearingTester.state = StartingMeasuringReaction;
                }
                else if (hearingTester.state == MeasuringReaction || hearingTester.state == WaitingBeforeMeasuringReaction)
                {
                    append_var_8(&writer, MeasuringReactionTime);
                }
                else if(hearingTester.state == Sending)
                {
                    append_var_8(&writer, SendingResults);
                    for (int i = 0; i < REACT_SURVEYS_COUNT; ++i)
                    {
                        usb_assert(append_var_16(&writer, hearingTester.react_times[i]));
                    }
//                    append_var_16(&writer, hearingTester.elapsed_time);/// Elapsed time and amplitude already sent via 0x12 command
//                    append_var_16(&writer, hearingTester.ampl);
                    hearing_stop(&hearingTester);
                    currMeasure = None;
                }
                else
                {
                    /// Normally we shouldn't come to this section, but if error occurred we must report about this to high level program
                    append_var_8(&writer, SendingResults);
                    append_var_8(&writer, hearingTester.state);
                    prepare_for_sending(&writer, cmd, false);
                    return;
                }
                prepare_for_sending(&writer, cmd, true);
            }
            else if(cmd == 0x18)
            {
                usb_assert(currMeasure == None);

                usb_assert(get_param_16(&reader, &skinTester.amplitude));
                usb_assert(get_param_16(&reader, (uint16_t*) &skinTester.burstPeriod));
                usb_assert(get_param_16(&reader, &skinTester.numberOfBursts));
                usb_assert(get_param_16(&reader, &skinTester.numberOfMeandrs));
                usb_assert(get_param_16(&reader, &skinTester.maxReactionTime));
                usb_assert(get_param_16(&reader, &skinTester.fillingFrequency));

//TODO Даник, напиши здесь проверки на диапазон допустимых значений для параметров, например, как у меня "usb_assert(hearingTester.dynamic < 2);"
                skinTester.numberOfBursts -= 1;
                SkinConductionStart(&skinTester);
                currMeasure = SkinConduction;
//                prepare_for_sending(&writer, cmd, true);
            }
            else
                assertion_fail("Command not recognised", cmd);
        }
        if (command[0] == '0')
        {
//            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
           HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
            append_var_8(&writer, 11);
            prepare_for_sending(&writer, 0x11, true);
//            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
        }
    }
}

void SkinConduction_send_result_to_PC(SkinConductionTester* ptr)
{
    if (ptr->reactionTime == 0)
    {
        append_var_8(&writer, false);
        append_var_16(&writer, 0);
        append_var_16(&writer, 0);
    }
    else
    {
        append_var_8(&writer, true);
        append_var_16(&writer, ptr->reactionTime);
        append_var_16(&writer, ptr->amplitude);
    }
    prepare_for_sending(&writer, 0x18, true);
    currMeasure = None;
}

void assertion_fail(const char* cond, const uint8_t cmd)
{
    reset_buffer(&writer); ///clean writer if I already have appended something
    size_t str_length = strlen(cond) + 3*sizeof(uint8_t) + 1*sizeof(uint8_t) + 1 >= writer.BUF_SIZE ? writer.BUF_SIZE - 3*sizeof(uint8_t) - 1*sizeof(uint8_t) - 1 : strlen(cond);///3 - CC, LenL, LenH; 1 - SS (checksum byte); 1 - size of '\0'
    for (const char* c = cond; c < cond + str_length; ++c)
        append_var_8(&writer, *c);
    append_var_8(&writer, '\0');
    prepare_for_sending(&writer, cmd, false);
}


