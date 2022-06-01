// This is a personal academic project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: https://pvs-studio.com

#include <chrono>
#include <thread>
#include <numeric>
#include <cmath>
#include <cstring>
#include "lebsasha's_algorithm.h"
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
[[maybe_unused]] auto safe_unsigned_decrease(T& var, size_t amount) -> typename enable_if<is_unsigned_v<T>, T>::type
{
    if (var >= amount)
        var -= amount;
    else
        var = 0;
    return var;
}

template <typename T>
[[maybe_unused]] auto safe_unsigned_increase(T& var, size_t amount) -> typename enable_if<is_unsigned_v<T>, T>::type
{
    if (var + amount <= MAX_VOLUME)
        var += amount;
    else
        var = MAX_VOLUME;
    return var;
}

HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ostream& _stat): writer(_writer), reader(_reader), stat(_stat)
{
    params_internal = {AmplitudeChangingAlgorithm::linear, TimeStepChangingAlgorithm::random_deviation};
    params_internal.previous_same_results_lookback_count = 1;
    params_internal.same_result_threshold = 10;
    params_internal.missed_decreasing_passes_count = 2;
    params_internal.amplitude_change_if_test_wrong = 20;
}

void HearingTester::execute(HearingParameters& parameters)
{

//    sleep(chrono::milliseconds(rand() % 250 + 500));
    execute_for_one_ear(parameters, HearingDynamic::Right);
//    sleep(chrono::milliseconds(rand() % 1000 + 2000));
    execute_for_one_ear(parameters, HearingDynamic::Left);
}

