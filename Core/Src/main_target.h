//
// Created by alexander on 01/11/2020.
//

#ifndef MAIN_TARGET
#define MAIN_TARGET

#define COUNTER_PERIOD (const uint16_t) 100

struct LED
{
    volatile uint32_t* pin;
    uint16_t detailyty;
    uint16_t i;
    char num;
    void (* curr_step)(struct LED*);
};

void ctor_LED(struct LED* led, uint16_t detailyty, volatile uint32_t* pin, char num);

void my_delay(int mc_s);

void process_cmd(uint8_t* Buf, uint32_t *Len);

#endif //MAIN_TARGET
