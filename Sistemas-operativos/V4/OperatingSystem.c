#include "OperatingSystem.h"
#include "OperatingSystemBase.h"
#include "MMU.h"
#include "Processor.h"
#include "Buses.h"
#include "Heap.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// Functions prototypes
void OperatingSystem_PrepareDaemons();
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_ShowTime(char);
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_ExtractFromRunToBlock();
void OperatingSystem_ExtractFromBlockToReady();
int OperatingSystem_GetExecutingProcessID();
void OperatingSystem_ReleaseMainMemory();
void asignarParticion();

//Ejercicio 10
char * statesNames[5] = { "NEW","READY","EXECUTING","BLOCKED","EXIT" };

//Ejercicio 14
int numberOfSuccessfullyCreatedProcesses;

//Ejercicio 4 V2
int numberOfClockInterrupts=0;

//Ejercicio 5 d V2
int absAcumulador;

// Ejercicio 5 b V2
// Heap with blocked processes sort by when to wakeup
int sleepingProcessesQueue[PROCESSTABLEMAXSIZE];
int numberOfSleepingProcesses = 0;

// The process table
PCB processTable[PROCESSTABLEMAXSIZE];

// Address base for OS code in this version
int OS_address_base = PROCESSTABLEMAXSIZE * MAINMEMORYSECTIONSIZE;

// Identifier of the current executing process
int executingProcessID=NOPROCESS;
// Ejercicio 1 V3
int OperatingSystem_GetExecutingProcessID(){
	return executingProcessID;
}

// Identifier of the System Idle Process
int sipID;

// Begin indes for daemons in programList
int baseDaemonsInProgramList; 

//Ejercicio 11
// Array that contains the identifiers of the READY processes
int readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = { 0,0 };
char * queueNames[NUMBEROFQUEUES] = { "USER","DAEMONS" };

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses=0;

//Ejercicio 5 V4
int numParticionesLeidas;

//Variable auxiliar ara poder mostrar el nombre del programa...
//char * auxNombrePrograma;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int daemonsIndex) {
	
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// Ejercicio 5 V4
	numParticionesLeidas=OperatingSystem_InitializePartitionTable();

	// Obtain the memory requirements of the program
	int processSize=OperatingSystem_ObtainProgramSize(&programFile, "OperatingSystemCode");

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);
	
	// Process table initialization (all entries are free)
	for (i=0; i<PROCESSTABLEMAXSIZE;i++)
		processTable[i].busy=0;
	
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base+1);

	// Create all system daemon processes
	OperatingSystem_PrepareDaemons(daemonsIndex);
	
	//Ejercicio 0 c V3
	ComputerSystem_FillInArrivalTimeQueue();
	OperatingSystem_PrintStatus();

	// Create all user processes from the information given in the command line
	numberOfSuccessfullyCreatedProcesses=OperatingSystem_LongTermScheduler();
	// Ejercicio 4 a V3
	if(numberOfNotTerminatedUserProcesses<=0 && numberOfProgramsInArrivalTimeQueue<=0)
		OperatingSystem_ReadyToShutdown();

	//Ejercicio 14
	/*
	if (numberOfSuccessfullyCreatedProcesses <= 1) {
		OperatingSystem_ReadyToShutdown();
	}
	*/
	
	if (strcmp(programList[processTable[sipID].programListIndex]->executableName,"SystemIdleProcess")) {
	    OperatingSystem_ShowTime(SHUTDOWN);
		// Show message "ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(21,SHUTDOWN);
		exit(1);		
	}

	// At least, one user process has been created
	// Select the first process that is going to use the processor
	selectedProcess=OperatingSystem_ShortTermScheduler();

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);
}

// Daemon processes are system processes, that is, they work together with the OS.
// The System Idle Process uses the CPU whenever a user process is able to use it
void OperatingSystem_PrepareDaemons(int programListDaemonsBase) {
  
	// Include a entry for SystemIdleProcess at 0 position
	programList[0]=(PROGRAMS_DATA *) malloc(sizeof(PROGRAMS_DATA));

	programList[0]->executableName="SystemIdleProcess";
	programList[0]->arrivalTime=0;
	programList[0]->type=DAEMONPROGRAM; // daemon program

	sipID=INITIALPID%PROCESSTABLEMAXSIZE; // first PID for sipID

	// Prepare aditionals daemons here
	// index for aditionals daemons program in programList
	baseDaemonsInProgramList=programListDaemonsBase;

}


// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the 
// 			command lineand daemons programs
int OperatingSystem_LongTermScheduler() {
  
	int PID, numberOfSuccessfullyCreatedProcesses=0;
	int i;
	
	//for (i = 0; programList[i] != NULL && i < PROGRAMSMAXNUMBER; i++) {
	while(OperatingSystem_IsThereANewProgram()==1){
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);

		//Ejercicio 4 b
		PROGRAMS_DATA *executableProgram = programList[i];
		switch (PID) {
			case PROGRAMDOESNOTEXIST:
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(104, ERROR, executableProgram->executableName,"--- it does not exist ---");
				break;
			case PROGRAMNOTVALID:
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(104, ERROR, executableProgram->executableName, "--- invalid priority or size ---");
				break;
			case NOFREEENTRY:
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(103, ERROR, executableProgram->executableName);
				break;
			case TOOBIGPROCESS:
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(105, ERROR, executableProgram->executableName);
				break;
			case MEMORYFULL:
				OperatingSystem_ShowTime(SYSPROC);
				ComputerSystem_DebugMessage(144, ERROR, executableProgram->executableName);
				break;
			default:
				numberOfSuccessfullyCreatedProcesses++;
				//if (programList[i]->type == USERPROGRAM)
				if(processTable[PID].queueID== USERPROCESSQUEUE)//Ejercicio 11 a (hacer que se utilize el atributo queueID del PCB)
					numberOfNotTerminatedUserProcesses++;
				// Move process to the ready state
				OperatingSystem_MoveToTheREADYState(PID);
			    //OperatingSystem_PrintStatus();//lo comento porque ya lo hace el movetoreadystate
		}
		i++;
	}

	if(numberOfSuccessfullyCreatedProcesses>0)
		OperatingSystem_PrintStatus();

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}


// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram) {
  
	int PID;
	int processSize;
	int loadingPhysicalAddress;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram=programList[indexOfExecutableProgram];

	int valorDeRetornoLoadProgram;

	// Obtain a process ID
	PID=OperatingSystem_ObtainAnEntryInTheProcessTable();

	//Ejercicio 4 a (Bien)
	if (PID == NOFREEENTRY)
		return PID;

	// Obtain the memory requirements of the program
	processSize=OperatingSystem_ObtainProgramSize(&programFile, executableProgram->executableName);	

	//Ejercicio 5 a
	if (processSize == PROGRAMDOESNOTEXIST)
		return PROGRAMDOESNOTEXIST;

	//Ejercicio 5 b
	if (processSize == PROGRAMNOTVALID)
		return PROGRAMNOTVALID;
	
	// Obtain the priority for the process
	priority=OperatingSystem_ObtainPriority(programFile);

	//Ejercicio 5 b
	if (priority == PROGRAMNOTVALID)
		return PROGRAMNOTVALID;
	

	//Ejercicio 6 b V4
	OperatingSystem_ShowTime(SYSMEM);
	ComputerSystem_DebugMessage(142,SYSMEM,PID,executableProgram->executableName,processSize);


	//Ejercicio 6 V4 
	int identificador;
	//auxNombrePrograma=executableProgram->executableName;
	// Obtain enough memory space
 	identificador=OperatingSystem_ObtainMainMemory(processSize, PID);

	//Ejercicio 6
	if (identificador == TOOBIGPROCESS)
		return TOOBIGPROCESS;
	if (identificador==MEMORYFULL)
		return MEMORYFULL;
	loadingPhysicalAddress=partitionsTable[identificador].initAddress;

	// Load program in the allocated memory
	valorDeRetornoLoadProgram=OperatingSystem_LoadProgram(programFile, loadingPhysicalAddress, processSize);

	//Ejercicio 7
	if (valorDeRetornoLoadProgram == TOOBIGPROCESS)
		return TOOBIGPROCESS;

	
	// PCB initialization
	OperatingSystem_PCBInitialization(PID, loadingPhysicalAddress, processSize, priority, indexOfExecutableProgram);
	
	//Ya no hay ninguna posibilidad de que el proceso no se cree correctamente
	//además, ya se ha inicializado el PCB por lo que podemos acceder a los nombres de los programas
	asignarParticion(identificador,PID);

	// Show message "Process [PID] created from program [executableName]\n"
	OperatingSystem_ShowTime(INIT);
	ComputerSystem_DebugMessage(22,INIT,PID,executableProgram->executableName);
	
	return PID;
}


// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID) {
	/*
 	if (processSize>MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;
	*/

	//Ejercicio 6 a V4
	int i=0;
	int memoriaLlena=0;
	int mejorAjuste=-1;

	while(i<PARTITIONTABLEMAXSIZE){

		//Particion ocupada
		if(partitionsTable[i].occupied==1){

			//Proceso cabe
			if(processSize<=partitionsTable[i].size)
				memoriaLlena++;//Hay al menos una particion en la que cabe el proceso pero ya está llena

		}

		//Particion libre
		else{

			//Proceso cabe y no hay ninguna opción mejor o ésta opción es mejor que la anterior
			if(processSize<=partitionsTable[i].size && (  mejorAjuste==-1 || partitionsTable[mejorAjuste].size>partitionsTable[i].size ) )
				mejorAjuste=i;
		}

		i++;
	}

	//Se ha encontrado una partición para el proceso
	if(mejorAjuste>=0){
		//Ésto sólo se debe hacer en el caso de que el proceso se haya creado correctamente. Si lo hacemos aquí, el loadProgram podría dar error toobigprocess y la partición ya estaría inicializada
		/*
		//Asignar la particion al proceso
		partitionsTable[mejorAjuste].occupied=1;
		partitionsTable[mejorAjuste].PID=PID;

		//Ejercicio 6 c V4
		OperatingSystem_ShowTime(SYSMEM);
		ComputerSystem_DebugMessage(143,SYSMEM,mejorAjuste,partitionsTable[mejorAjuste].initAddress,partitionsTable[mejorAjuste].size, PID,programList[processTable[PID].programListIndex]->executableName);
		*/

		return mejorAjuste;
	}
	//No se ha encontrado
	else
		if(memoriaLlena>0)
			return MEMORYFULL;
		else
			return TOOBIGPROCESS;
		
 	//return PID*MAINMEMORYSECTIONSIZE;
}
void asignarParticion(int mejorAjuste, int PID){
		OperatingSystem_ShowPartitionTable("before allocating memory");

		//Asignar la particion al proceso
		partitionsTable[mejorAjuste].occupied=1;
		partitionsTable[mejorAjuste].PID=PID;

		//Ejercicio 6 c V4
		OperatingSystem_ShowTime(SYSMEM);
		ComputerSystem_DebugMessage(143,SYSMEM,mejorAjuste,partitionsTable[mejorAjuste].initAddress,partitionsTable[mejorAjuste].size, PID,programList[processTable[PID].programListIndex]->executableName);

		//Movemos aquí el mensaje de que se ha creado el proceso, ya no está en la inicialización del pcb
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(111, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[processTable[PID].state]);

		OperatingSystem_ShowPartitionTable("after allocating memory");
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex) {

	processTable[PID].busy=1;
	processTable[PID].initialPhysicalAddress=initialPhysicalAddress;
	processTable[PID].processSize=processSize;
	processTable[PID].state=NEW;
	processTable[PID].priority=priority;
	processTable[PID].programListIndex=processPLIndex;

	//Ejercicio 10
	//OperatingSystem_ShowTime(SYSPROC);
	//ComputerSystem_DebugMessage(111, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[processTable[PID].state]);

	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM) {//Comprobamos el tipo en programList[], para luego inicializar el atributo queueID del PCB
		processTable[PID].copyOfPCRegister=initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister= ((unsigned int) 1) << EXECUTION_MODE_BIT;

		//Asignar tipo (Ejercicio 11 a)
		processTable[PID].queueID = DAEMONSQUEUE;
	} 
	else {
		processTable[PID].copyOfPCRegister=0;
		processTable[PID].copyOfPSWRegister=0;

		processTable[PID].queueID = USERPROCESSQUEUE;
	}

}


// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID) {
	//int indiceListaProgramas = processTable[PID].programListIndex;
	//int tipo = programList[indiceListaProgramas]->type;
	int tipo = processTable[PID].queueID;//Ejercicio 11 a (hacer que se utilize el atributo queueID del PCB)

	if (Heap_add(PID, readyToRunQueue[tipo],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[tipo] ,PROCESSTABLEMAXSIZE)>=0) {
		int estadoAnterior = processTable[PID].state;
		processTable[PID].state=READY;

		//Ejercicio 10
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[estadoAnterior], statesNames[processTable[PID].state]);

	}
	//OperatingSystem_PrintReadyToRunQueue();
}


// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
//Decidir si vamos a sacar un proceso se hace aqui, no en OperatingSystem_ExtractFromReadyToRun
int OperatingSystem_ShortTermScheduler() {
	
	int selectedProcess;
	if(numberOfReadyToRunProcesses[USERPROCESSQUEUE] > 0)
		selectedProcess = OperatingSystem_ExtractFromReadyToRun(0);
	else
		selectedProcess = OperatingSystem_ExtractFromReadyToRun(1);

	return selectedProcess;
}


// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun(int tipoCola) {
  
	int selectedProcess=NOPROCESS;

	selectedProcess=Heap_poll(readyToRunQueue[tipoCola],QUEUE_PRIORITY ,&numberOfReadyToRunProcesses[tipoCola]);
	
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess; 
}

//Ejercicio 5 d V2
void OperatingSystem_ExtractFromRunToBlock() {
	int estadoAnterior;
	int selectedProcess;

	//Calculamos los campos whenToWakeUp
	//Calculamos valor absoluto del acumulador
	absAcumulador=abs(Processor_GetAccumulator());

	//Establecemos el valor de whenToWakeUp
	processTable[executingProcessID].whenToWakeUp = absAcumulador + numberOfClockInterrupts + 1;


	//A�adimos el proceso a la lista de dormidos
	if (Heap_add(executingProcessID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses, PROCESSTABLEMAXSIZE) >= 0) {

		//Guardamos el contexto del proceso en ejecuci�n
	    OperatingSystem_SaveContext(executingProcessID);

		//Cambiamos el estado y lo imprimimos por pantalla
		estadoAnterior = processTable[executingProcessID].state;
		processTable[executingProcessID].state = BLOCKED;
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[estadoAnterior], statesNames[processTable[executingProcessID].state]);
	
		//Le quitamos el procesador
	    executingProcessID=NOPROCESS;
		
		// Select the next process to execute (sipID if no more user processes)
		selectedProcess = OperatingSystem_ShortTermScheduler();
		// Assign the processor to that process
		OperatingSystem_Dispatch(selectedProcess);
	}


}

//Ejercicio 6 a V2
void OperatingSystem_ExtractFromBlockToReady() {

	//Calculamos el pid del proceso a extraer, el primero de la cola
	int procesoAExtraer;
	procesoAExtraer = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);

	//Lo movemos a ready
	OperatingSystem_MoveToTheREADYState(procesoAExtraer);

	//Ejercicio 8
	//OperatingSystem_PrintStatus();//lo comento porque sale repetido
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID) {

	// The process identified by PID becomes the current executing process
	executingProcessID=PID;
	// Change the process' state
	int estadoAnterior = processTable[PID].state;
	processTable[PID].state=EXECUTING;

	//Ejercicio 10
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110, SYSPROC, PID, programList[processTable[PID].programListIndex]->executableName, statesNames[estadoAnterior], statesNames[processTable[PID].state]);
	
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}


// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID) {
  
	// New values for the CPU registers are obtained from the PCB
	Processor_CopyInSystemStack(MAINMEMORYSIZE-1,processTable[PID].copyOfPCRegister);
	Processor_CopyInSystemStack(MAINMEMORYSIZE-2,processTable[PID].copyOfPSWRegister);

	// Ejercicio 13 ahora lo restauramos
	Processor_SetAccumulator(processTable[PID].copyOfAccumulatorRegister);
	
	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);
}


// Function invoked when the executing process leaves the CPU 
void OperatingSystem_PreemptRunningProcess() {

	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID=NOPROCESS;
}


// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID) {
	
	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-1);
	
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister=Processor_CopyFromSystemStack(MAINMEMORYSIZE-2);

	// Ejercicio 13 sera necesario guardar tambien el registro acumulador, ya que de lo contrario se machacaria
	processTable[PID].copyOfAccumulatorRegister = Processor_GetAccumulator();
	
}