void HearingTester::execute_for_one_ear(HearingParameters& parameters, HearingDynamic dynamic)
{
    check_frequency_validness(parameters.frequency);
    const TickStep* tick_step_ptr = dynamic_cast<const TickStep*>(parameters.initial_amplitude_step);
    if (tick_step_ptr != nullptr)
        assert(tick_step_ptr->get_ticks() < parameters.max_amplitude_in_increasing_pass.get_ticks());
    assert(parameters.time_step <= 5'000);
    assert(parameters.pass_algorithm == PassAlgorithm::staircase);
    assert(params_internal.previous_same_results_lookback_count < parameters.increasing_accurate_pass_count);
//    assert(parameters.frequency < BASE_FREQ/2);
    params = parameters;

    Time_measurer global_time_measurer{};
    Time_measurer time_measurer{};
    const auto is_include_result = [] (const auto& result) { return result.first; };

    /// @section Results of current function is stored in this variables
    double average_increasing_threshold = 0;
    double average_decreasing_threshold = 0;

    /// format of this vectors: <if_precise_assessment, receivedResult>
    std::vector<std::pair<bool, const HearingThresholdResult>> increasing_pass_threshold_results{};
    std::vector<std::pair<bool, const HearingThresholdResult>> decreasing_pass_threshold_results{};
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
        tick_step_ptr = dynamic_cast<const TickStep*>(params.initial_amplitude_step);
        assert(tick_step_ptr != nullptr);
        for (size_t i = 0; i < params.increasing_pass_count; ++i)
        {
            cout << "Running increasing pass test " << i + 1 << " of " << params.increasing_pass_count << endl;
            HearingThresholdResult rough_result =
                    make_pass(PassVariant::IncreasingLinear, params.start_volume, tick_step_ptr);
            if (rough_result.is_result_received)
            {
                increasing_pass_threshold_results.emplace_back(false, rough_result);

                int first_accurate_measure_index = (int) increasing_pass_threshold_results.size() - 1 + 1;
                VolumeLevel start_volume = rough_result.threshold;
                start_volume.subtract(params.initial_amplitude_step);
                for (size_t j = 0; j < params.increasing_accurate_pass_count; ++j)
                {
                    sleep(chrono::milliseconds(rand() % 1500 + 500));
                    if (j >= params_internal.previous_same_results_lookback_count &&
                    std::count_if(increasing_pass_threshold_results.begin() + first_accurate_measure_index, increasing_pass_threshold_results.end(), [&](const auto& item){
                        return item.first;
                    }) >= params_internal.previous_same_results_lookback_count )  // If we already have at least lookback_count accurate results in vector after last rough measure
                    {
                        size_t same_results_count = std::count_if(increasing_pass_threshold_results.begin() + first_accurate_measure_index, increasing_pass_threshold_results.end(),
                                                                  [&](const auto& item) {
                                                              assert(item.first);
                                                              if (item.second.threshold.get_ticks() - start_volume.get_ticks() <= params_internal.same_result_threshold)
                                                                  return true;
                                                              return false;
                                                          });
                        if (same_results_count >= params_internal.previous_same_results_lookback_count && start_volume.get_ticks() >= params_internal.amplitude_change_if_test_wrong)
                        {
                            cout << "Warning! Changing start volume from " << start_volume.get_ticks();
                            start_volume.subtract_ticks(params_internal.amplitude_change_if_test_wrong); // We can safely decrease, as we check it safety in the 'if' condition
                            cout << " to " << start_volume.get_ticks() << ", because " << params_internal.previous_same_results_lookback_count
                                 << " last patient' responses are located near start volume" << endl;
                            j = 0;
                            first_accurate_measure_index = (int) increasing_pass_threshold_results.size() - 1 + 1;
                        }
                    }
                    TickStep unity_step = TickStep(1);
                    HearingThresholdResult accurate_result = make_pass(PassVariant::IncreasingLinear, start_volume, &unity_step);
                    if (accurate_result.is_result_received)
                        increasing_pass_threshold_results.emplace_back(true, accurate_result);
                }
            }

            sleep(chrono::milliseconds(rand() % 2000 + 1000));
        }

        size_t acceptable_results_count = std::count_if(increasing_pass_threshold_results.begin(), increasing_pass_threshold_results.end(), is_include_result);
        if (acceptable_results_count != 0)
            average_increasing_threshold =
                    std::accumulate(increasing_pass_threshold_results.begin(), increasing_pass_threshold_results.end(), 0.0,
                                    [&](const double a, const auto& p) {
                                        if (is_include_result(p))
                                            return a + p.second.threshold.get_ticks();
                                        else
                                            return a;
                                    }) / (double) acceptable_results_count;
        else
            average_increasing_threshold = 0;

        for (size_t i = 0; i < params.decreasing_pass_count; ++i)
        {
            cout << "Running decreasing pass test " << i + 1 << " of " << params.decreasing_pass_count << endl;
            VolumeLevel initial_volume_in_decreasing_pass = VolumeLevel().set_ticks((uint16_t) round(2 * average_increasing_threshold));
            HearingThresholdResult rough_result =
                    make_pass(PassVariant::DecreasingLinear, initial_volume_in_decreasing_pass, tick_step_ptr);
            if (rough_result.is_result_received)
            {
                decreasing_pass_threshold_results.emplace_back(false, rough_result);

                size_t first_accurate_measure_index = decreasing_pass_threshold_results.size() - 1 + 1;
                VolumeLevel start_amplitude = rough_result.threshold;
                start_amplitude.subtract(params.initial_amplitude_step);
                for (size_t j = 0; j < params.decreasing_accurate_pass_count; ++j)
                {
                    sleep(chrono::milliseconds(rand() % 1500 + 500));
                    if (j >= params_internal.previous_same_results_lookback_count)
                    {
                        size_t results_count = decreasing_pass_threshold_results.size() - first_accurate_measure_index;
                        if (j - results_count >= params_internal.missed_decreasing_passes_count) // || same_results_count >= previous_same_results_lookback_count)
                        {
                            cout << "Warning! Changing start volume from " << start_amplitude.get_ticks();
                            start_amplitude.add_ticks(params_internal.amplitude_change_if_test_wrong);
                            cout << " to " << start_amplitude.get_ticks() << ", because ";
                            /*else*/ if (j - results_count >= params_internal.missed_decreasing_passes_count)
                                cout << "patient doesn't hear tone";
                            cout << endl;
                            j = 0;
                        }
                    }
                    TickStep accurate_step = TickStep(3);
                    HearingThresholdResult accurate_result = make_pass(PassVariant::DecreasingLinear, start_amplitude, &accurate_step);
                    if (accurate_result.is_result_received)
                        decreasing_pass_threshold_results.emplace_back(true, accurate_result);
                }
            }

            sleep(chrono::milliseconds(rand() % 2000 + 1000));
        }

        acceptable_results_count = std::count_if(decreasing_pass_threshold_results.begin(), decreasing_pass_threshold_results.end(), is_include_result);
        if (acceptable_results_count != 0)
            average_decreasing_threshold =
                    std::accumulate(decreasing_pass_threshold_results.begin(), decreasing_pass_threshold_results.end(), 0.0,
                                    [&](const double a, const auto& p) {
                                        if (is_include_result(p))
                                            return a + p.second.threshold.get_ticks();
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
    log_string << " ear" ;
    log_string << ", with frequency " << params.frequency;
    log_string << endl;
    if (!increasing_pass_threshold_results.empty() || !decreasing_pass_threshold_results.empty()) /// Patient reacted at least in one pass
    {
//        sleep(chrono::milliseconds(rand() % 1000 + 750));
        uint16_t amplitude_for_react_survey = static_cast<uint16_t>(round(4 * average_increasing_threshold));
//        reaction_times = get_reaction_time(amplitude_for_react_survey);//TODO Make as another survey
        stop_current_measure();
//TODO Сделать плавное нарастание громкости при измерении времени реакции
        log_string.precision(4);
        log_string << "Is accurate result" << del << "Threshold (linear)" << del << "Threshold (dB)" << del << "Elapsed time by MCU" << del << "Elapsed time by PC" << endl;
        log_string << "Up threshold results:" << endl;
        for (const auto &item: increasing_pass_threshold_results)
            log_string << item.first << del << item.second.threshold.get_ticks() << del << item.second.threshold.get_dB() << del << item.second.elapsed_time << del << item.second.elapsed_time_on_PC << endl;
        if (!decreasing_pass_threshold_results.empty())
        {
            log_string << "Decreasing threshold results:" << endl;
            for (const auto &item : decreasing_pass_threshold_results)
                log_string << item.first << del << item.second.threshold.get_ticks() << del << item.second.threshold.get_dB() << del << item.second.elapsed_time << del << item.second.elapsed_time_on_PC << endl;
        }
        if (reaction_times[0] != 0)
        {
            log_string << "Reaction time results:" << endl;
            for (auto iter = reaction_times.begin(); iter < reaction_times.end(); ++iter)
            {
                log_string << *iter;
                if (iter != reaction_times.end() - 1)
                    log_string << del;
            }
        }
        log_string << endl;

        vector<uint16_t> increasing_thresholds;
        l_std::transform_if(increasing_pass_threshold_results.begin(),  increasing_pass_threshold_results.end(),
                            std::back_inserter(increasing_thresholds), [](const auto& result) {return result.second.threshold.get_ticks();}, is_include_result);

        vector<uint16_t> decreasing_thresholds;
        l_std::transform_if(decreasing_pass_threshold_results.begin(),  decreasing_pass_threshold_results.end(),
                            std::back_inserter(decreasing_thresholds), [](const auto& result) {return result.second.threshold.get_ticks();}, is_include_result);

//        assert(average_increasing_threshold - stat_aggregator::get_mean(increasing_thresholds.begin(), increasing_thresholds.end()));
//        assert(average_decreasing_threshold - stat_aggregator::get_mean(decreasing_thresholds.begin(), decreasing_thresholds.end()));
//        average_increasing_threshold - mean already calculated before
        double increasing_threshold_std = stat_aggregator::get_std(increasing_thresholds.begin(), increasing_thresholds.end(), average_increasing_threshold);

        double decreasing_threshold_std = stat_aggregator::get_std(decreasing_thresholds.begin(),  decreasing_thresholds.end(), average_decreasing_threshold);

        double increasing_conf_interval = stat_aggregator::get_confidence_interval(increasing_threshold_std, increasing_thresholds.size(), 0.95);
        double decreasing_conf_interval = stat_aggregator::get_confidence_interval(decreasing_threshold_std, decreasing_thresholds.size(), 0.95);

        cout << log_string.precision(2 + 2); // in this case in format xx.xx or x.xxx
        log_string << "increasing accurate results' threshold mean: " << average_increasing_threshold << " +- " << increasing_conf_interval << " (with std: " << increasing_threshold_std << ")";
        if (! decreasing_pass_threshold_results.empty())
        log_string << del << "decreasing accurate results' threshold mean:" << average_decreasing_threshold << " +- " << decreasing_conf_interval << " (with std: " << decreasing_threshold_std << ")";
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
HearingThresholdResult HearingTester::make_pass(const PassVariant pass_variant, const VolumeLevel start_volume, const TickStep* amplitude_step)
{
    if (pass_variant == PassVariant::DecreasingLinear)
    {
        const uint16_t TONE_INCREASING_TIME = 700;
        const size_t NUM_OF_STEPS = 30;
        const uint16_t time_step = TONE_INCREASING_TIME / NUM_OF_STEPS;
        static_assert(time_step >= 10); // Approximately minimum OS task scheduler time for sleep

        for (size_t i = 1; i <= NUM_OF_STEPS; ++i)
        {
            set_up_new_amplitude_and_receive_threshold_results(i * start_volume.get_ticks() / NUM_OF_STEPS);
            sleep(chrono::milliseconds(time_step));
        }
    }
    HearingThresholdResult pass_result{};
    uint16_t curr_time = 0;
    VolumeLevel curr_amplitude = start_volume;
    uint16_t time_step;
    switch (params_internal.time_step_changing_algorithm)
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
        switch (params_internal.amplitude_algorithm)
        {
            case AmplitudeChangingAlgorithm::linear:
                break;
            case AmplitudeChangingAlgorithm::exp_on_high_linear_at_low:
                assert(false && (bool) AmplitudeChangingAlgorithm::exp_on_high_linear_at_low);
//                if (curr_amplitude <= 100)
//                    amplitude_step = 10;
//                else
//                    amplitude_step = curr_amplitude / 10;
                break;
        }
        switch (pass_variant)
        {
            case PassVariant::IncreasingLinear:
                curr_amplitude.add(amplitude_step);
                if (curr_amplitude > params.max_amplitude_in_increasing_pass)
                    goto end;
                break;
            case PassVariant::DecreasingLinear:
                if (! curr_amplitude.subtract(amplitude_step))
                    goto end;
                break;
        }
        HearingThresholdResult result = set_up_new_amplitude_and_receive_threshold_results(curr_amplitude.get_ticks());
        if (result.is_result_received)
        {
            if (pass_variant == PassVariant::DecreasingLinear && curr_amplitude.get_ticks() >= start_volume.get_ticks() - 3 * amplitude_step->get_ticks())
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
        curr_time += time_step;
    }
    end:
    if (! pass_result.is_result_received)
        pass_result = set_up_new_amplitude_and_receive_threshold_results(0);// If result not received, try to get results at last time
    else
        set_up_new_amplitude_and_receive_threshold_results(0);
    pass_result.elapsed_time_on_PC = curr_time;
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

HearingThresholdResult HearingTester::set_up_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude)
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
        bool is_result_received = true;
        uint16_t elapsed_time_by_device = reader.get_param<uint16_t>();
        uint16_t received_amplitude_from_device = reader.get_param<uint16_t>();
        VolumeLevel volume_level = VolumeLevel().set_ticks(received_amplitude_from_device);
        return HearingThresholdResult{is_result_received, elapsed_time_by_device, volume_level};
    }
    else if (button_state == ActualButtonState::WaitingForPress || button_state == ActualButtonState::WaitingForRelease)  ///nothing needs to be read, we waiting until patient press or release the button (or when time has gone)
    {
        return HearingThresholdResult{false, 0, VolumeLevel().set_ticks(0)};
    }
    else
    {
        assert(false && static_cast<bool>(button_state));
        return HearingThresholdResult{false, 0, VolumeLevel().set_ticks(0)};
    }
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

void HearingTester::stop_current_measure()
{
    writer.set_cmd(0x4);
    writer.prepare_for_sending();
    writer.write();
    reader.read();
    assert(!reader.is_error() && reader.is_empty());
}

[[maybe_unused]] void HearingTester::set_tone_once(HearingDynamic dynamic, uint16_t frequency, uint16_t volume)
{
    check_frequency_validness(frequency);

    uint8_t cmd = 0x10;
    writer.set_cmd(cmd);
    writer.append_var<uint8_t>((uint8_t) dynamic);
    writer.append_var<uint16_t>(volume);
    writer.append_var<uint16_t>(frequency);
    writer.prepare_for_sending();
    writer.write();
    reader.read();
    if (reader.is_error())
    {
        cerr <<"error occurred while executing " << print_cmd(cmd) << " command: '" << reader.get_error_string() << '\'' << endl;
    }
}

void HearingTester::check_frequency_validness(uint16_t frequency)
{
    assert(frequency != 0);
    assert(frequency <= 20'000);
}

[[maybe_unused]] DbStep::DbStep(double _dB): dB(_dB)
{}

double DbStep::get_dB() const
{
    return dB;
}

[[maybe_unused]] TickStep::TickStep(uint16_t _ticks): ticks(_ticks)
{}

uint16_t TickStep::get_ticks() const
{
    return ticks;
}

//VolumeLevel::VolumeLevel(): ticks(0), dB(0)
//{}

VolumeLevel& VolumeLevel::set_ticks(uint16_t _ticks)
{
    if (_ticks <= VolumeLevel::MAX_VOLUME)
    {
        ticks = _ticks;
        dB = convert_to_dB(ticks);
    }
    else
    {
        cerr << "Warning! In " << __PRETTY_FUNCTION__ << " Attempt to set ticks =" << _ticks
             << " that greater than maximum possible volume " << VolumeLevel::MAX_VOLUME << ". Setting ticks = 0" << endl;
        ticks = 0;
        dB = convert_to_dB(ticks);
    }
    return *this;
}

[[maybe_unused]] VolumeLevel& VolumeLevel::set_dB(double _dB)
{
    assert(_dB >= 0);
    ticks = convert_to_ticks(_dB);
    assert(ticks <= VolumeLevel::MAX_VOLUME);
    this->dB = _dB;
    return *this;
}

bool VolumeLevel::add_dB(double amount)
{
    uint16_t new_ticks = convert_to_ticks(dB + amount);
    if (new_ticks <= VolumeLevel::MAX_VOLUME)
    {
        if (ticks == new_ticks)
            cerr << "Warning! In " << __PRETTY_FUNCTION__ << " Current tick (" << ticks << ") and new tick (" << new_ticks << ") "
                                                                                                                              "calculated for " << dB << " + " << amount << "dB are the same." << endl;
        ticks = new_ticks;
        dB += amount;
        return true;
    }
    // else
    return false;
}

bool VolumeLevel::add_ticks(uint16_t amount)
{
    uint16_t new_ticks = ticks + amount;
    if (new_ticks <= VolumeLevel::MAX_VOLUME)
    {
        ticks = new_ticks;
        dB = convert_to_dB(ticks);
        return true;
    }
    // else
    return false;
}

bool VolumeLevel::add(const VolumeStep* vs)
{
    if (dynamic_cast<const TickStep*>(vs) != nullptr)
    {
        auto ticks_ptr = dynamic_cast<const TickStep*>(vs);
        return add_ticks(ticks_ptr->get_ticks());
    }
    else if (dynamic_cast<const DbStep*>(vs) != nullptr)
    {
        auto dB_ptr = dynamic_cast<const DbStep*>(vs);
        return add_dB(dB_ptr->get_dB());
    }
    cerr << "Warning! In " << __PRETTY_FUNCTION__
         << " Unknown cast for volumeStep pointer provided to me! Not incrementing any number and returning false";
    return false;
}

bool VolumeLevel::subtract_dB(double amount)
{
    if (dB - amount < 0)
        return false;
    uint16_t new_ticks = convert_to_ticks(dB - amount);
    if(new_ticks <= VolumeLevel::MAX_VOLUME)
    {
        if (ticks == new_ticks)
            cerr << "Warning! In " << __PRETTY_FUNCTION__ << " Current tick (" << ticks << ") and new tick (" << new_ticks << ") "
                                                                                                                              "calculated for " << dB << " + " << amount << "dB are the same." << endl;
        ticks = new_ticks;
        dB -= amount;
        return true;
    }
    // else
    return false;
}

bool VolumeLevel::subtract_ticks(uint16_t amount)
{
    uint16_t new_ticks = ticks - amount;
    if (new_ticks <= VolumeLevel::MAX_VOLUME)
    {
        ticks = new_ticks;
        dB = convert_to_dB(ticks);
        return true;
    }
    // else
    // in case of underflow
    return false;
}

bool VolumeLevel::subtract(const VolumeStep* vs)
{
    if (dynamic_cast<const TickStep*>(vs) != nullptr)
    {
        auto ticks_ptr = dynamic_cast<const TickStep*>(vs);
        return subtract_ticks(ticks_ptr->get_ticks());
    }
    else if (dynamic_cast<const DbStep*>(vs) != nullptr)
    {
        auto dB_ptr = dynamic_cast<const DbStep*>(vs);
        return subtract_dB(dB_ptr->get_dB());
    }
    cerr << "Warning! In " << __PRETTY_FUNCTION__
         << " Unknown cast for volumeStep pointer provided to me! Not decrementing any number and returning false";
    return false;
}

uint16_t VolumeLevel::get_ticks() const
{
    return ticks;
}

double VolumeLevel::get_dB() const
{
    return dB;
}

double VolumeLevel::convert_to_dB(uint16_t ticks)
{
    return 20 * log10(ticks / TICKS_FOR_0dB);
}

uint16_t VolumeLevel::convert_to_ticks(double dB)
{
    return static_cast<uint16_t>(round(pow(10, dB/20) * TICKS_FOR_0dB));
}

bool VolumeLevel::operator<(const VolumeLevel& rhs) const
{
    if (ticks < rhs.ticks)
        return true;
    if (rhs.ticks < ticks)
        return false;
    cerr << "Warning! In " << __PRETTY_FUNCTION__ << " There are equal ticks ==" << ticks << " Returning false" << endl;
    return false;
//    return dB < rhs.dB;
}

bool VolumeLevel::operator>(const VolumeLevel& rhs) const
{
    return rhs < *this;
}

bool VolumeLevel::operator<=(const VolumeLevel& rhs) const
{
    return !(rhs < *this);
}

bool VolumeLevel::operator>=(const VolumeLevel& rhs) const
{
    return !(*this < rhs);
}
