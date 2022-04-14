#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <thread>
#include <iomanip>
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
 * 0x10 u8|port u16|volume u16[]|freqs ->
 * 0x11 u8|port u16|MAX_VOLUME u16|MSECONDS_TO_MAX u16[]|freqs ->
 * 0x11 u8|port u8|algorithm u16|max_volume u16|mseconds_to_max *u16|time_step u16[]|freqs ->
 * @note "*" means that presence of parameter depend on algorithm
 * 0x12 -> u8|state *u16|react_time *u16|el_time *u16|ampl
 * 0x18 u16|amplitude, u16|burstPeriod, u16|numberOfBursts, u16|numberOfMeandrs, u16|maxReactionTime -> u8|ifReacted u16|reactionTime u16|amplitude (In percent?)
 * @note react_time in ms
 * @note state can be:
 *      0 - MeasuringHearing
 *      1 - MeasuringReactionTime
 *      2 - MeasureEnd
 * @note "*" near parameter means that these params are sent only if state is MeasureEnd
 *
 * port mean:
 * 0 - A9 pin, left channel
 * 1 - A10 pin, right channel
 *
 *
 * If error in command CC:
 * CC ... -> 0x80(128)+CC u8[]|"string with \0" ///TODO Доделать
 */
uint8_t Commands[]={0x1, 0x4, 0x10, 0x11, 0x12, 0x18};

static const uint8_t SS_OFFSET = 42;
static const size_t BUF_SIZE = 128;///USB packet size

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))
//template<typename T>
//std::enable_if_t<std::is_array<T>::value_type, bool > arr_size(T arr)
//{
//    size_t i = sizeof_arr(arr);
//    auto t = std::tie(arr, i);
//    return t;
// //    return std::tuple(arr, sizeof_arr(arr));
//}

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
        assert(length + sizeof(var) + sizeof(uint8_t) <= BUF_SIZE);/// sizeof(uint8_t) is sizeof(SS)
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
    }
//TODO Write release conf with Tests
    void write(ostream& dev)
    {
        dev.write(buffer, (streamsize) length);
        dev.flush();
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
        for_each(buffer + CC + 1, buffer + length, [&sum](uint8_t c)
        { sum += c; });
        *reinterpret_cast<typeof(sum)*>(buffer+length) = sum;
        length += sizeof(sum);
    }

    void set_cmd(const uint8_t cmd)
    {
        if (count(Commands, Commands + sizeof_arr(Commands), cmd) == 0)
            cout << "Warning! Setting command 0x" << std::hex << (int) cmd << std::dec << ", that is not known in this program" << std::endl;
        buffer[CC] = (char)cmd;
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
        const uint16_t n = *reinterpret_cast<uint16_t*>(buffer + LenL);
        assert(n < BUF_SIZE);
        length =LenH+1;
        dev.read(buffer + length, n);/// DD DD DD ... DD
        length += n;
        uint8_t sum = SS_OFFSET;
        dev.read(buffer + length, sizeof (sum)); /// SS
        for_each(buffer + CC + 1, buffer + length, [&sum](uint8_t c)
        { sum += c; });
        if (sum != *reinterpret_cast<typeof(sum)*>(buffer+length))
        {
            cerr << "SS isn't correct" << endl;
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
        assert(length != 0);
        assert(read_length + sizeof(T) <= length);
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
    [[nodiscard]] uint8_t get_command() const
    {
        return buffer[CC];
    }
};

class TimeMeasurer
{
    constexpr const static auto now = std::chrono::steady_clock::now;
    std::chrono::steady_clock::time_point t_begin = now();
    std::chrono::steady_clock::time_point t_end;
public:
    void begin()
    {
        t_begin = now();
    }
    
    void end(const uint cmd = 0, const string& description = "")
    {
        t_end = now();
        cout << "Time for ";
        if (cmd != 0)
            cout << "0x" << std::hex << cmd << std::dec << " command";
        else if (!description.empty())
            cout << description;
        cout << " takes " << std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_begin).count() << endl;
    }
};

