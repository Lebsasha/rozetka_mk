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
    AmplitudeChangingAlgorithm amplitude_algorithm{};
    TimeStepChangingAlgorithm time_step_changing_algorithm{};
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

    void execute(HearingParameters parameters);

private:

    Command_writer& writer;
    Command_reader& reader;
    std::ostream& stat;

    HearingParameters params;
    enum class PassVariant {IncreasingLinear, DecreasingLinear};
    static const size_t REACTION_SURVEYS_COUNT = 3;


    void execute_for_one_ear(HearingParameters parameters, HearingDynamic dynamic);

    HearingThresholdResult make_pass(PassVariant pass_variant, uint16_t start_amplitude, uint16_t amplitude_step);

    void set_pass_parameters(DesiredButtonState state);

    void reset_current_result_on_device(DesiredButtonState state);

    HearingThresholdResult set_up_new_amplitude_and_receive_threshold_results(uint16_t curr_amplitude);

    std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> get_reaction_time(uint16_t amplitude);
};


#endif //HEARING_TESTER
