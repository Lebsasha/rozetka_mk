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
#include "protocol_classes.h"

using namespace std;

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

const string del = ", "; //Delimiter between writing to csv file results

//TODO rename enums to noun form
/// Possible hearing test states
enum class HearingState
{
    MeasuringHearingThreshold [[maybe_unused]] = 0, MeasuringReactionTime = 1, SendingResults = 2
};

enum class HearingDynamic {Left=1, Right=0};

enum class DesiredButtonState {StartWaitingForPress=0, StartWaitingForRelease=2};
enum class ActualButtonState {WaitingForPress=0, Pressed=1, WaitingForRelease=2, Released=3};

enum class PassAlgorithm {inc_linear_by_step=0, dec_linear_by_step=1, staircase};

enum class AmplitudeChangingAlgorithm {linear, exp_on_high_linear_at_low};

/// How time step need to be modified on each pass
/// @a random_deviation time step - i. e. when adding or subtracting random_deviation value to given time step
enum class TimeStepChangingAlgorithm {constant, random_deviation};

struct HearingParameters
{
    uint16_t frequency=0;
    uint16_t max_amplitude=0;
    uint16_t initial_amplitude_step=0;
//    uint16_t num_of_steps=0;
    uint16_t time_step=0;
    PassAlgorithm pass_algorithm{};
    AmplitudeChangingAlgorithm amplitude_algorithm{};
    TimeStepChangingAlgorithm time_step_changing_algorithm{};
};

class HearingTester
{
public:
    HearingTester(Command_writer& writer, Command_reader& reader, ofstream& stat);

    void execute(HearingParameters parameters);

private:

    Command_writer& writer;
    Command_reader& reader;
    ostream& stat;

    HearingParameters params;
    enum class PassVariant {IncreasingLinear, DecreasingLinear};
    static const size_t REACTION_SURVEYS_COUNT = 3;

    /// if_upper, amplitude
    vector<pair<bool, uint16_t>> threshold_results{};
    uint16_t increasing_hearing_threshold=0;
    uint16_t decreasing_hearing_threshold=0;
    std::array<uint16_t, REACTION_SURVEYS_COUNT> reaction_times ={0};

    /// Results of measure of one pass
    uint16_t elapsed_time_by_mk=0;
    uint16_t received_amplitude_from_mk=0;
    /// if true, all other current params defined
    bool is_result_received=false;

    void execute_for_one_ear(HearingParameters parameters, HearingDynamic dynamic);

    void make_pass(PassVariant pass_variant, uint16_t first_amplitude, uint16_t amplitude_step);

    void set_pass_parameters(DesiredButtonState state);

    void set_up_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude);

    std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> get_reaction_time(uint16_t amplitude);
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

#define USE_SLEEP
template<typename Rep, typename Period>
inline void sleep(const chrono::duration<Rep, Period>& rtime)
{
#ifdef USE_SLEEP
    this_thread::sleep_for(rtime);
#endif
}

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
        cout << "Time ";
        if (cmd != 0 || !description.empty())
            cout << "for ";
        if (cmd != 0)
        {
            cout << "0x" << std::hex << (int) cmd << std::dec << " command";
            if (!description.empty())
                cout << ", ";
        }
        if (!description.empty())
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
    const uint8_t long_test[] = {0x11, 0x18, 0x11, 0x18, 0x18, 0x11, 0x11, 0x18};
    const uint8_t short_test[] = {0x10, 0x4, 0x11};
//    const auto [cmds, cmds_l] = std::tie(short_test, sizeof_arr(short_test));
    const uint8_t* cmds = short_test;
    const size_t cmds_length = sizeof_arr(short_test);

    cout << "begin " << std::endl;
