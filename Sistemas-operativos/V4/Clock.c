#include "Clock.h"
#include "Processor.h"
// #include "ComputerSystem.h"
#define INTERVALBETWEENINTERRUPS 5

void OperatingSystem_InterruptLogic(int);

int tics=0;

void Clock_Update() {

	tics++;
    // ComputerSystem_DebugMessage(97,CLOCK,tics);

	if (tics % INTERVALBETWEENINTERRUPS == 0) {
		//OperatingSystem_InterruptLogic(CLOCKINT_BIT);
		Processor_RaiseInterrupt(CLOCKINT_BIT);
	}
}


int Clock_GetTime() {

	return tics;
}
