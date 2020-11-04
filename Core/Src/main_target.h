//
// Created by alexander on 01/11/2020.
//

#ifndef MAIN_TARGET
#define MAIN_TARGET

void my_delay(int mc_s);

void soft_glow(GPIO_TypeDef *port, int pin, int duty_cycle, int mc_s, int detailyty);

void calc_1();

void calc_2();

void calc_3();

enum STEP {UP, LIGHT, DOWN};
struct LED
{
    int counter;
    int i;
    int ampl;
    const int DETAILYTY;
};
#endif //MAIN_TARGET