//    for(size_t i =0;i<4;++i)
    try
    {
    for(const uint8_t* cmd_ptr = cmds; cmd_ptr < cmds + cmds_length; ++cmd_ptr)
    {
        sleep(1s);
        const int cmd = *cmd_ptr;
        if (cmd == 0x11)
        {
            stat << cmd_ptr - cmds << ", "; /// Number of curr command
            hearing_tester.execute({4000, 2500, 10, 300, PassAlgorithm::staircase, AmplitudeChangingAlgorithm::linear, TimeStepChangingAlgorithm::constant});

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
    catch (std::exception& e)
    {
        cerr << e.what() << endl;
    }
}



HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ofstream& _stat): writer(_writer), reader(_reader), stat(_stat)
{}

void HearingTester::execute(HearingParameters parameters)
{
    params = parameters;
    assert(params.pass_algorithm == PassAlgorithm::staircase);

    execute_for_one_ear(params, HearingDynamic::Right);
    sleep(chrono::milliseconds(rand() % 500 + 2000));
//    execute_for_one_ear(params, HearingDynamic::Left);
}

void HearingTester::execute_for_one_ear(HearingParameters parameters, HearingDynamic dynamic)
{
//    assert(_freq < BASE_FREQ/2);
    params = parameters;

    uint8_t cmd = 0x11;
    Time_measurer time_measurer{};
    is_result_received = false;
    threshold_results.clear();

    writer.set_cmd(cmd);
    writer.append_var<uint8_t>( static_cast<uint8_t>(dynamic) );/// Channel //TODO Перевести частоту на 80 кГц
    writer.append_var<uint16_t>(params.frequency);/// array of 1-3 notes
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

        if (params.pass_algorithm == PassAlgorithm::staircase)
        {
            const size_t increasing_pass_count = 3;
            for (size_t i = 0; i < increasing_pass_count; ++i)
            {
                make_pass(PassVariant::IncreasingLinear, 0, params.initial_amplitude_step);//TODO Откуда появляются результаты в 0 здесь?
                if (is_result_received)
                {
                    threshold_results.emplace_back(true, received_amplitude_from_mk);

                    sleep(chrono::milliseconds(rand() % 1000 + 500));
                    if (received_amplitude_from_mk >= 2 * params.initial_amplitude_step)
                        received_amplitude_from_mk -= 2 * params.initial_amplitude_step;
                    else
                        received_amplitude_from_mk = 0;
                    make_pass(PassVariant::IncreasingLinear, received_amplitude_from_mk, 1);
                    if (is_result_received)
                        threshold_results.emplace_back(true, received_amplitude_from_mk);
                }

                sleep(chrono::milliseconds(rand() % 1000 + 1500));
            }

            increasing_hearing_threshold = 0;
            for (auto [is_upper, amplitude]: threshold_results)
            {
                if (is_upper)
                    increasing_hearing_threshold += amplitude;
            }
            if (increasing_hearing_threshold != 0)
                increasing_hearing_threshold /= threshold_results.size();

            const size_t decreasing_pass_count = 3;
            for (size_t i = 0; i < decreasing_pass_count; ++i)
            {
                make_pass(PassVariant::DecreasingLinear, 2*increasing_hearing_threshold,
                          params.initial_amplitude_step);
                if (is_result_received)
                {
                    threshold_results.emplace_back(false, received_amplitude_from_mk);

                    sleep(chrono::milliseconds(rand() % 1000 + 500));
                    make_pass(PassVariant::DecreasingLinear,
                              received_amplitude_from_mk + 4 * params.initial_amplitude_step, 2);
                    if (is_result_received)
                        threshold_results.emplace_back(false, received_amplitude_from_mk);
                }

                sleep(chrono::milliseconds(rand() % 1000 + 1500));
            }

            decreasing_hearing_threshold = 0;
            for (auto [is_upper, amplitude]: threshold_results)
            {
                if (! is_upper)
                   decreasing_hearing_threshold += amplitude;
            }
            if (decreasing_hearing_threshold != 0)
                decreasing_hearing_threshold /= std::count_if(threshold_results.begin(), threshold_results.end(), [&](const auto& item){
                return item.first == false;
        });

        }
        else
            assert(params.pass_algorithm == PassAlgorithm::staircase); // Currently, only staircase algorithm is supported

        ostringstream log_string;
        log_string << "Results for ";
        switch (dynamic)
        {
            case HearingDynamic::Left:
                log_string << "Left";
                break;
            case HearingDynamic::Right:
                log_string << "Right";
                break;
        }
        log_string << " ear" << endl;
        if (! threshold_results.empty()) /// Patient reacted
        {
            sleep(chrono::milliseconds(rand() % 1000 + 2000));
            uint16_t amplitude_for_react_survey = 4 * increasing_hearing_threshold;
            set_pass_parameters(DesiredButtonState::StartWaitingForPress);
            reaction_times = get_reaction_time(amplitude_for_react_survey);

            log_string << "Threshold results:" << endl;
            for (const auto &item: threshold_results)
                log_string << item.first << del << item.second << endl;
            log_string << endl;
            log_string << "Reaction time results:" << endl;
            for (auto it = reaction_times.cbegin(); it < reaction_times.cend(); ++it)
            {
                log_string << *it;
                if (it != reaction_times.cend() - 1)
                    log_string << del;
            }
            log_string << endl;
            log_string << "increasing avg thresh: " << increasing_hearing_threshold << del << "decreasing avg thresh: " << decreasing_hearing_threshold;
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
        switch (params.pass_algorithm)
        {
            case PassAlgorithm::staircase:
                log_string << del << "staircase algorithm";
                break;
            default:
                log_string << del << "unknown algorithm";
        }

        log_string << endl;
        cout << log_string.str();
        stat << log_string.str();
    }
    else
    {
        string err = reader.get_error_string();
        cout << "error occurred when sending command: " << err << endl;
    }
    stat.flush();
}

// Make one amplitude pass
void HearingTester::make_pass(PassVariant pass_variant, uint16_t first_amplitude, uint16_t amplitude_step)
{
    if (pass_variant == PassVariant::DecreasingLinear)
    {
        const uint16_t TONE_INCREASING_TIME = 700;
        const size_t NUM_OF_STEPS = 30;
        uint16_t time_step = TONE_INCREASING_TIME / NUM_OF_STEPS;
        assert(time_step >= 10); // Approximately minimum OS task scheduler time for sleep

        for (size_t i = 1; i <= NUM_OF_STEPS; ++i)
        {
            set_up_new_amplitude_and_receive_threshold_results(i * first_amplitude / NUM_OF_STEPS);
            sleep(chrono::milliseconds(time_step));
        }
    }
    is_result_received = false;
    uint16_t curr_time = 0;
    uint16_t curr_amplitude = first_amplitude;
    uint16_t time_step;
    switch (params.time_step_changing_algorithm)
    {
        case TimeStepChangingAlgorithm::constant:
            time_step = params.time_step;
            break;
        case TimeStepChangingAlgorithm::random_deviation:
            time_step = params.time_step + rand() % 20 - rand() % 10;
            break;
    }
    DesiredButtonState button_state;
    switch (pass_variant)
    {
        case PassVariant::IncreasingLinear:
            button_state = DesiredButtonState::StartWaitingForPress;
            break;
        case PassVariant::DecreasingLinear:
            button_state = DesiredButtonState::StartWaitingForPress;//TODO Replace to Release
            break;
    }
    set_pass_parameters(button_state);
    while (true)
    {
        curr_time += params.time_step;
        switch (params.amplitude_algorithm)
        {
            case AmplitudeChangingAlgorithm::linear:
                break;
            case AmplitudeChangingAlgorithm::exp_on_high_linear_at_low:
                if (curr_amplitude <= 100)
                    amplitude_step = 10;
                else
                    amplitude_step = curr_amplitude / 10;
                break;
        }
        switch (pass_variant)
        {
            case PassVariant::IncreasingLinear:
                curr_amplitude += amplitude_step;
                break;
            case PassVariant::DecreasingLinear: //TODO Handle decreasing step in first pass
                if (curr_amplitude > amplitude_step)
                    curr_amplitude -= amplitude_step;
                else
                    goto end;
                break;
        }
        if (curr_amplitude > params.max_amplitude)
            break;
        set_up_new_amplitude_and_receive_threshold_results(curr_amplitude);
        if (is_result_received)
        {
            if (pass_variant == PassVariant::DecreasingLinear && curr_amplitude >= first_amplitude - 5 * amplitude_step)
            {
                set_pass_parameters(button_state);
                is_result_received = false;
            }
            else
                break;
        }
        sleep(chrono::milliseconds(time_step));
    }
    end:
    set_up_new_amplitude_and_receive_threshold_results(0);
}

/// set parameters for one pass
void HearingTester::set_pass_parameters(DesiredButtonState state)
{
    Time_measurer time_measurer{};
    uint8_t command = 0x12;
    writer.set_cmd(command);
    writer.append_var<uint8_t> ((uint8_t) state);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(command);
    assert(!reader.is_error());
}

void HearingTester::set_up_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude)
{
    Time_measurer time_measurer{};
    uint8_t command = 0x13;
    writer.set_cmd(command);
    writer.append_var<uint16_t>(curr_amplitude);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(command, string("Set new amplitude ").append(to_string(curr_amplitude)));
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
    else if (button_state == ActualButtonState::WaitingForPress || button_state == ActualButtonState::WaitingForRelease)  ///nothing needs to be read, we waiting until patient press or release the button (or when time has gone)
    {}
    else
        assert(false && static_cast<bool>(button_state));
}

std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> HearingTester::get_reaction_time(uint16_t amplitude)
{
    Time_measurer time_measurer{};
    HearingState state;
    do {
        writer.set_cmd(0x14);
        writer.append_var<uint16_t> (amplitude);
        writer.prepare_for_sending();
        time_measurer.begin();
        writer.write();
        reader.read();
        time_measurer.log_end(0x14);
        assert(!reader.is_error());
        state = (HearingState) reader.get_param<uint8_t>();
        sleep(1s);
    } while (state != HearingState::SendingResults); /// do until measure has being ended
    std::array<uint16_t, REACTION_SURVEYS_COUNT> _reaction_times ={0};
    for (auto& react_time: _reaction_times)
    	react_time = reader.get_param<uint16_t>();
    return _reaction_times;
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
