#include <iostream>
#include <string>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <thread>
#include <iomanip>
#include <vector>
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
 * 0x11 u8|port u16[]|freqs ->
 * @note "*" means that presence of parameter depend on algorithm
 * 0x12 u16|new_amplitude -> u8|state *u16|elapsed_time *u16|amplitude
 * 0x13 -> u8|state *u16|react_time
 * 0x18 u16|amplitude, u16|burstPeriod, u16|numberOfBursts, u16|numberOfMeandrs, u16|maxReactionTime -> u8|ifReacted u16|reactionTime u16|amplitude (In percent?)
 * @note react_time in ms
 * @note state can be:
 *      0 - MeasuringHearingThreshold
 *      1 - MeasuringReactionTime
 *      2 - MeasureEnd
 * @note asterisk "*" near parameter means that that parameter is sent only when state reaches some predefined value
 *
 * port mean:
 * 0 - A9 pin, left channel
 * 1 - A10 pin, right channel
 *
 *
 * If error in command CC:
 * CC ... -> 0x80(128)+CC u8[]|"string with \0" ///TODO Доделать
 */
uint8_t Commands[]={0x1, 0x4, 0x10, 0x11, 0x12, 0x13, 0x18};

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
    ostream& dev;
public:
    explicit Command_writer(ostream& dev): dev(dev), length(LenH + 1)
    {}

    template<typename T>
    void append_var(T var)
    {
        assert(length + sizeof(var) + sizeof(uint8_t) <= BUF_SIZE);/// sizeof(uint8_t) is sizeof(SS)
        *reinterpret_cast<T*>(buffer + length) = var;
        length += sizeof(var);
    }
//TODO Write release conf with Tests
    void write()
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
    size_t length;  /// Length of buffer without counting SS
    size_t read_length;
    istream& dev;
public:
    explicit Command_reader(istream& dev): dev(dev), length(0), read_length(0)
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

class Time_measurer
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

auto linear_step_algorithm(uint16_t max_amplitude, int milliseconds_to_max_ampl, int num_of_steps)
{
    std::vector<pair<uint16_t, uint16_t>> v;
    v.reserve(num_of_steps + 1);
    for (int i = 0; i <= num_of_steps; ++i)
        v.emplace_back(milliseconds_to_max_ampl * i/num_of_steps, max_amplitude * (i+1)/num_of_steps);
    return v;
}

