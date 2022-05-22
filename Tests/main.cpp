#include <string>
#include <fstream>
#include <algorithm>
#include <cassert>
#include <sstream>
#include <thread>
#include <iomanip>
#include <vector>
#include <cstdint>
#include <filesystem>
#include "../Core/Inc/notes.h"
#include "general.h"
#include "communication_with_MCU.h"
#include "time_measurer.h"
#include "hearing_tester.h"

/**
 * @code 0x1 -> u8|version u8[]|"string with \0" @endcode
 * @code 0x4 -> @endcode
 * @code 0x10 u8|dynamic u16|volume u16[]|freqs -> @endcode
 * @code 0x11 u8|dynamic u16[]|freqs -> @endcode
 * @code 0x12 u8|new_button_state -> u8|state @endcode
 * new_button_state - sets desired value that button must measure (e. g. if patient presses or releases button). This parameter should be sent before one pass (ascending or descending threshold measure)
 * @code 0x13 u16|new_amplitude -> u8|state u8|button_state *u16|elapsed_time *u16|amplitude @endcode
 * If @a 0x12 or @a 0x13 command received when device stand in inappropriate state (for example, when device sending reaction time results), then it will send these parameters with error bit set
 * @code 0x1X ... -> u8|state (that will be equal to MeasuringHearingThreshold) u8|internal_state @endcode
 * @a internal_state parameter describes what actually device does when error occurred. Refer to @c HearingStates enum in file @c src/Core/hearing.h for finding out possible states of this parameter
 * @code 0x14 *u16|amplitude_for_react_survey -> u8|state *u16|react_time @endcode
 * First time received, this command starts measuring reaction time, after this it will sent reaction time, when it has been measured
 * @code 0x18 u16|amplitude, u16|burstPeriod, u16|numberOfBursts, u16|numberOfMeandrs, u16|maxReactionTime -> u8|ifReacted u16|reactionTime u16|amplitude (In percent?) @endcode
 * state can be:
 *      0 - MeasuringHearingThreshold
 *      1 - MeasuringReactionTime
 *      2 - MeasureEnd
 *
 * Asterisk ("*") near parameter means that that parameter is sent only when 'state'(s) parameter(s) reaches some predefined value
 *
 * dynamic mean:
 * 0 - right channel
 * 1 - left channel
 *
 *
 * If error occur in command with code CC, then device will send command with raised 1'th bit in command code and string description about this error, i. e.:
 * @code 1<<7 | CC ... -> u8[]|"string with \0" @endcode ///TODO Переделать c кодами ошибок
 */

using namespace std;

//template<typename T>
//std::enable_if_t<std::is_array<T>::value_type, bool > arr_size(T arr)
//{
//    size_t i = sizeof_arr(arr);
//    auto t = std::tie(arr, i);
//    return t;
// //    return std::tuple(arr, sizeof_arr(arr));
//}

template<typename Rep, typename Period>
inline void sleep(const chrono::duration<Rep, Period>& rtime)
{
#ifdef USE_SLEEP
    this_thread::sleep_for(rtime);
#endif
}