int main (int , char** )
{
    const char* path="stats.csv"; //TODO Rename file
    ofstream stat(path, ios_base::app|ios_base::out);
    Command_writer writer;
    Command_reader reader;
    const char* device_location;
#ifdef linux
    device_location = "/dev/ttyACM0";
#elif defined(windows)
    device_location = "COM3"; // Untested
#else
#error Unsupported OS
#endif
    ofstream dev(device_location);
    ifstream dev_read(device_location);
    if(!dev || !dev_read)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    cout << "begin " << std::flush << std::endl;
    time_t t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    stat << endl << std::put_time(std::localtime(&t), "%y_%m_%d %H:%M:%S") << endl;
    TimeMeasurer timeMeasurer {};
//    system("sleep 3");   ///TODO Tricky error while testing: mk doesn't turn states correctly, but doesn't hang
//TODO Test 0x1
//TODO Test 0x4 (especially with 0x18)
    const uint8_t long_test[] = {0x11, 0x18, 0x11, 0x18, 0x18, 0x11, 0x11, 0x18};
    const uint8_t short_test[] = {0x11, 0x18, 0x11};
//    const auto [cmds, cmds_l] = std::tie(short_test, sizeof_arr(short_test));
    const uint8_t* cmds = short_test;
    const size_t cmds_length = sizeof_arr(short_test);

//    for(size_t i =0;i<4;++i)
    for(const uint8_t* cmd_ptr = cmds; cmd_ptr < cmds + cmds_length; ++cmd_ptr)
    {
        this_thread::sleep_for(1s);
        const int cmd = *cmd_ptr; // i==0 ? 0x11 : i==1 ? 0x18 : i==2 ? 0x18 : /**i == 3*/0x18;
        writer.set_cmd(cmd);
        if (cmd == 0x10 || cmd == 0x11)
            writer.append_var<uint8_t>(1);/// Port
        if (cmd == 0x10)
            writer.append_var<uint16_t>(1000);///Curr volume
        else if (cmd == 0x11)
        {
            writer.append_var<uint16_t>(20000);///max_vol
            writer.append_var<uint16_t>(5000);///msecs to max volume (max reaction time)
        }
        if (cmd == 0x10 || cmd == 0x11)
        {
            writer.append_var<uint16_t>((NOTE_C5));/// array of 1-3 notes
//            writer.append_var<uint16_t>(NOTE_E5);
//            writer.append_var<uint16_t>(NOTE_G5);
        }
        if (cmd == 0x18)
        {
            writer.append_var<uint16_t>(101);/// amplitude
            writer.append_var<uint16_t>(10);/// burst period
            writer.append_var<uint16_t>(10);/// number of bursts
            writer.append_var<uint16_t>(10);/// number of meanders
            writer.append_var<uint16_t>(2000);/// max reaction time
        }
        writer.prepare_for_sending();
        timeMeasurer.begin();
        writer.write(dev);
        reader.read(dev_read);
        timeMeasurer.end(cmd);
        if (!reader.is_error())
        {
            if (cmd == 0x10 || cmd == 0x11)
                assert(reader.is_empty());
            if (cmd == 0x10)
            {}
            if (cmd == 0x11)
            {
                uint8_t state;
                do
                {
                    writer.set_cmd(0x12);
                    writer.prepare_for_sending();
                    timeMeasurer.begin();
                    writer.write(dev);
                    reader.read(dev_read);
                    timeMeasurer.end(0x12);
                    reader.get_param(state);
                    this_thread::sleep_for(1s);
                } while (state != 2); /// 2 - measure end
                ostringstream temp;
                temp << cmd_ptr - cmds << ", " << reader.get_param<uint16_t>() << ", "<<reader.get_param<uint16_t>()<<", "<<(int)reader.get_param<uint16_t>()<<endl;
                cout<<temp.str();
                stat<<temp.str();
            }
            if (cmd == 0x18)
            {
                uint8_t if_reacted = reader.get_param<uint8_t>();
                uint16_t reactionTime = reader.get_param<uint16_t>();
                uint16_t ampl = reader.get_param<uint16_t>();
                ostringstream temp;
                temp << cmd_ptr - cmds << ", " << (int) if_reacted << ", " << reactionTime << ", " << ampl << ", 0x18" <<endl;
                cout << temp.str();
                stat << temp.str();
            }
        }
        else
        {
            cout<<"error: ";
            char c=0;
            do
            {
                reader.get_param(c);
                if (c != '\0')
                    cout<<c;
            } while (c != '\0');
            cout<<endl;
        }
    }

    cout<<"end"<<endl;
}



//    while (true)
//    {
//        int i = 4;
////        system("read dummy");
//        cin>>i;
//        if (i == 0)
//        {
//            dev.close();
//            dev_read.close();
//        }
//        else if (i == 1)
//        {
//            dev.open("/dev/ttyACM0");
//            dev_read.open("/dev/ttyACM0");
//        }
//        else
//        {
//            dev.close();
//            dev_read.close();
//            exit(0);
//        }
//    }
