#ifndef COMMUNICATIONWITHPC
#define COMMUNICATIONWITHPC

#define PROTOCOL_VERSION ((uint16_t) 1)
#define PROGRAM_VERSION ((uint16_t) 4)
#define FIRMWARE_VERSION "tone + pulse"
#include "general.h"

///@brief this enum points on appropriate indexes in binary protocol
///e. g. buffer[CC], ...
enum Commands
{
    CC = 0, LenL = 1, LenH = 2
};

typedef struct Command_writer //TODO rename to CamelCase (and reader same)
{
    uint8_t buffer[128];///BUF_SIZE constant
    size_t length;
    size_t BUF_SIZE;///const
    bool ifSending;
} Command_writer;

void Command_writer_ctor(Command_writer* ptr);

//void send_command(Command_writer* ptr);

void process_cmd(const uint8_t* command, const uint32_t* len);

#endif //COMMUNICATIONWITHPC
