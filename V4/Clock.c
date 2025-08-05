// V1
#include "Clock.h"
#include "Processor.h"
#include "ComputerSystem.h"

int tics = 0;

void Clock_Update()
{
	tics += 1;
	// ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 109, SHORTTERMSCHEDULE, tics);
	//  -- V2 - 1d -------------------------------------------------------------
	if (tics % intervalBetweenInterrupts == 0)
	{
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
}

int Clock_GetTime()
{
	return tics;
}
