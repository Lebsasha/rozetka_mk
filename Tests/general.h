#ifndef GENERAL
#define GENERAL

#include <ostream>

/// @section Some settings related to time in program

#define USE_SLEEP

#define USE_SLEEP_IN_HEARING_TEST

extern const std::string del; //Delimiter between results written to csv file


/// @section Auxiliary stuff

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))

/// Proxy class for printing command in hexadecimal view
class CommandPrinter
{
    uint8_t cmd;
public:
    explicit CommandPrinter(uint8_t cmd);

    friend std::ostream& operator<<(std::ostream& os, const CommandPrinter& printer);
};

CommandPrinter print_cmd(uint8_t cmd);

#endif //GENERAL
