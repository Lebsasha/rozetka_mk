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
std::ostream& operator<<(std::ostream& os, const HearingDynamic& enum_member);

enum class DesiredButtonState {StartWaitingForPress=0, StartWaitingForRelease=2};
enum class ActualButtonState {WaitingForPress=0, Pressed=1, WaitingForRelease=2, Released=3};

enum class PassAlgorithm {inc_linear_by_step=0, dec_linear_by_step=1, staircase};

/// How time step need to be modified on each pass
/// @a random_deviation time step - i. e. when adding or subtracting random_deviation value to given time step
enum class TimeStepChangingAlgorithm {constant, random_deviation};

static const uint16_t MAX_VOLUME = 4095;

class VolumeStep
{
public:
    virtual ~VolumeStep() = default;
};


class DbStep: public VolumeStep
{
    double dB;
public:
    [[maybe_unused]] explicit DbStep(double _dB);
    [[nodiscard]] double get_dB() const;
};

struct TickStep: public VolumeStep
{
    uint16_t ticks;
public:
    [[maybe_unused]] explicit TickStep(uint16_t _ticks);
    [[nodiscard]] uint16_t get_ticks() const;
};

class VolumeLevel
{
    uint16_t ticks;
    double dB;

    static double  convert_to_dB(uint16_t ticks);
    static uint16_t convert_to_ticks(double dB);

public:

    static const uint16_t MAX_TICK_VOLUME = 4095;
    constexpr static double TICKS_FOR_0dB = 15.4;

    VolumeLevel() = default;

    VolumeLevel& set_ticks(uint16_t _ticks);
    [[maybe_unused]] VolumeLevel& set_dB(double _dB);

    bool add_dB(double amount);
    bool add_ticks(uint16_t amount);
    bool add(const VolumeStep* vs);

    bool subtract_dB(double amount);
    bool subtract_ticks(uint16_t amount);
    bool subtract(const VolumeStep* vs);

    [[nodiscard]] uint16_t get_ticks() const;
    [[nodiscard]] double get_dB() const;

    bool is_equal(const VolumeLevel& rhs, VolumeStep* is_equal_threshold) const;

    bool operator<(const VolumeLevel& rhs) const;
    bool operator>(const VolumeLevel& rhs) const;
    bool operator<=(const VolumeLevel& rhs) const;
    bool operator>=(const VolumeLevel& rhs) const;
};

struct HearingParameters
{
    uint16_t frequency =0;
    VolumeLevel max_amplitude_in_increasing_pass =VolumeLevel();
    VolumeStep* initial_volume_step =nullptr;
    uint16_t time_step =0;
    PassAlgorithm pass_algorithm{};

    size_t increasing_pass_count =0;
    size_t increasing_accurate_pass_count =0;
    size_t decreasing_pass_count =0;
    size_t decreasing_accurate_pass_count =0;
    VolumeLevel start_volume =VolumeLevel();
};

struct HearingParametersForBSA_Algorithm
{
    uint16_t frequency;
    VolumeLevel start_volume;
    VolumeStep* increasing_volume_step;
    VolumeStep* decreasing_volume_step;
    uint16_t time_step;
    VolumeStep* is_equal_threshold;
    uint16_t minimum_count_of_equal_thresholds;
    bool is_reaction_time_need_to_be_measured;
};

struct HearingParametersInternal
{
    TimeStepChangingAlgorithm time_step_changing_algorithm{};
    uint16_t minimum_deviated_time_step =600;

    /// applicable for increasing pass
    size_t previous_same_results_lookback_count =0;
    uint16_t same_result_threshold =0;

    /// applicable for decreasing pass
    /// If patient miss more than \a missed_decreasing_passes_count, than we need to increase start_volume for decreasing pass
    size_t missed_decreasing_passes_count =0;

    /// applicable for increasing/decreasing passes
    uint16_t amplitude_change_if_test_wrong =0;
};

enum class PassVariant {Increasing, Decreasing};

struct HearingThresholdResultFromMCU
{
    bool is_result_received;
    uint16_t elapsed_time;
    VolumeLevel threshold;
};

struct HearingThresholdResult
{
    HearingThresholdResultFromMCU MCU_result;
    uint16_t elapsed_time_on_PC;
};

class HearingTester
{
public:
    HearingTester(Command_writer& writer, Command_reader& reader, std::ostream& stat);

    void execute(HearingParameters& parameters);

    void execute_for_one_ear(HearingParameters& parameters, HearingDynamic dynamic);

    std::vector<VolumeLevel> execute_for_one_ear(HearingParametersForBSA_Algorithm& parameters, HearingDynamic dynamic);

    [[maybe_unused]] void set_tone_once(HearingDynamic dynamic, uint16_t frequency, uint16_t volume);

private:

    Command_writer& writer;
    Command_reader& reader;
    std::ostream& stat;

    HearingParameters params;
    HearingParametersInternal params_internal;
    static const size_t REACTION_SURVEYS_COUNT = 3;


    HearingThresholdResult make_pass(const PassVariant pass_variant, const VolumeLevel start_volume, const TickStep* amplitude_step);

    void set_pass_parameters(DesiredButtonState state);

    void reset_current_result_on_device(DesiredButtonState state);

    HearingThresholdResultFromMCU set_up_new_amplitude_and_receive_threshold_result(uint16_t curr_amplitude);

    std::array<uint16_t, HearingTester::REACTION_SURVEYS_COUNT> get_reaction_time(uint16_t amplitude);

    void stop_current_measure();

    static bool is_frequency_valid(uint16_t frequency);

    bool start_hearing_threshold_measure(const uint16_t frequency, const HearingDynamic dynamic);
};


#endif //HEARING_TESTER
