#ifndef SKIN_CONDUCTION
#define SKIN_CONDUCTION

#include "general.h"

typedef struct SkinConductionTester
{
	void (*nextState) (struct SkinConductionTester*);
	uint16_t numberOfMeandrs;
	volatile uint32_t currentMeandr;

	uint16_t numberOfBursts;
	volatile uint32_t currentBurst;
	uint32_t burstActive;
	volatile uint16_t burstPeriod;
	uint16_t amplitude;
	volatile uint32_t* pwmPin;

	uint16_t reactionTime;
	uint16_t maxReactionTime;//in ms
}SkinConductionTester;

void ConstructDiode(SkinConductionTester* source);
void SkinConductionStart(SkinConductionTester* skinTester);
void SkinConductionEnd(SkinConductionTester* skinTester);
void SkinConduction_send_result_to_PC(SkinConductionTester* skinTester);

#endif /* SKIN_CONDUCTION */
