#include "skinConduction.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

uint32_t time = 0;
extern Button button;
extern SkinConductionTester skinTester;

void StateOff();
void StateWait();
void WaitStop();
void StateOn();
//void PreparePackageToPC(uint32_t reactionTime);

//volatile bool Pracue = false;
//bool IsPracue()
//{
//    return Pracue;
//}

void ConstructDiode(SkinConductionTester* skinTester)
{
	for (int i = 0; i < SOURCESNUM; ++i)
	{
		skinTester->channel[i].amplitude = 0;
	}
	skinTester->channel[0].pin = &TIM1->CCR1;

	skinTester->numberOfMeandrs = 0;
	skinTester->currentMeandr = 0;
	skinTester->meandrPeriod = 0;

	skinTester->numberOfBursts = 0;
	skinTester->currentBurst = 0;
	skinTester->burstPeriod = 0;
	skinTester->burstActive = 0;

	skinTester->numberOfPulses = 0;
	skinTester->currentPulse = 0;
	skinTester->pulsePause = 0;

	__HAL_TIM_CLEAR_FLAG(&htim4, TIM_SR_UIF);

	skinTester->reactionPause = 0;
	skinTester->maxReactionTime = 0;
	skinTester->maxReactionTime = 10000;
}

struct Phase
{
    int currentAmplitude;
    int minAmplitude;
    int maxAmplitude;
    int step;
    enum
    {
        FirstStage, SecondStage
    } stage;
} phase;

void SkinConductionStart(SkinConductionTester* skinTester)
{
//	htim4.Instance->ARR = skinTester->maxReactionTime;
    skinTester->burstActive = skinTester->numberOfMeandrs;
    htim2.Instance->CCR1 = skinTester->burstPeriod - skinTester->burstActive;
    *(skinTester->channel[0].pin) = skinTester->channel[0].amplitude;
    htim2.Instance->ARR = 9;
    htim2.Instance->PSC = 719;
    htim3.Instance->ARR = 719;
    htim3.Instance->PSC = 0;
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_Base_Start_IT(&htim2);
    HAL_TIM_Base_Start(&htim1);//TODO Совместить с нижней строкой
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    skinTester->nextState = StateWait;
}

void SkinConductionEnd(SkinConductionTester* skinTester)
{
 	HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_1);
	HAL_TIM_Base_Stop_IT(&htim4);
//	time = HAL_TIM_ReadCapturedValue(&htim4, TIM_CHANNEL_1);
    skinTester->reactionTime = button.stop_time-button.start_time;
    __HAL_TIM_SET_COUNTER(&htim4, 0);
	__HAL_TIM_SET_COUNTER(&htim3, 0);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->ARR = 9;//TODO ASK
	TIM_IC_InitTypeDef sConfigIC = {0};
	sConfigIC.ICPolarity = 0;
    sConfigIC.ICSelection = 0;
	sConfigIC.ICPrescaler = 0;
	sConfigIC.ICFilter = 0;
	HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
	htim4.Instance->CCR1 = 0;
	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
	sConfigIC.ICFilter = 0;
    HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
    SkinConductionSendResultToPC(skinTester);
}
//
//void WhatName()
//{
//	if (IsPracue())
//	{
//		uint8_t data[] = {'a', *(skinTester.channel[0].pin), phase.stage};
//		SendPackage(data, sizeof(data));
//	}
//	else
//	{
//		PreparePackageToPC(time);
//	}
//}

void SkinConductionStop(SkinConductionTester* skinTester)
{
	HAL_TIM_Base_Stop_IT(&htim2);
	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
	skinTester->currentBurst = 0;
	skinTester->nextState = StateOff;
 	HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_1);
	HAL_TIM_Base_Stop_IT(&htim4);
	time = HAL_TIM_ReadCapturedValue(&htim4, TIM_CHANNEL_1);
	__HAL_TIM_SET_COUNTER(&htim4, 0);
	__HAL_TIM_SET_COUNTER(&htim3, 0);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->ARR = 9;//TODO ASK
	TIM_IC_InitTypeDef sConfigIC = {0};
	sConfigIC.ICPolarity = 0;
    sConfigIC.ICSelection = 0;
	sConfigIC.ICPrescaler = 0;
	sConfigIC.ICFilter = 0;
	HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
	htim4.Instance->CCR1 = 0;
	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
	sConfigIC.ICFilter = 0;
    HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
//    Pracue = false;
//    uint8_t data[] = {'s', *(skinTester.channel[0].pin), phase.stage, time};
//    SendPackage(data, sizeof(data));
}


void StateOff()
{;}

void StateWait()
{
    if (skinTester.currentMeandr++ < 160)
    {
        return;
    }
    skinTester.currentMeandr = 0;
    WaitStop();
    skinTester.nextState = StateOn;
}

void WaitStop()
{
    htim2.Instance->ARR = skinTester.burstPeriod - 1;
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
//    HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_1);
    button.start_time=HAL_GetTick();
//    HAL_TIM_Base_Start_IT(&htim4);
}

void StateOn()
{
    if (skinTester.currentBurst++ < skinTester.numberOfBursts)
    {
        return;
    }
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    skinTester.currentBurst = 0;
    HAL_TIM_Base_Stop_IT(&htim2);//TODO Совместить с нижней строкой
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    skinTester.nextState = StateOff;
}

//void PreparePackageToPC(uint32_t reactionTime)
//{
//	if (reactionTime == 0)
//	{
//		uint8_t data[] = {'n', 0, 0, 0, *(skinTester.channel[0].pin)};
//		SendPackage(data, sizeof(data));
//	}
//	else
//	{
//		uint8_t data[] = {'n', 1, reactionTime % 256, reactionTime / 256, *(skinTester.channel[0].pin)};
//		SendPackage(data, sizeof(data));
//	}
//    Pracue = false;
//}

