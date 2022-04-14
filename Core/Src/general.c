#include "general.h"
#include <stdlib.h>

extern TIM_HandleTypeDef htim4;

void Button_ctor(Button* button, GPIO_TypeDef* GPIOx, uint16_t pin)
{
    button->GPIOx = GPIOx;
    button->pin = pin;
    button->start_time = 0;
    button->stop_time = 0;
    button->state = ButtonIdle;
}

/// @note If called when button already started, start counting again from current point of time, regardless of what state button is in

void ButtonStart(Button* button)
{
    button->start_time = HAL_GetTick();
    button->stop_time = 0;
    button->state = WaitingForPress;

    //TODO Rewrite with Input Capture
    //TODO Check htim4 not enabled with other parameters (i. e. some survey may use it and we don't want to accidentally overwrite parameters)
    htim4.Instance->PSC = 3;
    htim4.Instance->ARR = 17999;
    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK)
        Error_Handler();
}

void ButtonStop(Button* button)
{
    if (HAL_TIM_Base_Stop_IT(&htim4) != HAL_OK)
        Error_Handler();
    button->stop_time = 0;
    button->start_time = 0;
    button->state = ButtonIdle;
}

void RandInitializer_ctor(RandInitializer* randInitializer)
{
    randInitializer->isInitialised = false;
    htim4.Instance->ARR = (((uint32_t)(1))<<16)-1;///Can be done via special microseconds counter register in STM32 core
    htim4.Instance->PSC = 0;
    HAL_TIM_Base_Start_IT(&htim4);
}

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
#ifdef DEBUG
//    GPIOx->BSRR = pin;
    HAL_GPIO_WritePin(GPIOx, pin, pinState);
#endif
}
