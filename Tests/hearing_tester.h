#ifndef HEARING_TESTER
#define HEARING_TESTER

#include <iostream>
#include <array>
#include <vector>
#include "communication_with_MCU.h"

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

static const uint16_t MAX_VOLUME = 4095;

struct HearingParameters
{
    uint16_t frequency=0;
    uint16_t max_amplitude_in_increasing_pass=0;
    uint16_t initial_amplitude_step=0;
    uint16_t time_step=0;
    PassAlgorithm pass_algorithm{};

    size_t increasing_pass_count =0;
    size_t increasing_accurate_pass_count =0;
    size_t decreasing_pass_count =0;
    size_t decreasing_accurate_pass_count =0;
};

struct HearingParametersInternal
{
    AmplitudeChangingAlgorithm amplitude_algorithm{};
    TimeStepChangingAlgorithm time_step_changing_algorithm{};

    /// applicable for increasing pass
    size_t previous_same_results_lookback_count =0;
    uint16_t same_result_threshold =0;

    /// applicable for decreasing pass
    /// If patient miss more than \a missed_decreasing_passes_count, than we need to increase start_volume for decreasing pass
    size_t missed_decreasing_passes_count =0;

    /// applicable for increasing/decreasing passes
    uint16_t amplitude_change_if_test_wrong =0;
};

struct HearingThresholdResult
{
    bool is_result_received;
    uint16_t elapsed_time;
    uint16_t threshold;
};

class HearingTester
{
public:
    HearingTester(Command_writer& writer, Command_reader& reader, std::ostream& stat);

    void execute(HearingParameters& parameters);

    void execute_for_one_ear(HearingParameters& parameters, HearingDynamic dynamic);

    [[maybe_unused]] void set_tone_once(HearingDynamic dynamic, uint16_t frequency, uint16_t volume);

private:

    Command_writer& writer;
    Command_reader& reader;
    std::ostream& stat;

    HearingParameters params;
    HearingParametersInternal params_internal;
    enum class PassVariant {IncreasingLinear, DecreasingLinear};
    static const size_t REACTION_SURVEYS_COUNT = 3;


    HearingThresholdResult make_pass(PassVariant pass_variant, uint16_t start_amplitude, uint16_t amplitude_step);

    void set_pass_parameters(DesiredButtonState state);

    void reset_current_result_on_device(DesiredButtonState state);

    HearingThresholdResult set_up_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude);

    std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> get_reaction_time(uint16_t amplitude);

    void stop_current_measure();

    static void check_frequency_validness(uint16_t frequency);

    static void check_volume_validness(uint16_t& volume);
};


#endif //HEARING_TESTER
