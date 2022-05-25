#include <chrono>
#include <thread>
#include <numeric>
#include <cmath>
#include "general.h"
#include "time_measurer.h"
#include "hearing_tester.h"
#include "stat_aggregator.h"

using namespace std;


template<typename Rep, typename Period>
static inline void sleep(const chrono::duration<Rep, Period>& rtime)
{
#ifdef USE_SLEEP_IN_HEARING_TEST
    this_thread::sleep_for(rtime);
#endif
}

template <typename T>
auto safe_unsigned_decrease(T& var, size_t amount) -> typename enable_if<is_unsigned_v<T>, T>::type
{
    if (var >= amount)
        var -= amount;
    else
        var = 0;
    return var;
}

template <typename T>
auto safe_unsigned_increase(T& var, size_t amount) -> typename enable_if<is_unsigned_v<T>, T>::type
{
    if (var + amount <= MAX_VOLUME)
        var += amount;
    else
        var = MAX_VOLUME;
    return var;
}

HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ostream& _stat): writer(_writer), reader(_reader), stat(_stat)
{}

void HearingTester::execute(const HearingParameters parameters)
{
    assert(parameters.frequency <= 20'000);
    assert(parameters.max_amplitude_in_increasing_pass <= MAX_VOLUME);
    assert(parameters.initial_amplitude_step < parameters.max_amplitude_in_increasing_pass);
    assert(parameters.time_step <= 5'000);
    assert(parameters.pass_algorithm == PassAlgorithm::staircase);
    params = parameters;

    sleep(chrono::milliseconds(rand() % 250 + 500));
    execute_for_one_ear(params, HearingDynamic::Right);
    sleep(chrono::milliseconds(rand() % 1000 + 2000));
    execute_for_one_ear(params, HearingDynamic::Left);
}

void HearingTester::execute_for_one_ear(const HearingParameters parameters, const HearingDynamic dynamic)
{
//    assert(_freq < BASE_FREQ/2);
    params = parameters;

    Time_measurer global_time_measurer{};
    Time_measurer time_measurer{};
    const auto is_include_result = [] (const auto& result) { return ! result.first; };

    /// @section Results of current function is stored in this variables
    double average_increasing_threshold = 0;
    double average_decreasing_threshold = 0;

    /// format of this vectors: <if_precise_assessment, receivedResult>
    std::vector<std::pair<bool, HearingThresholdResult>> up_threshold_results{};
    std::vector<std::pair<bool, HearingThresholdResult>> down_threshold_results{};
    std::array<uint16_t, REACTION_SURVEYS_COUNT> reaction_times = {0};
    /// @endsection

    uint8_t cmd = 0x11;
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

    if (reader.is_error())
    {
        string err = reader.get_error_string();
        cout << "error occurred when sending command " << print_cmd(cmd) << ": '" << err << '\'' << endl;
        return;
    }

    assert(reader.is_empty());

    if (params.pass_algorithm == PassAlgorithm::staircase)
    {
        const size_t increasing_pass_count = 6;
        const size_t increasing_accurate_pass_count = 0;
        const size_t decreasing_pass_count = 0;
        const size_t decreasing_accurate_pass_count = 3;

        const size_t previous_same_results_lookback_count = 2;
        const uint16_t same_result_threshold = 10;
        /// If patient miss more than \a missed_decreasing_passes_count, than we need to increase start_volume for decreasing pass
        const size_t missed_decreasing_passes_count = 2;
        const uint16_t amplitude_change_if_test_wrong = 20;

        for (size_t i = 0; i < increasing_pass_count; ++i)
        {
            cout << "Running increasing pass test " << i << " of " << increasing_pass_count << endl;
            HearingThresholdResult rough_result =
                    make_pass(PassVariant::IncreasingLinear, 0, params.initial_amplitude_step);
            if (rough_result.is_result_received)
            {
                up_threshold_results.emplace_back(false, rough_result);

                int first_accurate_measure_index = (int) up_threshold_results.size() - 1 + 1;
                uint16_t start_amplitude = rough_result.threshold;
                safe_unsigned_decrease(start_amplitude, 2 * params.initial_amplitude_step);
                for (size_t j = 0; j < increasing_accurate_pass_count; ++j)
                {
                    sleep(chrono::milliseconds(rand() % 1500 + 500));
                    if (j >= previous_same_results_lookback_count &&
                    std::count_if(up_threshold_results.begin() + first_accurate_measure_index, up_threshold_results.end(), [&](const auto& item){
                        return item.first;
                    }) >= previous_same_results_lookback_count )  // If we already have at least lookback_count accurate results in vector after last rough measure
                    {
                        size_t same_results_count = std::count_if(up_threshold_results.begin() + first_accurate_measure_index, up_threshold_results.end(),
                                                          [&](const auto& item) {
                                                              assert(item.first);
                                                              if (item.second.threshold - start_amplitude <= same_result_threshold)
                                                                  return true;
                                                              return false;
                                                          });
                        if (same_results_count >= previous_same_results_lookback_count && start_amplitude >= amplitude_change_if_test_wrong)
                        {
                            cout << "Warning! Changing start volume from " << start_amplitude;
                            start_amplitude -= amplitude_change_if_test_wrong; // We can safely decrease, as we check it safety in the 'if' condition
                            cout << " to " << start_amplitude << ", because " << previous_same_results_lookback_count
                                 << " last patient' responses are located near start volume" << endl;
                            j = 0;
                        }
                    }
                    HearingThresholdResult accurate_result = make_pass(PassVariant::IncreasingLinear, start_amplitude, 1);
                    if (accurate_result.is_result_received)
                        up_threshold_results.emplace_back(true, accurate_result);
                }
            }

            sleep(chrono::milliseconds(rand() % 2000 + 1000));
        }

        size_t acceptable_results_count = std::count_if(up_threshold_results.begin(), up_threshold_results.end(), is_include_result);
        if (acceptable_results_count != 0)
            average_increasing_threshold =
                    std::accumulate(up_threshold_results.begin(), up_threshold_results.end(), 0.0,
                                    [&](const double a, const auto& p) {
                                        if (is_include_result(p))
                                            return a + p.second.threshold;
                                        else
                                            return a;
                                    }) / (double) acceptable_results_count;
        else
            average_increasing_threshold = 0;

        for (size_t i = 0; i < decreasing_pass_count; ++i)
        {
            uint16_t initial_amplitude_in_decreasing_pass = (uint16_t) (round(2 * average_increasing_threshold));
            HearingThresholdResult rough_result =
                    make_pass(PassVariant::DecreasingLinear, initial_amplitude_in_decreasing_pass, params.initial_amplitude_step);
            if (rough_result.is_result_received)
            {
                down_threshold_results.emplace_back(false, rough_result);

                int first_accurate_measure_index = (int) down_threshold_results.size() - 1 + 1;
                uint16_t start_amplitude = rough_result.threshold;
                safe_unsigned_increase(start_amplitude, 4 * params.initial_amplitude_step);
                for (size_t j = 0; j < decreasing_accurate_pass_count; ++j)
                {
                    sleep(chrono::milliseconds(rand() % 1500 + 500));
                    if (j >= previous_same_results_lookback_count)
                    {
                        size_t results_count = down_threshold_results.size() - first_accurate_measure_index;
                        if (j - results_count >= missed_decreasing_passes_count) // || same_results_count >= previous_same_results_lookback_count)
                        {
                            cout << "Warning! Changing start volume from " << start_amplitude;
                            safe_unsigned_increase(start_amplitude, amplitude_change_if_test_wrong);
                            cout << " to " << start_amplitude << ", because ";
                            /*else*/ if (j - results_count >= missed_decreasing_passes_count)
                                cout << "patient doesn't hear tone";
                            cout << endl;
                            j = 0;
                        }
                    }
                    HearingThresholdResult accurate_result = make_pass(PassVariant::DecreasingLinear, start_amplitude, 3);
                    if (accurate_result.is_result_received)
                        down_threshold_results.emplace_back(true, accurate_result);
                }
            }

            sleep(chrono::milliseconds(rand() % 2000 + 1000));
        }

        acceptable_results_count = std::count_if(down_threshold_results.begin(), down_threshold_results.end(), is_include_result);
        if (acceptable_results_count != 0)
            average_decreasing_threshold =
                    std::accumulate(down_threshold_results.begin(), down_threshold_results.end(), 0.0,
                                    [&](const double a, const auto& p) {
                                        if (is_include_result(p))
                                            return a + p.second.threshold;
                                        else
                                            return a;
                                    }) / (double) acceptable_results_count;
        else
            average_decreasing_threshold = 0;

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
    if (!up_threshold_results.empty() || !down_threshold_results.empty()) /// Patient reacted at least in one pass
    {
        sleep(chrono::milliseconds(rand() % 1000 + 750));
//        stop_current_measure();
        uint16_t amplitude_for_react_survey = static_cast<uint16_t>(round(4 * average_increasing_threshold));
//        reaction_times = get_reaction_time(amplitude_for_react_survey);//TODO Make as another survey
        stop_current_measure();
//TODO Сделать плавное нарастание громкости при измерении амплитуды
        log_string << "Up threshold results:" << endl;
        for (const auto &item: up_threshold_results)
            log_string << item.first << del << item.second.threshold << endl;
        log_string << "Down threshold results:" << endl;
        for (const auto &item : down_threshold_results)
            log_string << item.first << del << item.second.threshold << endl;
        log_string << "Reaction time results:" << endl;
        for (auto iter = reaction_times.begin(); iter < reaction_times.end(); ++iter)
        {
            log_string << *iter;
            if (iter != reaction_times.end() - 1)
                log_string << del;
        }
        log_string << endl;

        vector<uint16_t> increasing_thresholds;
        for (const auto &item : up_threshold_results)
            if (is_include_result(item))
                increasing_thresholds.push_back(item.second.threshold);

        vector<uint16_t> decreasing_thresholds;
        for (const auto &item : down_threshold_results)
            if (is_include_result(item))
                decreasing_thresholds.push_back(item.second.threshold);

        auto temp = stat_aggregator::get_std(increasing_thresholds.begin(), increasing_thresholds.end(), average_increasing_threshold);
//           average_increasing_threshold - mean already calculated before
        double increasing_threshold_std = 0.0;
        std::for_each(increasing_thresholds.begin(),  increasing_thresholds.end(), [&](const uint16_t d) {
            increasing_threshold_std += (d - average_increasing_threshold) * (d - average_increasing_threshold);
        });
        if (! increasing_thresholds.empty())
            increasing_threshold_std = sqrt(increasing_threshold_std / static_cast<double>(increasing_thresholds.size() - 1));
        else
            increasing_threshold_std = std::numeric_limits<double>::quiet_NaN();

        double decreasing_threshold_std = 0.0;
        std::for_each(decreasing_thresholds.begin(),  decreasing_thresholds.end(), [&](const double d) {
            decreasing_threshold_std += (d - average_decreasing_threshold) * (d - average_decreasing_threshold);
        });
        if (! decreasing_thresholds.empty())
            decreasing_threshold_std = sqrt(decreasing_threshold_std / static_cast<double>(decreasing_thresholds.size() - 1));
        else
            decreasing_threshold_std = std::numeric_limits<double>::quiet_NaN();
        double increasing_conf_interval = stat_aggregator::get_confidence_interval(increasing_threshold_std, increasing_thresholds.size(), 0.95);
        double decreasing_conf_interval = stat_aggregator::get_confidence_interval(decreasing_threshold_std, decreasing_thresholds.size(), 0.95);
        cout << log_string.precision(2 + 2);
        log_string << "increasing avg threshold: " << average_increasing_threshold << " +- " << increasing_conf_interval << " (with std: " << increasing_threshold_std << ")" << del <<
                "decreasing avg threshold: " << average_decreasing_threshold << " +- " << decreasing_conf_interval << " (with std: " << decreasing_threshold_std << ")";
    }
    else /// Patient never reacted, need to stop measure
    {
        stop_current_measure();
        log_string << "Patient isn't reacted";
    }
    log_string << del << "algorithm: ";
    switch (params.pass_algorithm)
    {
        case PassAlgorithm::staircase:
            log_string << "staircase";
            break;
        default:
            log_string << "unknown";
    }
    log_string << endl;

    log_string << endl;
    cout << log_string.str();
    stat << log_string.str();
    stat.flush();
    global_time_measurer.log_end(0, "run test for one ear");
}

// Make one amplitude pass
HearingThresholdResult HearingTester::make_pass(const PassVariant pass_variant, const uint16_t start_amplitude, uint16_t amplitude_step)
{
    if (pass_variant == PassVariant::DecreasingLinear)
    {
        const uint16_t TONE_INCREASING_TIME = 700;
        const size_t NUM_OF_STEPS = 30;
        const uint16_t time_step = TONE_INCREASING_TIME / NUM_OF_STEPS;
        static_assert(time_step >= 10); // Approximately minimum OS task scheduler time for sleep

        for (size_t i = 1; i <= NUM_OF_STEPS; ++i)
        {
            set_up_new_amplitude_and_receive_threshold_results(i * start_amplitude / NUM_OF_STEPS);
            sleep(chrono::milliseconds(time_step));
        }
    }
    HearingThresholdResult pass_result{};
    uint16_t curr_time = 0;
    uint16_t curr_amplitude = start_amplitude;
    uint16_t time_step;
    switch (params.time_step_changing_algorithm)
    {
        case TimeStepChangingAlgorithm::constant:
            time_step = params.time_step;
            break;
        case TimeStepChangingAlgorithm::random_deviation:
            const uint16_t divider = params.time_step / 10;
            time_step = params.time_step + rand() % divider - rand() % divider;
            cout << "In pass: Setting " << time_step << " time step" << endl;
            break;
    }
    DesiredButtonState button_state;
    switch (pass_variant)
    {
        case PassVariant::IncreasingLinear: // NOLINT(bugprone-branch-clone)
            button_state = DesiredButtonState::StartWaitingForPress;
            break;
        case PassVariant::DecreasingLinear:
            button_state = DesiredButtonState::StartWaitingForPress;
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
                if (curr_amplitude > params.max_amplitude_in_increasing_pass)
                    goto end;
                break;
            case PassVariant::DecreasingLinear:
                if (curr_amplitude > amplitude_step)
                    curr_amplitude -= amplitude_step;
                else
                    goto end;
                break;
        }
        HearingThresholdResult result = set_up_new_amplitude_and_receive_threshold_results(curr_amplitude);
        if (result.is_result_received)
        {
            if (pass_variant == PassVariant::DecreasingLinear && curr_amplitude >= start_amplitude - 3 * amplitude_step)
            {
                reset_current_result_on_device(button_state);
            }
            else
            {
                pass_result = result;
//                goto end;
                break;
            }
        }
        sleep(chrono::milliseconds(time_step));
    }
    end:
    if (! pass_result.is_result_received)
        pass_result = set_up_new_amplitude_and_receive_threshold_results(0);// If result not received, try to get results at last time
    else
        set_up_new_amplitude_and_receive_threshold_results(0);
    return pass_result;
}

/// set parameters for one pass
void HearingTester::set_pass_parameters(const DesiredButtonState state)
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

void HearingTester::reset_current_result_on_device(const DesiredButtonState state)
{
    Time_measurer time_measurer{};
    uint8_t command = 0x12;
    writer.set_cmd(command);
    writer.append_var<uint8_t> ((uint8_t) state);
    writer.prepare_for_sending();
    time_measurer.begin();
    writer.write();
    reader.read();
    time_measurer.log_end(command, "previous result was cleared");
    assert(!reader.is_error());
}

HearingThresholdResult HearingTester::set_up_new_amplitude_and_receive_threshold_results(const uint16_t curr_amplitude)
{
    assert(curr_amplitude <= MAX_VOLUME);
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
        bool is_result_received = true;
        uint16_t elapsed_time_by_device = reader.get_param<uint16_t>();
        uint16_t received_amplitude_from_device = reader.get_param<uint16_t>();
        return HearingThresholdResult{is_result_received, elapsed_time_by_device, received_amplitude_from_device};
    }
    else if (button_state == ActualButtonState::WaitingForPress || button_state == ActualButtonState::WaitingForRelease)  ///nothing needs to be read, we waiting until patient press or release the button (or when time has gone)
    {
        return HearingThresholdResult{false, 0, 0};
    }
    else
        assert(false && static_cast<bool>(button_state));
}

std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> HearingTester::get_reaction_time(const uint16_t amplitude)
{
    assert(amplitude <= MAX_VOLUME);
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

void HearingTester::stop_current_measure()
{
    writer.set_cmd(0x4);
    writer.prepare_for_sending();
    writer.write();
    reader.read();
    assert(!reader.is_error() && reader.is_empty());
}