int main (int , char** )
{
    const char* path="stats.csv"; //TODO Rename file
    ofstream stat(path, ios_base::app|ios_base::out);
    const char* device_location;
#ifdef linux
    device_location = "/dev/ttyACM0";
#elif defined(windows)
    device_location = "COM3"; // Untested
#else
#error Unsupported OS
#endif
    ofstream dev_write(device_location);
    ifstream dev_read(device_location);
    if(!dev_write || !dev_read)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    Command_writer writer(dev_write);
    Command_reader reader(dev_read);
    time_t t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    stat << endl << std::put_time(std::localtime(&t), "%y_%m_%d %H:%M:%S") << endl;
    Time_measurer time_measurer {};
    int max_amplitude = 20000;
    int milliseconds_to_max_volume = 10000;
    int num_of_steps = 30;
    auto amplitudes = linear_step_algorithm(max_amplitude, milliseconds_to_max_volume, num_of_steps);
    uint16_t reaction_time;
    uint16_t elapsed_time;
    uint16_t amplitude_heared;
    uint16_t elapsed_time_by_mk;
    uint16_t amplitude_heared_by_mk;
//TODO Test 0x4 (especially with 0x18)
    const uint8_t long_test[] = {0x11, 0x18, 0x11, 0x18, 0x18, 0x11, 0x11, 0x18};
    const uint8_t short_test[] = {0x11, 0x18, 0x11};
//    const auto [cmds, cmds_l] = std::tie(short_test, sizeof_arr(short_test));
    const uint8_t* cmds = short_test;
    const size_t cmds_length = sizeof_arr(short_test);

    cout << "begin " << std::endl;
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
            reaction_time = 0;
            elapsed_time = 0;
            amplitude_heared = 0;
            elapsed_time_by_mk = 0;
            amplitude_heared_by_mk = 0;
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
        time_measurer.begin();
        writer.write();
        reader.read();
        time_measurer.end(cmd);
        if (!reader.is_error())
        {
            if (cmd == 0x10 || cmd == 0x11)
                assert(reader.is_empty());
            if (cmd == 0x10)
            {}
            else
            {
                const string del = ", ";
                if (cmd == 0x11)
                {
                    for (auto iter = amplitudes.begin(); iter < amplitudes.end() - 1; ++iter)  /// As we calculate time as difference between elements, last element in 'amplitudes' only indicates time duration of amplitude before last element
                    {
                        auto [time, amplitude] = *iter;
                        auto next_time = (iter+1)->first;
                        writer.set_cmd(0x12);
                        writer.append_var(amplitude);
                        writer.prepare_for_sending();
                        time_measurer.begin();
                        writer.write();
                        reader.read();
                        time_measurer.end(0x12);
                        assert(!reader.is_error());
                        uint8_t state = reader.get_param<uint8_t>();
                        if (state == 1) /// Patient reacted and mk starts measuring reaction
                        {
                            elapsed_time_by_mk = reader.get_param<uint16_t>();
                            amplitude_heared_by_mk = reader.get_param<uint16_t>();
                            break;
                        }
                        this_thread::sleep_for(chrono::milliseconds(next_time - time));
                        amplitude_heared = amplitude;
                        elapsed_time = time;
                    }
                    ostringstream log_string;
                    log_string << cmd_ptr - cmds << del; /// Number of curr command
                    if (amplitude_heared_by_mk == 0)  /// Patient doesn't reacted, need to stop measure
                    {
                        writer.set_cmd(0x4);
                        writer.prepare_for_sending();
                        writer.write();
                        reader.read();
                        assert(!reader.is_error() && reader.is_empty());
                        log_string << '0' << del << '0' << del << '0' << del << '0' << del << '0' << del << '0' << del << '0';
                    }
                    else /// Patient reacted
                    {
    //                    assert(elapsed_time <= elapsed_time_by_mk);
                        uint8_t state;
                        do {
                            writer.set_cmd(0x13);
                            writer.prepare_for_sending();
                            time_measurer.begin();
                            writer.write();
                            reader.read();
                            time_measurer.end(0x13);
                            assert(!reader.is_error());
                            reader.get_param(state);
                            this_thread::sleep_for(1s);
                        } while (state != 2); /// 2 - measure end
                        reaction_time = reader.get_param<uint16_t>();//TODO If reaction time higher than elapsed_time
    //                    uint16_t elapsed_time = reader.get_param<uint16_t>();
    //                    uint16_t amplitude = reader.get_param<uint16_t>();
                        auto real_elapsed_time = elapsed_time_by_mk - reaction_time;
                        auto real_amplitude_heared = (std::find_if(amplitudes.begin(), amplitudes.end(),
                                                              [&](const auto& item){ return item.first >= real_elapsed_time; }) - 1)->second;

                        log_string << reaction_time << del << real_elapsed_time << del << real_amplitude_heared << del
                            << elapsed_time_by_mk << del << amplitude_heared_by_mk << del << elapsed_time << del << amplitude_heared;
                    }
                    log_string << endl;
                    cout << log_string.str();
                    stat << log_string.str();
                }
                else if (cmd == 0x18)
                {
                    uint8_t if_reacted = reader.get_param<uint8_t>();
                    uint16_t reaction_time_on_skin_conduction = reader.get_param<uint16_t>();
                    uint16_t ampl = reader.get_param<uint16_t>();
                    ostringstream temp;
                    temp << cmd_ptr - cmds << del << (int) if_reacted << del << reaction_time_on_skin_conduction << del << ampl << del << "0x18" << endl;
                    cout << temp.str();
                    stat << temp.str();
                }
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
