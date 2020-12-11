#ifndef MAIN_TARGET
#define MAIN_TARGET

#define COUNTER_PERIOD 1800
//htim1.Instance->ARR+1
//(const uint16_t) 100

struct LED
{
    volatile uint32_t* duty_cycle;
    volatile uint16_t detailyty;
    uint16_t i;
    char num;

    void (* curr_step)(struct LED*);
};

void ctor_LED(struct LED* led, uint16_t detailyty, volatile uint32_t* duty_cycle, char num);

void my_delay(int mc_s);

void process_cmd(const uint8_t* command, const uint32_t* len);

#endif //MAIN_TARGET
