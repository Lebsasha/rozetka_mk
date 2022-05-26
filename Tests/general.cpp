// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include "general.h"

const std::string del = ", ";

CommandPrinter print_cmd(uint8_t cmd)
{
    return CommandPrinter(cmd);
}

CommandPrinter::CommandPrinter(uint8_t cmd) : cmd(cmd)
{}

std::ostream& operator<<(std::ostream& os, const CommandPrinter& printer)
{
    os << "0x" << std::hex << static_cast<int>(printer.cmd) << std::dec;
    return os;
}
