#ifndef MAIN_TARGET
#define MAIN_TARGET

#define LONG_STRING \
"12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678"

struct LED
{
    volatile uint32_t* pin;
    uint16_t detailyty;
    uint16_t i;
    char num;

    void (* curr_step)(struct LED*);
};

void my_delay(int mc_s);

void process_cmd(const uint8_t* command, const uint32_t* len);

#endif //MAIN_TARGET
