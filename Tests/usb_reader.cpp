#include <iostream>
#include <fstream>
#include <algorithm>
#include <cassert>
#include "../Core/Inc/notes.h"

using namespace std;

///@brief this enum points on appropriate indexes in bin. prot.
///e. g. buffer[CC], ...
enum
{
    CC = 0, LenL = 1, LenH = 2
};

/**
 * 0x1 -> u8|version u8[]|"string with \0"
 * 0x4 ->
 * 0x10 u8|port u8|volume u16[]|freqs ->
 * @note freqs preserve 1 digit after point with help of fixed point, i. e. if you pass 300,6 Hz it will transmit and set in mk 300,6 Hz
 * 0x11 u8|port u8|volume u16[]|freqs ->
 * 0x12 -> u16|react_time u8|ampl
 * @note react_time in ms
 *
 * If error in command CC:
 * CC ... -> 0x80(128)+CC u8[]|"string with \0" ///TODO Не так, как в стандарте!
 */
uint8_t Commands[]={0x1, 0x4, 0x10, 0x11, 0x12};

static const uint8_t SS_OFFSET = 42;
static const size_t BUF_SIZE = 64;///USB packet size

class Command_writer
{
    char buffer[BUF_SIZE] = {0};
    size_t length;
public:
    Command_writer() : length(LenH + 1)
    {}

    template<typename T>
    void append_var(T var)
    {
        assert(length + sizeof(var) < BUF_SIZE);///Less only (without equal) because we must append CC
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
        buffer[LenL] += sizeof(var);
    }
//TODO Припаять "кнопку"
//TODO Write release conf with Tests
    void write(ostream& dev)
    {
        dev.write(buffer, length);
        dev.flush();
        for(char* c=buffer+length-1;c>=buffer;--c)
        {
            *c=0;
        }
        length=LenH+1;
    }

    void prepare_for_sending()
    {
        assert(buffer[CC]!=0);
        uint8_t sum = SS_OFFSET;
        for_each(buffer + CC + 1, buffer + length, [&sum](char c)
        { sum += c; });
        buffer[length] = sum;
        length += sizeof(sum);
    }

    void set_cmd(const uint8_t cmd)
    {
        assert(count(Commands, Commands + sizeof(Commands)/sizeof(Commands[0]), cmd)==1);
        buffer[CC] = cmd;
    }
};

class Command_reader
{
    char buffer[BUF_SIZE] = {0};
    size_t length;
    size_t read_length;
public:
    Command_reader() : length(0), read_length(0)
    {}

    char* read(istream& dev)
    {
        dev.read(buffer, LenH + 1);/// CC LenL LenH
        const uint16_t n = (*reinterpret_cast<uint8_t*>(buffer + LenH) << 8) + *reinterpret_cast<uint8_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        length =LenH+1;
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        dev.read(buffer + length, sizeof (uint8_t)); /// SS
        uint8_t sum = SS_OFFSET;
        for_each(buffer + LenL, buffer + length, [&sum](char c)
        { sum += c; });
        if (sum != (uint8_t) buffer[length])
        {
            cerr << "SS isn't correct" << endl;
            assert(false);
        }
        length += sizeof (uint8_t);
        read_length= LenH + 1;
        return buffer;
    }

    bool is_empty() const
    {
        return length==3+1 || length==0;
    }
    int is_error() const
    {
        return buffer[CC]&(1<<7);
    }
    template<typename T>
    T get_param(T& param)
    {
        assert(length != 0);
        assert(read_length + sizeof(T) + 1 <= length);
        param = *reinterpret_cast<T*>(buffer + read_length);
        read_length += sizeof(T);
        return param;
    }

    template<typename T>
    T get_param()
    {
        T param;
        return get_param(param);
    }
    uint8_t get_command() const
    {
        return buffer[CC];
    }
};

int main (int , char** )
{
    const char* path="react_time.csv";
    ofstream stat(path, ios_base::app|ios_base::out);
    Command_writer writer;
    Command_reader reader;
    char comp_command[]="sleep  ";
    ofstream dev("/dev/ttyACM0");
    ifstream dev_read("/dev/ttyACM0");
    if(!dev || !dev_read)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    cout<<"begin "<<std::flush;
    for(size_t i =0;i<10;++i)
    {
        comp_command[sizeof(comp_command) - 1 - 1] = rand() % 4 + '0';
        system(comp_command);
        const int cmd = 0x11;
        writer.set_cmd(cmd);
        writer.append_var<uint8_t>(1);/// Port
        writer.append_var<uint8_t>(30);
//        writer.append_var<uint16_t>((NOTE_C4)*10);
//        writer.append_var<uint16_t>(NOTE_E4*10);
        writer.append_var<uint16_t>(NOTE_G4);
//        writer.append_var<uint16_t>((NOTE_C5));
        writer.prepare_for_sending();
        writer.write(dev);
        reader.read(dev_read);
        if (!reader.is_error())
        {
            assert(reader.is_empty());
            if (cmd == 0x11)
            {
                do
                {
                    system("sleep 1");
                    writer.set_cmd(0x12);
                    writer.prepare_for_sending();
                    writer.write(dev);
                    reader.read(dev_read);
                } while (reader.is_error());
                uint16_t react_time;
                std::cout << i << ' ' << reader.get_param(react_time) << ", el_time "<<reader.get_param<uint16_t>()<<", Ampl: "<<(int)reader.get_param<uint8_t>()<<endl;
            }
        }
        else
        {
            cout<<"error: ";
            char c=0;
            do
            {
                reader.get_param(c);
                cout<<c;
            } while (c);
            cout<<endl;
        }
        //TODO Спросить про Clock tree; Vref, etc.,
    }

    cout<<"end"<<endl;
}
