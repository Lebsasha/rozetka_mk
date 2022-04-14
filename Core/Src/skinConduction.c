#include "skinConduction.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

extern Button button;

void StateOff();
void StateWait();
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

//	__HAL_TIM_CLEAR_FLAG(&htim4, TIM_SR_UIF);

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
    htim1.Instance->PSC = 17;
    htim1.Instance->ARR = 99;
    htim2.Instance->ARR = 9;
    htim2.Instance->PSC = 719;
    htim3.Instance->ARR = 1;
    htim3.Instance->PSC = 359;
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_Base_Start_IT(&htim2);
    //HAL_TIM_Base_Start(&htim1);//TODO Даник, скорее всего это не нужно вызывать, поэтому если без этой строки хорошо работает, то удаляй ее
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    skinTester->nextState = StateWait;
}

void SkinConductionEnd(SkinConductionTester* skinTester)
{
//	time = HAL_TIM_ReadCapturedValue(&htim4, TIM_CHANNEL_1);
    if (button.state == Pressed)
        skinTester->reactionTime = button.stop_time - button.start_time;
    else if(button.state == Timeout)
        skinTester->reactionTime = 0;
    else
        return;//TODO If we came to this section then nothing needs to be done, since this function is called when the survey is not actually finished or (unlikely) something went wrong
    ButtonStop(&button);
//    HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_1);
//    HAL_TIM_Base_Stop_IT(&htim4);
//    __HAL_TIM_SET_COUNTER(&htim4, 0);
	__HAL_TIM_SET_COUNTER(&htim3, 0);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->ARR = 9;//TODO
//	TIM_IC_InitTypeDef sConfigIC = {0};
//	sConfigIC.ICPolarity = 0;
//    sConfigIC.ICSelection = 0;
//	sConfigIC.ICPrescaler = 0;
//	sConfigIC.ICFilter = 0;
//	HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
//	htim4.Instance->CCR1 = 0;
//	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
//	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
//	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
//	sConfigIC.ICFilter = 0;
//    HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
    SkinConduction_send_result_to_PC(skinTester);
}

void SkinConductionStop(SkinConductionTester* skinTester)
{
	HAL_TIM_Base_Stop_IT(&htim2);
	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
	skinTester->currentBurst = 0;
	skinTester->nextState = StateOff;
// 	HAL_TIM_IC_Stop(&htim4, TIM_CHANNEL_1);
//	HAL_TIM_Base_Stop_IT(&htim4);
//	time = HAL_TIM_ReadCapturedValue(&htim4, TIM_CHANNEL_1);
//	__HAL_TIM_SET_COUNTER(&htim4, 0);
	__HAL_TIM_SET_COUNTER(&htim3, 0);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->ARR = 9;//TODO
//	TIM_IC_InitTypeDef sConfigIC = {0};
//	sConfigIC.ICPolarity = 0;
//    sConfigIC.ICSelection = 0;
//	sConfigIC.ICPrescaler = 0;
//	sConfigIC.ICFilter = 0;
//	HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
//	htim4.Instance->CCR1 = 0;
//	sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
//	sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
//	sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
//	sConfigIC.ICFilter = 0;
//    HAL_TIM_IC_ConfigChannel(&htim4, &sConfigIC, TIM_CHANNEL_1);
//    Pracue = false;
//    uint8_t data[] = {'s', *(skinTester.channel[0].pin), phase.stage, time};
//    SendPackage(data, sizeof(data));
}


void StateOff(SkinConductionTester* skinTester)
{;}

void StateWait(SkinConductionTester* skinTester)
{
    if (skinTester->currentMeandr++ < 160)
    {
        return;
    }
    skinTester->currentMeandr = 0;
    htim2.Instance->ARR = skinTester->burstPeriod - 1;
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
//    HAL_TIM_IC_Start(&htim4, TIM_CHANNEL_1);
    ButtonStart(&button);
//    HAL_TIM_Base_Start_IT(&htim4);
    skinTester->nextState = StateOn;
}

void StateOn(SkinConductionTester* skinTester)
{
    if (skinTester->currentBurst++ < skinTester->numberOfBursts)
    {
        return;
    }
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    skinTester->currentBurst = 0;
    HAL_TIM_Base_Stop_IT(&htim2);//TODO SAY Нельзя совместить с нижней строкой
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
    skinTester->nextState = StateOff;
}
