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

ostream& operator<<(ostream& os, const PassVariant& enum_member)
{
    const char* str;
    switch(enum_member){
#define PROCESS_VAL(p) case(p): str = #p; break;
        PROCESS_VAL(PassVariant::Increasing);
        PROCESS_VAL(PassVariant::Decreasing);
#undef PROCESS_VAL
    }
    return os << str;
}

ostream& operator<<(ostream& os, const HearingDynamic& enum_member)
{
    const char* str;
    switch(enum_member)
    {
        case (HearingDynamic::Left):
            str = "Left";
            break;
        case (HearingDynamic::Right):
            str = "Right";
            break;
    }
    return os << str;
}

HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ostream& _stat): writer(_writer), reader(_reader), stat(_stat)
{
    params_internal = {TimeStepChangingAlgorithm::random_deviation};
    params_internal.previous_same_results_lookback_count = 1;
    params_internal.same_result_threshold = 10;
    params_internal.missed_decreasing_passes_count = 2;
    params_internal.amplitude_change_if_test_wrong = 20;
}

void HearingTester::execute(HearingParameters& parameters)
{
    bool dynamic = rand() % 2;
//    sleep(chrono::milliseconds(rand() % 250 + 500));
//    execute_for_one_ear(parameters, static_cast<HearingDynamic>(dynamic));
    execute_for_one_ear(parameters, HearingDynamic::Right);
//    sleep(chrono::milliseconds(rand() % 1000 + 2000));
    execute_for_one_ear(parameters, HearingDynamic::Left);
}

