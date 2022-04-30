#include <string>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <thread>
#include <iomanip>
#include <vector>
#include <filesystem>
#include <cstdint>
#include "../Core/Inc/notes.h"
#include "protocol_classes.h"

using namespace std;

/**
 * 0x1 -> u8|version u8[]|"string with \0"
 * 0x4 ->
 * 0x10 u8|dynamic u16|volume u16[]|freqs ->
 * 0x11 u8|dynamic u16[]|freqs ->
 * 0x12 u16|new_amplitude *u8|new_button_state -> u8|state u8|button_state *u16|elapsed_time *u16|amplitude
 * new_button_state - sets desired value that button must measure (i. e. if patient presses or released button). This parameter need to sent before one ascending or descending threshold measure. 
 * 0x13 -> u8|state *u16|react_time
 * First time received, this command starts measuring reaction if hearing threshold already estimated, after this it will sent reaction time, when it has been measured
 * 0x18 u16|amplitude, u16|burstPeriod, u16|numberOfBursts, u16|numberOfMeandrs, u16|maxReactionTime -> u8|ifReacted u16|reactionTime u16|amplitude (In percent?)
 * @note react_time in ms
 * @note state can be:
 *      0 - MeasuringHearingThreshold
 *      1 - MeasuringReactionTime
 *      2 - MeasureEnd
 * @note asterisk "*" near parameter means that that parameter is sent only when 'state'(s) parameter(s) reaches some predefined value
 *
 * dynamic mean:
 * 0 - left channel
 * 1 - right channel
 *
 *
 * If error in command CC:
 * CC ... -> 0x80(128)+CC u8[]|"string with \0" ///TODO Переделать c кодами ошибок
 */

const string del = ", "; //Delimiter between writing to csv file results

//TODO rename enums to noun form
/// Possible hearing test states
enum class HearingState
{
    MeasuringHearingThreshold [[maybe_unused]] = 0, /*SendingThresholdResult=1, */MeasuringReactionTime = 1, SendingResults = 2
};

enum class HearingDynamic {Left=0, Right=1};

enum class DesiredButtonState {StartWaitingForPress=0, StartWaitingForRelease=2};
enum class ActualButtonState {WaitingForPress=0, Pressed=1, WaitingForRelease=2, Released=3};

enum class Algorithm {inc_linear_by_step=0, dec_linear_by_step=1, staircase};

class HearingTester
{
public:
    HearingTester(Command_writer& writer, Command_reader& reader, ofstream& stat);
    void execute(uint16_t freq, HearingDynamic dynamic, uint16_t max_amplitude,
                 uint16_t num_of_steps, uint16_t time_step, Algorithm amplitude_algorithm);

private:
    void send_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude);

    void set_pass_parameters(DesiredButtonState state);

    uint16_t get_reaction_time();

    Command_writer& writer;
    Command_reader& reader;
    ostream& stat;

    /// Parameters of test
    uint16_t freq=0;
    uint16_t max_amplitude=0;
    uint16_t num_of_steps=0;
    uint16_t time_step=0;
    Algorithm amplitude_algorithm{};

    /// Results of measure of one pass
    uint16_t elapsed_time_by_mk=0;
    uint16_t received_amplitude_from_mk=0;
    bool is_result_received=false;
};

#define sizeof_arr(arr) (sizeof(arr)/sizeof((arr)[0]))
//template<typename T>
//std::enable_if_t<std::is_array<T>::value_type, bool > arr_size(T arr)
//{
//    size_t i = sizeof_arr(arr);
//    auto t = std::tie(arr, i);
//    return t;
// //    return std::tuple(arr, sizeof_arr(arr));
//}

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
    
    void log_end(const uint8_t cmd = 0, const string& description = "")
    {
        t_end = now();
        cout << "Time for ";
        if (cmd != 0)
            cout << "0x" << std::hex << (int) cmd << std::dec << " command";
        else if (!description.empty())
            cout << description;
        cout << " takes ";
        auto diff = t_end - t_begin;
        if (chrono::duration_cast<chrono::seconds>(diff).count() >= 2)
            cout << chrono::duration_cast<chrono::seconds>(diff).count() << " s";
        else if (chrono::duration_cast<chrono::milliseconds>(diff).count() >= 2)
            cout<< chrono::duration_cast<chrono::milliseconds>(diff).count() << " ms";
        else
            cout << std::chrono::duration_cast<std::chrono::microseconds>(diff).count() << " us";
        cout << endl;
    }
};

