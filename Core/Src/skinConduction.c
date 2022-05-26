#include "skinConduction.h"

extern TIM_HandleTypeDef htim1;
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

extern Button button;

void StateOff();
void StateWait();
void StateOn();

void ConstructDiode(SkinConductionTester* skinTester)
{
	skinTester->amplitude = 0;
	skinTester->pwmPin = &TIM1->CCR1;

	skinTester->numberOfMeandrs = 0;
	skinTester->currentMeandr = 0;

	skinTester->numberOfBursts = 0;
	skinTester->currentBurst = 0;
	skinTester->burstPeriod = 0;
	skinTester->burstActive = 0;

//	__HAL_TIM_CLEAR_FLAG(&htim4, TIM_SR_UIF);

	skinTester->maxReactionTime = 0;
	skinTester->maxReactionTime = 10000;
}

void SkinConductionStart(SkinConductionTester* skinTester)
{
//	htim4.Instance->ARR = skinTester->maxReactionTime;
    skinTester->burstActive = skinTester->numberOfMeandrs;
    htim2.Instance->CCR1 = skinTester->burstPeriod - skinTester->burstActive;
    *(skinTester->pwmPin) = skinTester->amplitude;
    htim1.Instance->PSC = 17;
    htim1.Instance->ARR = 99;
    htim2.Instance->ARR = 9;
    htim2.Instance->PSC = 719;
    htim3.Instance->ARR = 1;
    htim3.Instance->PSC = 359;
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    HAL_TIM_Base_Start_IT(&htim2);
    //HAL_TIM_Base_Start(&htim1);//TODO Даник, скорее всего это не нужно вызывать, поэтому если без этой строки хорошо работает, то удаляй ее
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    skinTester->nextState = StateWait;
}

void SkinConductionEnd(SkinConductionTester* skinTester)
{
    if (button.state == Pressed)
        skinTester->reactionTime = button.stop_time - button.start_time;
    else if(button.state == Timeout)
        skinTester->reactionTime = 0;
    else
        return;//TODO If we came to this section then nothing needs to be done, since this function is called when the survey is not actually finished or (unlikely) something went wrong
    ButtonStop(&button);
	__HAL_TIM_SET_COUNTER(&htim3, 0);
	__HAL_TIM_SET_COUNTER(&htim2, 0);
	htim2.Instance->ARR = 9;//TODO
    SkinConduction_send_result_to_PC(skinTester);
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
    ButtonStart(&button, WaitingForPress);
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
    HAL_TIM_Base_Stop_IT(&htim2);//Эту функцию нельзя совместить с нижней строкой
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_3);
    skinTester->nextState = StateOff;
}
