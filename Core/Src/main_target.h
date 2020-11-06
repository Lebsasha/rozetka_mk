//
// Created by alexander on 01/11/2020.
//

#ifndef MAIN_TARGET
#define MAIN_TARGET

enum STEP {UP, LIGHT, DOWN};
struct LED
{
    uint16_t pin;
    uint16_t detailyty;
    uint16_t i;
    unsigned char counter;
    unsigned char ampl;
    enum STEP curr_step;
};

void ctor_LED(struct LED* led, int detailyty, int pin);
void my_delay(int mc_s);
void calc(struct LED*);

#endif //MAIN_TARGET