template<typename ...T>
auto calculate_amplitude_points(Algorithm alg, std::tuple<T...> algorithm_parameters)
{
    std::vector<pair<uint16_t, uint16_t>> v;
    if (alg == Algorithm::inc_linear_by_step || alg == Algorithm::dec_linear_by_step)
    {
        uint16_t max_amplitude = std::get<0>(algorithm_parameters);
        uint16_t milliseconds_to_max_ampl = std::get<1>(algorithm_parameters);
        int num_of_steps = std::get<2>(algorithm_parameters);
        v.reserve(num_of_steps + 1);
        if (alg == Algorithm::inc_linear_by_step)
        {
            for (int i = 0; i < num_of_steps; ++i)
                v.emplace_back(milliseconds_to_max_ampl * i/num_of_steps, max_amplitude * (i + 1)/num_of_steps);
            v.emplace_back(milliseconds_to_max_ampl, max_amplitude); /// Add one more element to make possible calculating duration of maximum amplitude via difference of time point
        }
        else if (alg == Algorithm::dec_linear_by_step)
        {
            for (int i = 0; i < num_of_steps; ++i)
                v.emplace_back(milliseconds_to_max_ampl * i/num_of_steps, max_amplitude * (num_of_steps - i)/num_of_steps);
            v.emplace_back(milliseconds_to_max_ampl, max_amplitude * 1/num_of_steps);
        }
    }
    return v;
}

int main (int , char** )
{
    auto log_dir = std::filesystem::path("Logs");
    auto stats_path = log_dir.append("stats.csv");
    if (!std::filesystem::exists(log_dir))
    {
        std::filesystem::create_directory(log_dir);
        cout << "Info: log dir '" << log_dir << "' wasn't exist and automatically created" << endl;
    }
    if (!std::filesystem::exists(stats_path))
    {
        ofstream o(stats_path); /// Create file
        o << endl;
        o.close();
    }
//    const char* stats_path="Logs/stats.csv";
    ofstream stat(stats_path, ios_base::app | ios_base::out);
    const char* device_location;
#ifdef __linux__
    device_location = "/dev/ttyACM0";
#elif defined(_WIN32) || defined(_MSC_VER)
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
    int max_amplitude = 0xffff;
//    int max_amplitude = 5000;
    int milliseconds_to_max_volume = 10000;
    int num_of_steps = 10;
    Algorithm amplitude_algorithm = Algorithm::dec_linear_by_step;
    auto amplitudes = calculate_amplitude_points(amplitude_algorithm, tuple(max_amplitude, milliseconds_to_max_volume, num_of_steps));
    uint16_t reaction_time;
    uint16_t elapsed_time;
    uint16_t amplitude_heared;
    uint16_t elapsed_time_by_mk;
    uint16_t amplitude_heared_by_mk;
    HearingTester hearing_tester(writer, reader, stat);
//TODO Test 0x4 (especially with 0x18)
    const uint8_t long_test[] = {0x11, 0x18, 0x11, 0x18, 0x18, 0x11, 0x11, 0x18};
    const uint8_t short_test[] = {0x11, 0x4, 0x11, 0x11};
//    const auto [cmds, cmds_l] = std::tie(short_test, sizeof_arr(short_test));
    const uint8_t* cmds = short_test;
    const size_t cmds_length = sizeof_arr(short_test);

    cout << "begin " << std::endl;
//    for(size_t i =0;i<4;++i)
    for(const uint8_t* cmd_ptr = cmds; cmd_ptr < cmds + cmds_length; ++cmd_ptr)
    {
        this_thread::sleep_for(1s);
        const int cmd = *cmd_ptr;
        if (cmd == 0x11)
        {
            stat << cmd_ptr - cmds << ", "; /// Number of curr command
            hearing_tester.execute(4000, HearingDynamic::Right, 0x0fff, 10, 400, Algorithm::staircase);

            continue;
        }
        writer.set_cmd(cmd);
        if (cmd == 0x10 || cmd == 0x11)
            writer.append_var<uint8_t>( (uint8_t)(HearingDynamic::Right) );/// Channel
        if (cmd == 0x10)
            writer.append_var<uint16_t>(0xffff);///Curr volume
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
            writer.append_var<uint16_t>((4000));/// array of 1-3 notes
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
        time_measurer.log_end(cmd);
        if (!reader.is_error())
        {
            if (cmd == 0x10 || cmd == 0x11)
                assert(reader.is_empty());
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
                    time_measurer.log_end(0x12);
                    assert(!reader.is_error());
                    uint8_t state = reader.get_param<uint8_t>();
                    if (state == (uint8_t) HearingState::MeasuringReactionTime) /// Patient reacted and mk starts measuring reaction
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
                        time_measurer.log_end(0x13);
                        assert(!reader.is_error());
                        reader.get_param(state);
                        this_thread::sleep_for(1s);
                    } while (state != (uint8_t) HearingState::SendingResults); /// until measure has being ended
                    reaction_time = reader.get_param<uint16_t>();
//                    uint16_t elapsed_time = reader.get_param<uint16_t>();
//                    uint16_t amplitude = reader.get_param<uint16_t>();
                    uint16_t real_elapsed_time;
                    uint16_t real_amplitude_heared;
                    if (reaction_time < elapsed_time_by_mk)
                    {
                        real_elapsed_time = elapsed_time_by_mk - reaction_time;
                        real_amplitude_heared = (std::find_if(amplitudes.begin(), amplitudes.end(), [&](const auto& item)
                                                                { return item.first >= real_elapsed_time; }) - 1)->second;
                    }
                    else
                    {
                        real_elapsed_time = 0;
                        real_amplitude_heared = 0;
                    }

                    log_string << reaction_time << del << real_elapsed_time << del << real_amplitude_heared << del
                               << elapsed_time_by_mk << del << amplitude_heared_by_mk << del << elapsed_time << del << amplitude_heared;
                }
                if (amplitude_algorithm == Algorithm::inc_linear_by_step)
                    log_string << del << "inc";
                else if (amplitude_algorithm == Algorithm::dec_linear_by_step)
                    log_string << del << "dec";
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
            else
            {} //Another commands don't need to be handled
        }
        else
        {
            cerr<<"error: ";
            char c=0;
            do
            {
                reader.get_param(c);
                if (c != '\0')
                    cerr<<c;
            } while (c != '\0');
            cerr<<endl;
        }
        stat.flush();
    }

    cout<<"end"<<endl;
}



HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ofstream& _stat): writer(_writer), reader(_reader), stat(_stat)
{}

void HearingTester::execute(uint16_t _freq, HearingDynamic _dynamic, uint16_t _max_amplitude, uint16_t _num_of_steps, uint16_t _time_step, Algorithm _amplitude_algorithm)
{
//    assert(_freq < BASE_FREQ/2);
    freq = _freq;
    max_amplitude = _max_amplitude;
    if (num_of_steps * time_step > 120'000)
    {
        cout << "Warning! Maximum time test could take is " << num_of_steps * time_step/1000 << " seconds" << endl;
    }
    num_of_steps = _num_of_steps;
    time_step = _time_step;
    amplitude_algorithm = _amplitude_algorithm;

    uint8_t cmd = 0x11;
    Time_measurer time_measurer{};
    uint16_t reaction_time = 0;
    uint16_t elapsed_time = 0;
    uint16_t amplitude_heard = 0;
    is_result_received = false;
//    uint16_t elapsed_time_by_mk = 0;
//    uint16_t received_amplitude_from_mk = 0;

    writer.set_cmd(cmd);
    writer.append_var<uint8_t>( (uint8_t)(HearingDynamic::Right) );/// Channel
    writer.append_var<uint16_t>((freq));/// array of 1-3 notes
//    writer.append_var<uint16_t>(NOTE_E5);
//    writer.append_var<uint16_t>(NOTE_G5);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(cmd);
    if (!reader.is_error())
    {
        assert(reader.is_empty());

//        int milliseconds_to_max_volume = 10000;

        if (amplitude_algorithm == Algorithm::staircase)
        {
            //Incrementing amplitude pass
            uint16_t curr_step = 0;
            uint16_t curr_time = 0;
            uint16_t curr_amplitude = 0;
            set_pass_parameters(DesiredButtonState::StartWaitingForPress);
            while (curr_step < num_of_steps)
            {
                curr_time = (curr_step + 1) * time_step;
                curr_amplitude = (curr_step + 1) * max_amplitude / num_of_steps;
                send_new_amplitude_and_receive_threshold_results(curr_amplitude);
                if (is_result_received)
                    break;
                this_thread::sleep_for(chrono::milliseconds(time_step));
                curr_step++;
                amplitude_heard = curr_amplitude;
                elapsed_time = curr_time;
            }
            send_new_amplitude_and_receive_threshold_results(0);
            this_thread::sleep_for(chrono::milliseconds(1000));
        }
        else
            assert(amplitude_algorithm == Algorithm::staircase);

        ostringstream log_string;
        if (is_result_received) /// Patient reacted
        {
            reaction_time = get_reaction_time();
            //uint16_t elapsed_time = reader.get_param<uint16_t>();
            //uint16_t amplitude = reader.get_param<uint16_t>();
            uint16_t real_elapsed_time;
            uint16_t real_amplitude_heard;
            if (reaction_time < elapsed_time_by_mk)
            {
                real_elapsed_time = elapsed_time_by_mk - reaction_time;
                real_amplitude_heard = received_amplitude_from_mk;
            } else
            {
                real_elapsed_time = 0;
                real_amplitude_heard = 0;
            }

            log_string << reaction_time << del << real_elapsed_time << del << real_amplitude_heard << del
                       << elapsed_time_by_mk << del << received_amplitude_from_mk << del << elapsed_time << del << amplitude_heard;
        }
        else /// Patient never reacted, need to stop measure
        {
            writer.set_cmd(0x4);
            writer.prepare_for_sending();
            writer.write();
            reader.read();
            assert(!reader.is_error() && reader.is_empty());
            log_string << '0' << del << '0' << del << '0' << del << '0' << del << '0' << del << '0' << del << '0';
        }
        switch (amplitude_algorithm)
        {
            case Algorithm::inc_linear_by_step:
                log_string << del << "inc";
                break;
            case Algorithm::dec_linear_by_step:
                log_string << del << "dec";
                break;
            case Algorithm::staircase:
                log_string << del << "staircase";
                break;
        }

        log_string << endl;
        cout << log_string.str();
        stat << log_string.str();
    }
    else
    {
        string err = reader.read_error();
        cout << "error occurred when sending command: " << err << endl;
    }
    stat.flush();
}

/// set parameters for pass
void HearingTester::set_pass_parameters(DesiredButtonState state)
{
    Time_measurer time_measurer{};
    uint8_t command = 0x12;
    writer.set_cmd(command);
    writer.append_var(0); // temporary need add amplitude to this command
    writer.append_var(state);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(command);
    assert(!reader.is_error());
}

void HearingTester::send_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude)
{
    Time_measurer time_measurer{};
    uint8_t command = 0x12;
    writer.set_cmd(command);
    writer.append_var(curr_amplitude);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(command);
    assert(!reader.is_error());

    HearingState state = (HearingState) reader.get_param<uint8_t>();
    assert(state == HearingState::MeasuringHearingThreshold);
    ActualButtonState button_state = (ActualButtonState) reader.get_param<uint8_t>();
    if (button_state == ActualButtonState::Pressed || button_state == ActualButtonState::Released)
    {
        is_result_received = true;
        elapsed_time_by_mk = reader.get_param<uint16_t>();
        received_amplitude_from_mk = reader.get_param<uint16_t>();
    }
    else  ///nothing needs to be read, we waiting until patient press or release the button (or when time has gone)
    { }
}

uint16_t HearingTester::get_reaction_time()
{
    Time_measurer time_measurer{};
    HearingState state;
    do {
        writer.set_cmd(0x13);
        writer.prepare_for_sending();
        time_measurer.begin();
        writer.write();
        reader.read();
        time_measurer.log_end(0x13);
        assert(!reader.is_error());
        state = (HearingState) reader.get_param<uint8_t>();
        this_thread::sleep_for(1s);
    } while (state != HearingState::SendingResults); /// do until measure has being ended
    uint16_t reaction_time = reader.get_param<uint16_t>();
    return reaction_time;
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
