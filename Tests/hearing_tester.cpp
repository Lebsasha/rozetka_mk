#include <chrono>
#include <thread>
#include "time_measurer.h"
#include "hearing_tester.h"

using namespace std;


template<typename Rep, typename Period>
inline void sleep(const chrono::duration<Rep, Period>& rtime)
{
#ifdef USE_SLEEP_IN_HEARING_TEST
    this_thread::sleep_for(rtime);
#endif
}

const string del = ", "; //Delimiter between writing to csv file results

HearingTester::HearingTester(Command_writer& _writer, Command_reader& _reader, ostream& _stat): writer(_writer), reader(_reader), stat(_stat)
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
            sleep(chrono::milliseconds(rand() % 1000 + 1500));
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
