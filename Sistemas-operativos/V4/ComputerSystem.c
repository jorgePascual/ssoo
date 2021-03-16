#include <stdio.h>
#include <stdlib.h>
#include "ComputerSystem.h"
#include "OperatingSystem.h"
#include "ComputerSystemBase.h"
#include "Processor.h"
#include "Messages.h"
#include "Asserts.h"

// Functions prototypes
void ComputerSystem_PrintProgramList();
int Clock_GetTime();

int arrivalTimeQueue[PROGRAMSMAXNUMBER];
int numberOfProgramsInArrivalTimeQueue=0;

// Array that contains basic data about all daemons
// and all user programs specified in the command line
PROGRAMS_DATA *programList[PROGRAMSMAXNUMBER];

// Powers on of the Computer System.
void ComputerSystem_PowerOn(int argc, char *argv[]) {

	// Load debug messages
	int nm=0;
	nm=Messages_Load_Messages(nm,TEACHER_MESSAGES_FILE);
	nm=Messages_Load_Messages(nm,STUDENT_MESSAGES_FILE);
	printf("%d Messages Loaded\n",nm);

	// Obtain a list of programs in the command line and debus sections
	int daemonsBaseIndex = ComputerSystem_ObtainProgramList(argc, argv);

	int na=Asserts_LoadAsserts();
	if (na==-1)
		// printf("Asserts file unavailable\n");
		ComputerSystem_DebugMessage(84,POWERON);
	else
		// printf("%d Asserts Loaded\n",na);
		ComputerSystem_DebugMessage(85,POWERON,na);

	//Ejercicio 2
	ComputerSystem_PrintProgramList();

	// Request the OS to do the initial set of tasks. The last one will be
	// the processor allocation to the process with the highest priority
	OperatingSystem_Initialize(daemonsBaseIndex);
	
	// Tell the processor to begin its instruction cycle 
	Processor_InstructionCycleLoop();
	
}

// Powers off the CS (the C program ends)
void ComputerSystem_PowerOff() {
	// Show message in red colour: "END of the simulation\n" 
	ComputerSystem_DebugMessage(Processor_PSW_BitState(EXECUTION_MODE_BIT) ? 5 : 4, SHUTDOWN, Clock_GetTime());
	ComputerSystem_DebugMessage(99,SHUTDOWN); 
	exit(0);
}

/////////////////////////////////////////////////////////
//  New functions below this line  //////////////////////

//Ejercicio 1
void ComputerSystem_PrintProgramList() {

	// Ejercicio 1 V2
	ComputerSystem_DebugMessage(Processor_PSW_BitState(EXECUTION_MODE_BIT) ? 5 : 4, INIT, Clock_GetTime());

	ComputerSystem_DebugMessage(101, INIT);
	int i;
	for (i = 1; i < PROGRAMSMAXNUMBER && programList[i] != NULL; i++) {//Mal, habr�a que poner programList.Lenght
		PROGRAMS_DATA *executableProgram = programList[i];
		ComputerSystem_DebugMessage(102, INIT, executableProgram->executableName, executableProgram->arrivalTime);
	}
	
}