// Exception management routine
void OperatingSystem_HandleException() {
	// Ejercicio 2 V4
	switch(Processor_GetRegisterB()){
		case DIVISIONBYZERO:
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,"division by zero");
		break;

		case INVALIDPROCESSORMODE:
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,"invalid processor mode");
		break;

		case INVALIDADDRESS:
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,"invalid address");
		break;

		case INVALIDINSTRUCTION:
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(140,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,"invalid instruction");
		break;
	}
	
	OperatingSystem_TerminateProcess();
	
	//Ejercicio 7 c
	OperatingSystem_PrintStatus();
}


// All tasks regarding the removal of the process
void OperatingSystem_TerminateProcess() {
  
	int selectedProcess;
  	
	//Ejercicio 10 cambios
	int estadoAnterior = processTable[executingProcessID].state;

	processTable[executingProcessID].state=EXIT;

	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(110, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, statesNames[estadoAnterior], statesNames[processTable[executingProcessID].state]);
	
	//Ejercicio 7 V4
	OperatingSystem_ReleaseMainMemory();

	//if (programList[processTable[executingProcessID].programListIndex]->type==USERPROGRAM) 
		if(processTable[executingProcessID].queueID==USERPROCESSQUEUE)//Ejercicio 11 a (hacer que se utilize el atributo queueID del PCB)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;
	
	//Ejercicio 4 c V3
	if (numberOfNotTerminatedUserProcesses<=0 && numberOfProgramsInArrivalTimeQueue<=0) {
		// Simulation must finish 
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	selectedProcess=OperatingSystem_ShortTermScheduler();

	//Para que no se meta en un bucle infinito
	/*if(selectedProcess==-1)
		OperatingSystem_Dispatch(3);
	else
		// Assign the processor to that process
		OperatingSystem_Dispatch(selectedProcess);*/
	OperatingSystem_Dispatch(selectedProcess);
}

// System call management routine
void OperatingSystem_HandleSystemCall() {
  
	int systemCallID;

	// Register A contains the identifier of the issued system call
	systemCallID=Processor_GetRegisterA();
	
	switch (systemCallID) {
		//Declaracion de variables que usaremos en el caso SYSCALL_YIELD
		int prioridadProcesoEnEjecucion;
		int tipoProcesoEnEjecucion;
		int pidDelMasPrioritarioDeLaColaDeListos;
		int prioridadDelMasPrioritarioDeLaColaDeListos;

		case SYSCALL_PRINTEXECPID:
	        OperatingSystem_ShowTime(SYSPROC);
			// Show message: "Process [executingProcessID] has the processor assigned\n"
			ComputerSystem_DebugMessage(24,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			break;

		case SYSCALL_END:
	        OperatingSystem_ShowTime(SYSPROC);
			// Show message: "Process [executingProcessID] has requested to terminate\n"
			ComputerSystem_DebugMessage(25,SYSPROC,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
			OperatingSystem_TerminateProcess();
			OperatingSystem_PrintStatus();
			break;

		case SYSCALL_YIELD:
			//Antes de quitar el proceso, hay que quitarlo de la cola de listos. Primero quitamos el que se esta ejecutando y luego pasamos el otro de listo a ejecutando, en ese orden.
			//Hallar prioridad del proceso en ejecucion
			prioridadProcesoEnEjecucion = processTable[executingProcessID].priority;

			//Hallar tipo del proceso en ejecucion
			tipoProcesoEnEjecucion = processTable[executingProcessID].queueID;

			//Hallar pid del proceso mas prioritario de la cola de listos
			pidDelMasPrioritarioDeLaColaDeListos = readyToRunQueue[tipoProcesoEnEjecucion][0];

			//Halla prioridad del proceso mas prioritario de la cola de listos del tipo del proceso en ejecucion
			prioridadDelMasPrioritarioDeLaColaDeListos = processTable[pidDelMasPrioritarioDeLaColaDeListos].priority;

			//Si no es el mismo proceso
			if (pidDelMasPrioritarioDeLaColaDeListos != executingProcessID) {
				//Si tienen la misma prioridad
				if (prioridadProcesoEnEjecucion == prioridadDelMasPrioritarioDeLaColaDeListos) {
					//Imprimimos el mensaje por pantalla
					OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
					ComputerSystem_DebugMessage(115, SHORTTERMSCHEDULE, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName,
						pidDelMasPrioritarioDeLaColaDeListos, programList[processTable[pidDelMasPrioritarioDeLaColaDeListos].programListIndex]->executableName);
					//El proceso en ejecucion abandona la cpu (hacemos algo parecido a lo que hace OperatingSystem_PreemptRunningProcess())

					//Guardamos el contexto del proceso que se esta ejecutando y va a ceder la cpu para que mas tarde pueda retomar su ejecucion
					OperatingSystem_PreemptRunningProcess();
					int pidADespachar = OperatingSystem_ExtractFromReadyToRun(tipoProcesoEnEjecucion);
					//Despachamos al proceso al que le ha cedido la cpu el que estaba en ejecucion
					OperatingSystem_Dispatch(pidADespachar);

					//Para que no se vuelva a ejecutar el proceso al que le han cedido la cpu...
					//numberOfReadyToRunProcesses[tipoProcesoEnEjecucion]--;
			        OperatingSystem_PrintStatus();
				}
			}
			break;

		//Ejercicio 5 d V2
		case SYSCALL_SLEEP:
			OperatingSystem_ExtractFromRunToBlock();
			OperatingSystem_PrintStatus();
			break;
		
		// Ejercicio 4 a v4
		default:
		OperatingSystem_ShowTime(SYSPROC);
		ComputerSystem_DebugMessage(141,INTERRUPT,executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName,systemCallID);
		//Ejercicio 4 b V4
		OperatingSystem_TerminateProcess();
		OperatingSystem_PrintStatus();

	}
}
	
//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint){
	switch (entryPoint){
		case SYSCALL_BIT: // SYSCALL_BIT=2
			OperatingSystem_HandleSystemCall();
			break;
		case EXCEPTION_BIT: // EXCEPTION_BIT=6
			OperatingSystem_HandleException();
			break;
		case CLOCKINT_BIT: //CLOCKINT_BIT=9
			OperatingSystem_HandleClockInterrupt();
			break;
	}

}

//Ejercicio 9
/*void OperatingSystem_PrintReadyToRunQueue() {
	OperatingSystem_ShowTime(SYSPROC);
	ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	int i;
	for (i=0; i < numberOfReadyToRunProcesses; i++) {
		int pidProceso = readyToRunQueue[i];
		int prioridadProceso = processTable[pidProceso].priority;
		if(i==numberOfReadyToRunProcesses-1)
			ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, pidProceso, prioridadProceso);
		else
			ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, pidProceso, prioridadProceso);
	}
}*/
//Ejercicio 11 b
void OperatingSystem_PrintReadyToRunQueue() {
	OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
	//ComputerSystem_DebugMessage(106, SHORTTERMSCHEDULE);
	ComputerSystem_DebugMessage(98, SHORTTERMSCHEDULE,"Ready-to-run processes queue:\n");
	int i;
	int j;
	for (j = 0; j < NUMBEROFQUEUES; j++) {
		ComputerSystem_DebugMessage(98, SHORTTERMSCHEDULE, "\t\t");
		ComputerSystem_DebugMessage(98, SHORTTERMSCHEDULE, queueNames[j]);
		ComputerSystem_DebugMessage(98, SHORTTERMSCHEDULE, ": ");
		if (numberOfReadyToRunProcesses[j] == 0)
			ComputerSystem_DebugMessage(98, SHORTTERMSCHEDULE,"\n");
		for (i = 0; i < numberOfReadyToRunProcesses[j]; i++) {
			int pidProceso = readyToRunQueue[j][i];
			int prioridadProceso = processTable[pidProceso].priority;
			if (i == numberOfReadyToRunProcesses[j] - 1)
				ComputerSystem_DebugMessage(108, SHORTTERMSCHEDULE, pidProceso, prioridadProceso);
			else
				ComputerSystem_DebugMessage(107, SHORTTERMSCHEDULE, pidProceso, prioridadProceso);
		}
	}
}

// In OperatingSystem.c Exercise 2-b of V2
void OperatingSystem_HandleClockInterrupt() {
	//Incrementamos el n�mero de interrupciones de reloj
	numberOfClockInterrupts++;

	//Si no est� en modo protejido imprimimos un tabulador
	if (!Processor_PSW_BitState(EXECUTION_MODE_BIT))
		ComputerSystem_DebugMessage(98, INTERRUPT,"\t");

	//Imprimimos el n� de interrupciones de reloj
	OperatingSystem_ShowTime(INTERRUPT);
	ComputerSystem_DebugMessage(120, INTERRUPT, numberOfClockInterrupts);

	//Comprobamos si hay procesos listos para despertarse
	int alMenosUno = 0;

	/**
	 * Tiene que ser con el while, porque con el for se produce bucle infinito ya que al extraer a ready el proceso, numberOfSleepingProcesses
	 * se reduce en 1, por lo tanto no itera numberOfSleepingProcesses veces, sino numberOfSleepingProcesses/2 veces y deja
	 * la mitad de procesos sin despertar
	 */
	while(processTable[Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses)].whenToWakeUp == numberOfClockInterrupts){
			OperatingSystem_ExtractFromBlockToReady();
			alMenosUno = 1;
	}
	// Ejercicio 3 a V3
	int procesosLTS=OperatingSystem_LongTermScheduler();
	// Ejercicio 4 b V3
	if(numberOfNotTerminatedUserProcesses<=0 && numberOfProgramsInArrivalTimeQueue<=0)
		OperatingSystem_ReadyToShutdown();

	/*int i;
	for (i = 0; i < numberOfSleepingProcesses; i++) {
		if (processTable[Heap_getFirst(sleepingProcessesQueue, numberOfSleepingProcesses)].whenToWakeUp == numberOfClockInterrupts) {
			OperatingSystem_ExtractFromBlockToReady();
			alMenosUno = 1;
		}
	}*/

	//Si hay al menos uno que se tiene que despertar, o al menos 1 que introdujo el LTS, se muestra el estado de todos los que haya
	if (alMenosUno==1||procesosLTS>=1){// Ejercicio 3 b V3
			OperatingSystem_PrintStatus();

		//Comprobar si hay algún proceso de la cola de listos con mayor prioridad que el que se está ejecutando
		int prioridadProcesoActual=processTable[executingProcessID].priority;
		int pidMasPrioritarioColaListos=Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE],numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
		
		//Si el proceso que saca al proceso que está ejecutandose de la cpu no es de usuario, será daemon
		if(pidMasPrioritarioColaListos==-1){
			pidMasPrioritarioColaListos=Heap_getFirst(readyToRunQueue[DAEMONSQUEUE],numberOfReadyToRunProcesses[DAEMONSQUEUE]);
		}
		int prioridadMasPrioritarioColaListos=processTable[pidMasPrioritarioColaListos].priority;
		
		//Si el más prioritario de listos tiene más prioridad
		if((prioridadProcesoActual>prioridadMasPrioritarioColaListos && processTable[executingProcessID].queueID == USERPROCESSQUEUE)
			|| processTable[executingProcessID].queueID == DAEMONSQUEUE){//if(tiene menos prioridad)

			//Imprimimos el mensaje
			OperatingSystem_ShowTime(SHORTTERMSCHEDULE);
			ComputerSystem_DebugMessage(121, SHORTTERMSCHEDULE, 
			executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, 
			pidMasPrioritarioColaListos, programList[processTable[pidMasPrioritarioColaListos].programListIndex]->executableName);

			//Salvamos el contexto del proceso actual y lo movemos a ready
			OperatingSystem_PreemptRunningProcess();

			// Select the next process to execute (sipID if no more user processes)
			int selectedProcess = OperatingSystem_ShortTermScheduler();
			// Assign the processor to that process
			OperatingSystem_Dispatch(selectedProcess);
			OperatingSystem_PrintStatus();

		}
	}
}

//Ejercicio 7 V4
void OperatingSystem_ReleaseMainMemory(){
	OperatingSystem_ShowPartitionTable("before releasing memory");

	int i=0;
	while(i<PARTITIONTABLEMAXSIZE){
		if(partitionsTable[i].occupied==1 && partitionsTable[i].PID==executingProcessID){
			partitionsTable[i].occupied=0;
			//partitionsTable[i].PID=-1;

			OperatingSystem_ShowTime(SYSMEM);
			ComputerSystem_DebugMessage(145, SYSMEM, i,partitionsTable[i].initAddress,partitionsTable[i].size,
			executingProcessID,programList[processTable[executingProcessID].programListIndex]->executableName);
		}
		i++;
	}

	OperatingSystem_ShowPartitionTable("after releasing memory");
}