void HearingTester::execute_for_one_ear(HearingParameters& parameters, HearingDynamic dynamic)
{
    assert(is_frequency_valid(parameters.frequency));
    const TickStep* tick_step_ptr = dynamic_cast<const TickStep*>(parameters.initial_volume_step);
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

    bool is_measure_started = start_hearing_threshold_measure(params.frequency, dynamic);
    assert(is_measure_started);

    if (params.pass_algorithm == PassAlgorithm::staircase)
    {
        tick_step_ptr = dynamic_cast<const TickStep*>(params.initial_volume_step);
        assert(tick_step_ptr != nullptr);
        for (size_t i = 0; i < params.increasing_pass_count; ++i)
        {
            cout << "Running increasing pass test " << i + 1 << " of " << params.increasing_pass_count << endl;
            HearingThresholdResult rough_result =
                    make_pass(PassVariant::Increasing, params.start_volume, tick_step_ptr);
            if (rough_result.MCU_result.is_result_received)
            {
                increasing_pass_threshold_results.emplace_back(false, rough_result);

                int first_accurate_measure_index = (int) increasing_pass_threshold_results.size() - 1 + 1;
                VolumeLevel start_volume = rough_result.MCU_result.threshold;
                start_volume.subtract(params.initial_volume_step);
                for (size_t j = 0; j < params.increasing_accurate_pass_count; ++j)
                {
                    sleep(chrono::milliseconds(rand() % 2000 + 1000));
                    if (j >= params_internal.previous_same_results_lookback_count &&
                    std::count_if(increasing_pass_threshold_results.begin() + first_accurate_measure_index, increasing_pass_threshold_results.end(), [&](const auto& item){
                        return item.first;
                    }) >= params_internal.previous_same_results_lookback_count )  // If we already have at least lookback_count accurate results in vector after last rough measure
                    {
                        size_t same_results_count = std::count_if(increasing_pass_threshold_results.begin() + first_accurate_measure_index, increasing_pass_threshold_results.end(),
                                                                  [&](const auto& item) {
                                                              assert(item.first);
                                                              if (item.second.MCU_result.threshold.get_ticks() - start_volume.get_ticks() <= params_internal.same_result_threshold)
                                                                  return true;
                                                              return false;
                                                          });
                        if (same_results_count >= params_internal.previous_same_results_lookback_count && start_volume.get_ticks() > 0)
                        {
                            cout << "Warning! Changing start volume from " << start_volume.get_ticks();
                            if (! start_volume.subtract_ticks(params_internal.amplitude_change_if_test_wrong))
                                start_volume.set_ticks(0);
                            cout << " to " << start_volume.get_ticks() << ", because " << params_internal.previous_same_results_lookback_count
                                 << " last patient' responses are located near start volume" << endl;
                            j = 0;
                            first_accurate_measure_index = (int) increasing_pass_threshold_results.size() - 1 + 1;
                        }
                    }
                    TickStep unity_step = TickStep(1);
                    HearingThresholdResult accurate_result = make_pass(PassVariant::Increasing, start_volume, &unity_step);
                    if (accurate_result.MCU_result.is_result_received)
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
                                            return a + p.second.MCU_result.threshold.get_ticks();
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
                    make_pass(PassVariant::Decreasing, initial_volume_in_decreasing_pass, tick_step_ptr);
            if (rough_result.MCU_result.is_result_received)
            {
                decreasing_pass_threshold_results.emplace_back(false, rough_result);

                size_t first_accurate_measure_index = decreasing_pass_threshold_results.size() - 1 + 1;
                VolumeLevel start_amplitude = rough_result.MCU_result.threshold;
                start_amplitude.subtract(params.initial_volume_step);
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
                    HearingThresholdResult accurate_result = make_pass(PassVariant::Decreasing, start_amplitude, &accurate_step);
                    if (accurate_result.MCU_result.is_result_received)
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
                                            return a + p.second.MCU_result.threshold.get_ticks();
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
        log_string.precision(2 + 2); // in format xx.xx or x.xxx
        log_string << "Is accurate result" << del << "Threshold (linear)" << del << "Threshold (dB)" << del << "Elapsed time by MCU" << del << "Elapsed time by PC" << endl;
        log_string << "Up threshold results:" << endl;
        for (const auto &item: increasing_pass_threshold_results)
            log_string << item.first << del << item.second.MCU_result.threshold.get_ticks() << del << item.second.MCU_result.threshold.get_dB() << del << item.second.MCU_result.elapsed_time << del << item.second.elapsed_time_on_PC << endl;
        if (!decreasing_pass_threshold_results.empty())
        {
            log_string << "Decreasing threshold results:" << endl;
            for (const auto &item : decreasing_pass_threshold_results)
                log_string << item.first << del << item.second.MCU_result.threshold.get_ticks() << del << item.second.MCU_result.threshold.get_dB() << del << item.second.MCU_result.elapsed_time << del << item.second.elapsed_time_on_PC << endl;
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
                            std::back_inserter(increasing_thresholds), [](const auto& result) {return result.second.MCU_result.threshold.get_ticks();}, is_include_result);

        vector<uint16_t> decreasing_thresholds;
        l_std::transform_if(decreasing_pass_threshold_results.begin(),  decreasing_pass_threshold_results.end(),
                            std::back_inserter(decreasing_thresholds), [](const auto& result) {return result.second.MCU_result.threshold.get_ticks();}, is_include_result);

//        assert(average_increasing_threshold - stat_aggregator::get_mean(increasing_thresholds.begin(), increasing_thresholds.end()));
//        assert(average_decreasing_threshold - stat_aggregator::get_mean(decreasing_thresholds.begin(), decreasing_thresholds.end()));
//        average_increasing_threshold - mean already calculated before
        double increasing_threshold_std = stat_aggregator::get_std(increasing_thresholds.begin(), increasing_thresholds.end(), average_increasing_threshold);

        double decreasing_threshold_std = stat_aggregator::get_std(decreasing_thresholds.begin(),  decreasing_thresholds.end(), average_decreasing_threshold);

        double increasing_conf_interval = stat_aggregator::get_confidence_interval(increasing_threshold_std, increasing_thresholds.size(), 0.95);
        double decreasing_conf_interval = stat_aggregator::get_confidence_interval(decreasing_threshold_std, decreasing_thresholds.size(), 0.95);

        log_string << "increasing accurate results' threshold: " << average_increasing_threshold << " +- " << increasing_conf_interval << " (with std: " << increasing_threshold_std << ")";
        if (! decreasing_pass_threshold_results.empty())
        log_string << del << "decreasing accurate results' threshold:" << average_decreasing_threshold << " +- " << decreasing_conf_interval << " (with std: " << decreasing_threshold_std << ")";
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
    global_time_measurer.log_end("run test for one ear");
}

vector<VolumeLevel> HearingTester::execute_for_one_ear(HearingParametersForBSA_Algorithm& parameters, HearingDynamic dynamic)
{
    assert(is_frequency_valid(parameters.frequency));
    assert(parameters.increasing_volume_step != nullptr);
    assert(parameters.decreasing_volume_step != nullptr);
    assert(parameters.time_step <= 5'000);
    assert(parameters.is_equal_threshold != nullptr);
    assert(parameters.minimum_count_of_equal_thresholds < 10);
//    params_for_BSA = parameters;

    const uint16_t MINIMUM_TIME_STEP = 1500;
    const uint16_t TIME_TO_WAIT_BEFORE_GETTING_CONFIRMING_RESULT = 3;

    bool is_measure_started = start_hearing_threshold_measure(parameters.frequency, dynamic);
    assert(is_measure_started);

    Time_measurer time_measurer{};

    vector<HearingThresholdResult> results_in_increasing_pass{};
    vector<HearingThresholdResult> results_in_decreasing_pass{};

    size_t max_same_results_count = 0;
    HearingThresholdResult result_with_max_same_count {};

    PassVariant current_pass = PassVariant::Increasing;
    VolumeLevel current_volume = parameters.start_volume;
    uint16_t current_time_step = parameters.time_step;

    uint16_t current_time = 0;
    bool is_current_pass_need_to_be_changed = false;

    set_pass_parameters(DesiredButtonState::StartWaitingForPress);
    HearingThresholdResultFromMCU current_result = set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
    if (current_result.is_result_received == true)
    {
        cerr << "Warning! In " << __PRETTY_FUNCTION__ << ": when set initial threshold, some result already received. Ignoring it." << endl;
        reset_current_result_on_device(DesiredButtonState::StartWaitingForPress);
    }

    while (max_same_results_count < parameters.minimum_count_of_equal_thresholds)
    {
        sleep(chrono::milliseconds(current_time_step));

        if (current_pass == PassVariant::Decreasing)
        {
            HearingThresholdResultFromMCU current_result_for_previous_volume = set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
            if (current_result_for_previous_volume.is_result_received == false)
            {
//                reset_current_result_on_device(DesiredButtonState::StartWaitingForPress);
                bool is_subtracted = current_volume.subtract(parameters.decreasing_volume_step);
                if (!is_subtracted)
                {
                    if (current_volume.get_ticks() != 0)
                        current_volume.set_ticks(0);
                    else
                        cerr << "Warning! In " << __PRETTY_FUNCTION__ << " in decreasing pass: "
                                           "volume 0 is set, but patient reacted for this volume. Continuing waiting till patient will not react" << endl;
                }
                set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
            }
            else
            {
                ///Confirm that patient not pressed and released button again
                reset_current_result_on_device(DesiredButtonState::StartWaitingForRelease);
                sleep(chrono::milliseconds(TIME_TO_WAIT_BEFORE_GETTING_CONFIRMING_RESULT));
                HearingThresholdResultFromMCU confirming_result = set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
                if (confirming_result.is_result_received == true)
                {
                    results_in_decreasing_pass.emplace_back(HearingThresholdResult{current_result_for_previous_volume,
                                                                                   static_cast<uint16_t>(current_time - current_time_step)});
                    is_current_pass_need_to_be_changed = true;
                }
                else
                    cerr << "Catch false result in decreasing pass" << endl;
            }
        }
        else if (current_pass == PassVariant::Increasing)
        {
            HearingThresholdResultFromMCU current_result_for_previous_volume = set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
            if (current_result_for_previous_volume.is_result_received == false)
            {
                bool is_increased = current_volume.add(parameters.increasing_volume_step);
                if (!is_increased)
                {
                    if (current_volume.get_ticks() < VolumeLevel::MAX_TICK_VOLUME)
                        current_volume.set_ticks(VolumeLevel::MAX_TICK_VOLUME);
                    else
                        cerr << "Warning! In increasing pass: max volume is set, but patient doesn't reacted for this volume. Continuing waiting till patient will not react" << endl;
                }
                set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
            }
            else
            {
                ///Confirm that patient not pressed and released button again
                reset_current_result_on_device(DesiredButtonState::StartWaitingForPress);
                sleep(chrono::milliseconds(TIME_TO_WAIT_BEFORE_GETTING_CONFIRMING_RESULT));
                HearingThresholdResultFromMCU confirming_result = set_up_new_amplitude_and_receive_threshold_result(current_volume.get_ticks());
                if (confirming_result.is_result_received == true)
                {
                    results_in_increasing_pass.emplace_back(HearingThresholdResult{current_result_for_previous_volume, static_cast<uint16_t>(current_time - current_time_step)});

                    for (const auto &item : results_in_increasing_pass)
                    {
                        size_t count_of_same_result = std::count_if(results_in_increasing_pass.begin(), results_in_increasing_pass.end(), [&](const typename decltype(results_in_increasing_pass)::value_type result)
                        {
                            return item.MCU_result.threshold.is_equal(result.MCU_result.threshold, parameters.is_equal_threshold);
                        });
                        assert(count_of_same_result >= 1); // Хотя бы сам с собой он будет равен
//                       cout << "Count of 'same' results with threshold " << item.MCU_result.threshold.get_ticks() << ": " << count_of_same_result << endl;
                        if (count_of_same_result > max_same_results_count)
                        {
                            max_same_results_count = count_of_same_result;
                            result_with_max_same_count = item;
                        }
                    }
                    is_current_pass_need_to_be_changed = true;
                }
                else
                    cerr << "Catch false result in increasing pass" << endl;
            }
        }

        if (is_current_pass_need_to_be_changed)
        {
            current_time = 0;

            if (current_pass == PassVariant::Increasing)
                current_pass = PassVariant::Decreasing;
            else
                current_pass = PassVariant::Increasing;

            /// Set current_time_step with randomly deviated from parameters.time_step value
            const bool sign_plus = rand() % 2;
            if (sign_plus)
            {
                const uint16_t divider = parameters.time_step / 3;
                current_time_step = parameters.time_step + rand() % divider;
            }
            else
            {
                uint16_t divider;
                if (parameters.time_step >= MINIMUM_TIME_STEP)
                    divider = parameters.time_step - MINIMUM_TIME_STEP;
                else
                    divider = 1;
                current_time_step = parameters.time_step - rand() % divider;
            }
            cout << "Now " << current_pass << " pass" << endl;
//            cout << "In pending " << current_pass << " pass: setting " << current_time_step << " time step" << endl;

            if (current_pass == PassVariant::Increasing)
                set_pass_parameters(DesiredButtonState::StartWaitingForPress);
            else
                set_pass_parameters(DesiredButtonState::StartWaitingForRelease);

            is_current_pass_need_to_be_changed = false;
        }
        else
        {
            current_time += current_time_step;
        }
    }

    std::vector<VolumeLevel> final_results = {};
    std::for_each(results_in_increasing_pass.begin(), results_in_increasing_pass.end(), [&](const auto& item)
    {
        if (result_with_max_same_count.MCU_result.threshold.is_equal(item.MCU_result.threshold, parameters.is_equal_threshold))
            final_results.push_back(item.MCU_result.threshold);
    });
    assert(final_results.size() == max_same_results_count);

    if(final_results[0].get_dB() > 30)
    {
        set_up_new_amplitude_and_receive_threshold_result(VolumeLevel().set_dB(15).get_ticks());
        sleep(chrono::milliseconds(10));
        set_up_new_amplitude_and_receive_threshold_result(0);
    }
    else
        set_up_new_amplitude_and_receive_threshold_result(0);

    std::array<uint16_t, REACTION_SURVEYS_COUNT> reaction_time_results{};

    if (parameters.is_reaction_time_need_to_be_measured)
    {
        VolumeLevel volume_for_measuring_reaction_time = final_results[0];
        volume_for_measuring_reaction_time.add_dB(5);
        reaction_time_results = get_reaction_time(volume_for_measuring_reaction_time.get_ticks());
    }

//    double final_threshold = stat_aggregator::get_mean(final_results.begin(),  final_results.end());

//    vector<uint16_t> increasing_thresholds;
//    std::transform(results_in_increasing_pass.begin(),  results_in_increasing_pass.end(),
//                        std::back_inserter(increasing_thresholds), [](const auto& result) {return result.MCU_result.threshold.get_ticks();});
//
//    vector<uint16_t> decreasing_thresholds;
//    std::transform(results_in_decreasing_pass.begin(),  results_in_decreasing_pass.end(),
//                        std::back_inserter(decreasing_thresholds), [](const auto& result) {return result.MCU_result.threshold.get_ticks();});

//    double average_increasing_threshold = stat_aggregator::get_mean(increasing_thresholds.begin(), increasing_thresholds.end());
//    double average_decreasing_threshold = stat_aggregator::get_mean(decreasing_thresholds.begin(), decreasing_thresholds.end());
//
//    double increasing_threshold_std = stat_aggregator::get_std(increasing_thresholds.begin(), increasing_thresholds.end(), average_increasing_threshold);
//    double decreasing_threshold_std = stat_aggregator::get_std(decreasing_thresholds.begin(),  decreasing_thresholds.end(), average_decreasing_threshold);
//
//    double increasing_conf_interval = stat_aggregator::get_confidence_interval(increasing_threshold_std, increasing_thresholds.size(), 0.95);
//    double decreasing_conf_interval = stat_aggregator::get_confidence_interval(decreasing_threshold_std, decreasing_thresholds.size(), 0.95);

    std::stringstream printed_results;
    printed_results.precision(4);
    printed_results << "Threshold (linear)" << del << "Threshold (dB)" << del << "Elapsed time by PC" << del << "Elapsed time by MCU" << del << "Delta time" << endl;
    printed_results << "Increasing threshold results:" << endl;
    for (const auto &item : results_in_increasing_pass)
    {
        printed_results << item.MCU_result.threshold.get_ticks() << del << item.MCU_result.threshold.get_dB() << del << item.elapsed_time_on_PC << del << item.MCU_result.elapsed_time;
        if (item.MCU_result.elapsed_time > item.elapsed_time_on_PC)
            printed_results << del << item.MCU_result.elapsed_time - item.elapsed_time_on_PC;
        printed_results << endl;
    }

    printed_results << "Decreasing threshold results:" << endl;
    for (const auto &item : results_in_decreasing_pass)
    {
        printed_results << item.MCU_result.threshold.get_ticks() << del << item.MCU_result.threshold.get_dB() << del << item.elapsed_time_on_PC << del << item.MCU_result.elapsed_time;
        if (item.MCU_result.elapsed_time > item.elapsed_time_on_PC)
            printed_results << del << item.MCU_result.elapsed_time - item.elapsed_time_on_PC;
        printed_results << endl;
    }

    printed_results << "Final results:" << endl;
    for (const auto &item : final_results)
    {
        printed_results << item.get_ticks() << del << item.get_dB() << endl;
    }

    if(reaction_time_results[0] != 0)
    {
        printed_results << "Reaction time: ";
        for (const auto &item : reaction_time_results)
        {
            printed_results << del << item;
        }
    }

//    printed_results << "increasing threshold results: " << average_increasing_threshold << " +- " << increasing_conf_interval << " (with std: " << increasing_threshold_std << ")";
//    printed_results << del << "decreasing threshold results:" << average_decreasing_threshold << " +- " << decreasing_conf_interval << " (with std: " << decreasing_threshold_std << ")";

    stat << printed_results.str();
    stat.flush();
    cout << printed_results.str();

    time_measurer.log_end("estimating threshold");

    return final_results;
}

bool HearingTester::start_hearing_threshold_measure(const uint16_t frequency, const HearingDynamic dynamic)
{
    uint8_t cmd = 0x11;
    writer.set_cmd(cmd);
    writer.append_var<uint8_t>(static_cast<uint8_t>(dynamic) );/// Channel //TODO Перевести частоту на 80 кГц
    writer.append_var<uint16_t>(frequency);/// also it could be array of 1-3 notes
    writer.prepare_for_sending();
    writer.write();
    reader.read();
    if (reader.is_error())
    {
        string err = reader.get_error_string();
        cerr << "error occurred when sending command " << print_cmd(cmd) << ": '" << err << '\'' << endl;
        return false;
    }
    else
    {
        assert(reader.is_empty());
        return true;
    }
}

// Make one amplitude pass
HearingThresholdResult
HearingTester::make_pass(const PassVariant pass_variant, const VolumeLevel start_volume, const TickStep* amplitude_step)
{
    if (pass_variant == PassVariant::Decreasing)
    {
        const uint16_t TONE_INCREASING_TIME = 700;
        const size_t NUM_OF_STEPS = 30;
        const uint16_t time_step = TONE_INCREASING_TIME / NUM_OF_STEPS;
        static_assert(time_step >= 10); // Approximately minimum OS task scheduler time for sleep

        for (size_t i = 1; i <= NUM_OF_STEPS; ++i)
        {
            set_up_new_amplitude_and_receive_threshold_result(i * start_volume.get_ticks() / NUM_OF_STEPS);
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
            const bool sign_plus = rand() % 2;
            if (sign_plus)
            {
                const uint16_t divider = params.time_step / 3;
                time_step = params.time_step + rand() % divider;
            }
            else
            {
                uint16_t divider;
                if (params.time_step >= params_internal.minimum_deviated_time_step)
                    divider = params.time_step - params_internal.minimum_deviated_time_step;
                else
                    divider = 1;
                time_step = params.time_step - rand() % divider;
            }
            cout << "In pass: Setting " << time_step << " time step" << endl;
            break;
    }
    DesiredButtonState button_state;
    switch (pass_variant)
    {
        case PassVariant::Increasing: // NOLINT(bugprone-branch-clone)
            button_state = DesiredButtonState::StartWaitingForPress;
            break;
        case PassVariant::Decreasing:
            button_state = DesiredButtonState::StartWaitingForPress;
            break;
    }
    set_pass_parameters(button_state);
    while (true)
    {
        switch (pass_variant)
        {
            case PassVariant::Increasing:
                curr_amplitude.add(amplitude_step);
                if (curr_amplitude > params.max_amplitude_in_increasing_pass)
                    goto end;
                break;
            case PassVariant::Decreasing:
                if (! curr_amplitude.subtract(amplitude_step))
                    goto end;
                break;
        }
        HearingThresholdResultFromMCU result = set_up_new_amplitude_and_receive_threshold_result(
                curr_amplitude.get_ticks());
        if (result.is_result_received)
        {
            if (pass_variant == PassVariant::Decreasing && curr_amplitude.get_ticks() >= start_volume.get_ticks() - 3 * amplitude_step->get_ticks())
            {
                reset_current_result_on_device(button_state);
            }
            else
            {
                pass_result.MCU_result = result;
                curr_time -= time_step; // for correct reaction time calculating
//                goto end;
                break;
            }
        }
        sleep(chrono::milliseconds(time_step));
        curr_time += time_step;
    }
    end:
    if (! pass_result.MCU_result.is_result_received)
        pass_result.MCU_result = set_up_new_amplitude_and_receive_threshold_result(0);// If result not received, try to get results at last time
    else
        set_up_new_amplitude_and_receive_threshold_result(0);
    pass_result.elapsed_time_on_PC = curr_time;
    return pass_result;
}

/// set parameters for one pass (makes device counts time till desired button state will appear on it)
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

HearingThresholdResultFromMCU HearingTester::set_up_new_amplitude_and_receive_threshold_result(uint16_t curr_amplitude)
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
        return HearingThresholdResultFromMCU{is_result_received, elapsed_time_by_device, volume_level};
    }
    else if (button_state == ActualButtonState::WaitingForPress || button_state == ActualButtonState::WaitingForRelease)  ///nothing needs to be read, we waiting until patient press or release the button (or when time has gone)
    {
        return HearingThresholdResultFromMCU{false, 0, VolumeLevel().set_ticks(0)};
    }
    else
    {
        assert(false && static_cast<bool>(button_state));
        return HearingThresholdResultFromMCU{false, 0, VolumeLevel().set_ticks(0)};
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
    assert(is_frequency_valid(frequency));

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

bool HearingTester::is_frequency_valid(uint16_t frequency)
{
    return frequency != 0 && frequency <= 20'000;
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
    if (_ticks <= VolumeLevel::MAX_TICK_VOLUME)
    {
        ticks = _ticks;
        dB = convert_to_dB(ticks);
    }
    else
    {
        cerr << "Warning! In " << __PRETTY_FUNCTION__ << " Attempt to set ticks =" << _ticks
             << " that greater than maximum possible volume " << VolumeLevel::MAX_TICK_VOLUME << ". Setting ticks = 0" << endl;
        ticks = 0;
        dB = convert_to_dB(ticks);
    }
    return *this;
}

[[maybe_unused]] VolumeLevel& VolumeLevel::set_dB(double _dB)
{
    assert(_dB >= 0 && _dB <= 80);
    ticks = convert_to_ticks(_dB);
    assert(ticks <= VolumeLevel::MAX_TICK_VOLUME);
    this->dB = _dB;
    return *this;
}

bool VolumeLevel::add_dB(double amount)
{
    uint16_t new_ticks = convert_to_ticks(dB + amount);
    if (new_ticks <= VolumeLevel::MAX_TICK_VOLUME)
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
    if (new_ticks <= VolumeLevel::MAX_TICK_VOLUME)
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
    // else
    cerr << "Warning! In " << __PRETTY_FUNCTION__
         << " Unknown cast for volumeStep pointer provided to me! Not incrementing any number and returning false";
    return false;
}

bool VolumeLevel::subtract_dB(double amount)
{
//    if (dB - amount < 0)
//        return false;
    uint16_t new_ticks = convert_to_ticks(dB - amount);
    if(new_ticks <= VolumeLevel::MAX_TICK_VOLUME)
    {
        if (ticks == new_ticks)
        {
            cerr << "Warning! In " << __PRETTY_FUNCTION__ << " Current tick (" << ticks << ") and new tick (" << new_ticks << ") "
                                                                                                                              "calculated for " << dB << " - " << amount << "dB are the same." << endl;
            if (ticks == 0)
                cerr << "Underflow for converting dB to ticks. Not doing subtract" << endl;
        }
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
    if (new_ticks <= VolumeLevel::MAX_TICK_VOLUME)
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
    // else
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

bool VolumeLevel::is_equal(const VolumeLevel& rhs, VolumeStep* is_equal_threshold) const
{
    if (this->get_ticks() == rhs.get_ticks())
        return true;
    VolumeLevel min_el = this->get_ticks() < rhs.get_ticks() ? *this : rhs;
    const VolumeLevel& max_el = this->get_ticks() < rhs.get_ticks() ? rhs : *this;
    min_el.add(is_equal_threshold);
    if (min_el.get_ticks() >= max_el.get_ticks())
        return true;
    return false;
}
