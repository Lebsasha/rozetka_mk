#include "general.h"
#include <stdlib.h>

extern TIM_HandleTypeDef htim4;

void Button_ctor(Button* button, Pin pin)
{
    button->GPIOx = pin.GPIOx;
    button->pin = pin.pin;
    button->start_time = 0;
    button->stop_time = 0;
    button->state = ButtonIdle;
}

/// @note If called when button already started, start counting again from current point of time, regardless of what state button is in
/// @param state must be @c WaitingForRelease or @c WaitingForPress, otherwise state will be unchanged and false will be returned
/// @return false if argument @a state is invalid, otherwise true
bool ButtonStart(Button *button, ButtonStateOnHighLevel state)
{
    if (state == HL_WaitingForPress)
    {
        button->state = WaitingForPress;
    }
    else if (state == HL_WaitingForRelease)
    {
        button->state = WaitingForRelease;
    }
    else
        return false;
    button->start_time = HAL_GetTick();
    button->stop_time = 0;

    //TODO Check that htim4 not used in other way (i. e. some survey may use it and we don't want to accidentally overwrite parameters)
//    __HAL_TIM_ENABLE_IT(&htim4, TIM_IT_UPDATE);
//    __HAL_TIM_ENABLE(&htim4);
    htim4.Instance->PSC = 3;
    htim4.Instance->ARR = 17999;
    HAL_TIM_Base_Start_IT(&htim4);
    return true;
}

/// This function should be called when current measure (e. g. SkinConduction) finished at all, for releasing TIM4 from button' function
void ButtonStop(Button* button)
{
    HAL_TIM_Base_Stop_IT(&htim4);
    button->stop_time = 0;
    button->start_time = 0;
    button->state = ButtonIdle;
}

void timer_start(Timer* ptr, uint32_t delay)
{
    ptr->endTick = HAL_GetTick() + delay;
}

bool is_time_passed(Timer* ptr)
{
    if (ptr->endTick != 0)
        return HAL_GetTick() >= ptr->endTick;
    else
        return false;
}

/// Reset needed for preventing accidental accessing already elapsed timer
void timer_reset(Timer* ptr)
{
    ptr->endTick = 0;
}

void RandInitializer_ctor(RandInitializer* randInitializer)
{
    randInitializer->isInitialised = false;
    htim4.Instance->ARR = UINT16_MAX;///Another way random seed could be got: via special microseconds counter register in STM32 core
    htim4.Instance->PSC = 0;
    HAL_TIM_Base_Start_IT(&htim4);
}

/// Get seed for setting @code srand(seed)
void InitRand(RandInitializer* randInitializer)
{
    if (!randInitializer->isInitialised)
    {
        HAL_TIM_Base_Stop_IT(&htim4);
        uint16_t seed = htim4.Instance->CNT;
        srand(seed);
        randInitializer->isInitialised = true;
    }
}

inline void write_pin_if_in_debug(GPIO_TypeDef* GPIOx, uint16_t pin, GPIO_PinState pinState)
{
#ifndef NDEBUG
//    GPIOx->BSRR = pin;
    HAL_GPIO_WritePin(GPIOx, pin, pinState);
#endif
}
