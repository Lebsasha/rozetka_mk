#ifndef COMMUNICATION_WITH_MCU
#define COMMUNICATION_WITH_MCU

#include <cassert>
#include <sstream>
#include <iostream>
#include <algorithm>

///General constants for Writer and Reader classes

///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum
{
    CC = 0, LenL = 1, LenH = 2
};

static const uint8_t Commands[]={0x1, 0x4, 0x10, 0x11, 0x12, 0x13, 0x14, 0x18};

static const uint8_t SS_OFFSET = 42;
static const size_t BUF_SIZE = 128;///USB packet size

class Command_writer
{
    std::ostream& dev;
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    explicit Command_writer(std::ostream& dev): dev(dev), length(LenH + 1)
    {}

    template<typename T = uint16_t>
    void append_var(T var)
    {
        assert(length + sizeof(var) + sizeof(uint8_t) <= BUF_SIZE);/// sizeof(uint8_t) is sizeof(SS)
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
    }

    void write()
    {
        dev.write(buffer, (std::streamsize) length);
        dev.flush();
        if (! dev.good())
        {
            std::stringstream error {};
            error << "Cannot write to COM port, because one of error flag had set when last command 0x" << std::hex << (int) buffer[CC] << " was sent to device";
            throw std::runtime_error(error.str());
        }
        for(char* c=buffer+length-1;c>=buffer;--c)
        {
            *c=0;
        }
        length=LenH+1;
    }

    void prepare_for_sending()
    {
        *reinterpret_cast<uint16_t*>(buffer+LenL)=length-3*sizeof(uint8_t);
        uint8_t sum = SS_OFFSET;
        std::for_each(buffer + CC + 1, buffer + length, [&sum](uint8_t c)
        { sum += c; });
        *reinterpret_cast<decltype(sum)*>(buffer+length) = sum;
        length += sizeof(sum);
    }

    void set_cmd(const uint8_t cmd)
    {
        if (std::count(Commands, Commands + sizeof_arr(Commands), cmd) == 0)
            std::cout << "Warning! Setting command 0x" << std::hex << (int) cmd << std::dec << ", that is not known in this program" << std::endl;
        buffer[CC] = (char)cmd;
    }
};

class Command_reader
{
    std::istream& dev;
    char buffer[BUF_SIZE] = {0};
    size_t length;  /// Length of buffer without counting SS
    size_t read_length;
public:
    explicit Command_reader(std::istream& dev): dev(dev), length(0), read_length(0)
    {}

    char* read()
    {
        dev.read(buffer, LenH + 1);/// CC LenL LenH
        const uint16_t n = *reinterpret_cast<uint16_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        length =LenH+1;
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        uint8_t sum = SS_OFFSET;
        dev.read(buffer + length, sizeof (sum)); /// SS
        std::for_each(buffer + CC + 1, buffer + length, [&sum](uint8_t c)
        { sum += c; });
        if (sum != *reinterpret_cast<decltype(sum)*>(buffer+length))
        {
            std::cerr << "SS isn't correct" << std::endl;
            assert(false);
        }
        read_length= LenH + 1;
        return buffer;
    }

    [[nodiscard]] bool is_empty() const
    {
        return length==3*sizeof(uint8_t) || length==0;
    }

    [[nodiscard]] uint8_t is_error() const
    {
        return buffer[CC]&(1<<7);
    }

    template<typename T>
    T get_param(T& param)
    {
        assert(!is_empty());
        assert(read_length + sizeof(T) <= length);
        return __get_param(param);
    }

    template<typename T>
    T get_param()
    {
        T param;
        return get_param(param);
    }

    [[nodiscard]] uint8_t get_command() const
    {
        return buffer[CC];
    }

    std::string get_error_string()
    {
        std::stringstream err;
        char c=0;
        do
        {
            if (is_empty() || read_length + sizeof(c) > length)
                break;
            __get_param(c);
            if (c != '\0')
                err<<c;
        } while (c != '\0');
        return err.str();
    }

private:

    template<typename T>
    T __get_param(T& param) // NOLINT(bugprone-reserved-identifier)
    {
        param = *reinterpret_cast<T*>(buffer + read_length);
        read_length += sizeof(T);
        return param;
    }
};

#endif //COMMUNICATION_WITH_MCU