template<typename ...T>
auto calculate_amplitude_points(PassAlgorithm alg, std::tuple<T...> algorithm_parameters)
{
    std::vector<pair<uint16_t, uint16_t>> v;
    if (alg == PassAlgorithm::inc_linear_by_step || alg == PassAlgorithm::dec_linear_by_step)
    {
        uint16_t max_amplitude = std::get<0>(algorithm_parameters);
        uint16_t milliseconds_to_max_ampl = std::get<1>(algorithm_parameters);
        int num_of_steps = std::get<2>(algorithm_parameters);
        v.reserve(num_of_steps + 1);
        if (alg == PassAlgorithm::inc_linear_by_step)
        {
            for (int i = 0; i < num_of_steps; ++i)
                v.emplace_back(milliseconds_to_max_ampl * i/num_of_steps, max_amplitude * (i + 1)/num_of_steps);
            v.emplace_back(milliseconds_to_max_ampl, max_amplitude); /// Add one more element to make possible calculating duration of maximum amplitude via difference of time point
        }
        else if (alg == PassAlgorithm::dec_linear_by_step)
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
    ofstream dev_write(device_location);
    ifstream dev_read(device_location);
    if(!dev_write || !dev_read)
    {
        std::cout << "Error opening COM file" << std::endl;
        return 1;
    }
    system("stty -F /dev/ttyACM0 -parenb -parodd -cmspar cs8 hupcl -cstopb cread -clocal -crtscts -ignbrk -brkint ignpar -parmrk -inpck -istrip -inlcr -igncr -icrnl -ixon -ixoff -iuclc -ixany -imaxbel -iutf8 -opost -olcuc -ocrnl -onlcr -onocr -onlret -ofill -ofdel nl0 cr0 tab0 bs0 vt0 ff0 -isig -icanon -iexten -echo -echoe -echok -echonl -noflsh -xcase -tostop -echoprt -echoctl -echoke -flusho -extproc");
    Command_writer writer(dev_write);
    Command_reader reader(dev_read);
//#elif defined(_WIN32) || defined(_MSC_VER)
//    device_location = "COM3"; // Untested
#else
#error Unsupported OS
#endif
    time_t t = std::chrono::system_clock::to_time_t(chrono::system_clock::now());
    stat << endl << std::put_time(std::localtime(&t), "%y_%m_%d %H:%M:%S") << endl;
    srand(t);
    Time_measurer time_measurer {};
    int max_amplitude = 0xffff;
//    int max_amplitude = 5000;
    int milliseconds_to_max_volume = 10000;
    int num_of_steps = 10;
    PassAlgorithm amplitude_algorithm = PassAlgorithm::dec_linear_by_step;
    auto amplitudes = calculate_amplitude_points(amplitude_algorithm, tuple(max_amplitude, milliseconds_to_max_volume, num_of_steps));
    uint16_t reaction_time;
    uint16_t elapsed_time;
    uint16_t amplitude_heared;
    uint16_t elapsed_time_by_mk;
    uint16_t amplitude_heared_by_mk;
    HearingTester hearing_tester(writer, reader, stat);
//TODO Test 0x4 (especially with 0x18)
    const vector<uint8_t> long_test = {0x11, 0x18, 0x11, 0x18, 0x18, 0x11, 0x11, 0x18};
    const vector<uint8_t> short_test = {0x11/*, 0x4, 0x11*/};
//    const auto [cmds, cmds_l] = std::tie(short_test, sizeof_arr(short_test));
    auto& cmds = short_test;
//    const size_t cmds_length = sizeof_arr(short_test);

    cout << "begin " << std::endl;
//    for(size_t i =0;i<4;++i)
    try
    {
    for(auto cmd_ptr = cmds.cbegin(); cmd_ptr < cmds.cend(); ++cmd_ptr) // NOLINT(cppcoreguidelines-narrowing-conversions)
    {
        sleep(1s);
        const int cmd = *cmd_ptr;
        if (cmd == 0x11)
        {
            stat << cmd_ptr - cmds.cbegin() << ", "; /// Number of curr command
            hearing_tester.execute({4000, 4000, 20, 400, PassAlgorithm::staircase, AmplitudeChangingAlgorithm::linear, TimeStepChangingAlgorithm::random_deviation});

            continue;
        }
        writer.set_cmd(cmd);
        if (cmd == 0x10 || cmd == 0x11)
            writer.append_var<uint8_t>( (uint8_t)(HearingDynamic::Right) );/// Channel
        if (cmd == 0x10)
            writer.append_var<uint16_t>(500);///Curr volume
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
                    writer.set_cmd(0x13);
                    writer.append_var(amplitude);
                    writer.prepare_for_sending();
                    time_measurer.begin();
                    writer.write();
                    reader.read();
                    time_measurer.log_end(0x13);
                    assert(!reader.is_error());
                    uint8_t state = reader.get_param<uint8_t>();
                    if (state == (uint8_t) HearingState::MeasuringReactionTime) /// Patient reacted and mk starts measuring reaction
                    {
                        elapsed_time_by_mk = reader.get_param<uint16_t>();
                        amplitude_heared_by_mk = reader.get_param<uint16_t>();
                        break;
                    }
                    sleep(chrono::milliseconds(next_time - time));
                    amplitude_heared = amplitude;
                    elapsed_time = time;
                }
                ostringstream log_string;
                log_string << cmd_ptr - cmds.cbegin() << del; /// Number of curr command
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
                        writer.set_cmd(0x14);
                        writer.prepare_for_sending();
                        time_measurer.begin();
                        writer.write();
                        reader.read();
                        time_measurer.log_end(0x14);
                        assert(!reader.is_error());
                        reader.get_param(state);
                        sleep(1s);
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
                if (amplitude_algorithm == PassAlgorithm::inc_linear_by_step)
                    log_string << del << "inc";
                else if (amplitude_algorithm == PassAlgorithm::dec_linear_by_step)
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
                temp << cmd_ptr - cmds.cbegin() << del << (int) if_reacted << del << reaction_time_on_skin_conduction << del << ampl << del << "0x18" << endl;
                cout << temp.str();
                stat << temp.str();
            }
            else
            {} //Another commands don't need to be handled
        }
        else
        {
            cerr<<"error: ";
            cerr<<reader.get_error_string();
            cerr<<endl;
        }
        stat.flush();
    }

    cout<<"end"<<endl;
    }
    catch (std::exception& e)
    {
        cerr << e.what() << endl;
    }
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
