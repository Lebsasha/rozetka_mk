#ifndef SKIN_CONDUCTION
#define SKIN_CONDUCTION

#define SOURCESNUM 1

#include "general.h"

typedef struct SkinConductionTester
{
	void (*nextState) (struct SkinConductionTester*);
	uint16_t numberOfMeandrs;
	volatile uint32_t currentMeandr;
	uint32_t meandrPeriod;
	uint32_t pwmPeriod;

	uint16_t numberOfBursts;
	volatile uint32_t currentBurst;
	uint32_t burstActive;
	volatile uint16_t burstPeriod;

	uint32_t numberOfPulses;
	uint32_t currentPulse;
	uint32_t pulsePause;
	uint32_t reactionPause;

	struct Channel
	{
		uint16_t amplitude;
		volatile uint32_t* pin;
	} channel[SOURCESNUM];
	uint16_t reactionTime;
	uint16_t maxReactionTime;//in ms
}SkinConductionTester;

void ConstructDiode(SkinConductionTester* source);
//bool IsPracue();
void SkinConductionStart(SkinConductionTester* skinTester);
void SkinConductionStop(SkinConductionTester* source);
void SkinConductionEnd(SkinConductionTester* skinTester);
void SkinConduction_send_result_to_PC(SkinConductionTester* skinTester);

#endif /* SKIN_CONDUCTION */
