#ifndef COMMUNICATIONWITHPC
#define COMMUNICATIONWITHPC

#include "general.h"

#define PROTOCOL_VERSION ((uint16_t) 1)
#define PROGRAM_VERSION ((uint16_t) 4)
extern const char* const FIRMWARE_VERSION;
//const char* const FIRMWARE_VERSION =  "tone + pulse";

///@brief this enum points on appropriate indexes in binary protocol
///e. g. buffer[CC], ...
enum Commands
{
    CC = 0, LenL = 1, LenH = 2
};

typedef struct CommandWriter
{
    uint8_t buffer[128]; /// It's size equals BUF_SIZE constant
    /// @note length counted without SS byte
    size_t length;
    size_t BUF_SIZE;
    bool is_sending;
} CommandWriter;

void CommandWriter_ctor(CommandWriter* ptr);
//void send_command(CommandWriter* ptr); /// private function in main.c

void process_cmd(const uint8_t* command, const uint32_t* len);

#endif //COMMUNICATIONWITHPC
