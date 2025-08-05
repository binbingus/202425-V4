#include "Simulator.h"
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
void OperatingSystem_PCBInitialization(int, int, int, int, int);
void OperatingSystem_MoveToTheREADYState(int);
void OperatingSystem_Dispatch(int);
void OperatingSystem_RestoreContext(int);
void OperatingSystem_SaveContext(int);
void OperatingSystem_TerminateExecutingProcess();
int OperatingSystem_LongTermScheduler();
void OperatingSystem_PreemptRunningProcess();
int OperatingSystem_CreateProcess(int);
int OperatingSystem_ObtainMainMemory(int, int);
int OperatingSystem_ShortTermScheduler();
int OperatingSystem_ExtractFromReadyToRun();
void OperatingSystem_HandleException();
void OperatingSystem_HandleSystemCall();
void OperatingSystem_PrintReadyToRunQueue();
void OperatingSystem_YieldRunningProcess();
void OperatingSystem_HandleClockInterrupt();
void OperatingSystem_MoveToBlockedState(int);
int OperatingSystem_ExtractFromBlockedQueue();
void OperatingSystem_ReleaseMainMemory();
void OperatingSystem_CoalesceHoles();

// The process table
// PCB processTable[PROCESSTABLEMAXSIZE];
PCB *processTable;

// Size of the memory occupied for the OS
int OS_MEMORY_SIZE = 32;

// Address base for OS code in this version
int OS_address_base;

// Identifier of the current executing process
int executingProcessID = NOPROCESS;

// Identifier of the System Idle Process
int sipID;

// Initial PID for assignation (Not assigned)
int initialPID = -1;

// Begin indes for daemons in programList
// int baseDaemonsInProgramList;

// Array that contains the identifiers of the READY processes
// heapItem readyToRunQueue[NUMBEROFQUEUES][PROCESSTABLEMAXSIZE];
heapItem *readyToRunQueue[NUMBEROFQUEUES];
// -- V1 - 11a ----------------------------------------------------------------
int numberOfReadyToRunProcesses[NUMBEROFQUEUES] = {0, 0};
char *queueNames[NUMBEROFQUEUES] = {"USER", "DAEMONS"};

// Variable containing the number of not terminated user processes
int numberOfNotTerminatedUserProcesses = 0;

// char DAEMONS_PROGRAMS_FILE[MAXFILENAMELENGTH]="teachersDaemons";

int MAINMEMORYSECTIONSIZE = 60;

extern int MAINMEMORYSIZE;

int PROCESSTABLEMAXSIZE = 4;

// -- V1 - 10a ----------------------------------------------------------------
char *statesNames[5] = {"NEW", "READY", "EXECUTING", "BLOCKED", "EXIT"};

// -- V2 - 1e -----------------------------------------------------------------
int numberOfClockInterrupts = 0;

// -- V2 - 5b -----------------------------------------------------------------
heapItem *sleepingProcessesQueue;
int numberOfSleepingProcesses = 0;

// -- V4 - 6 ------------------------------------------------------------------
int hole = 0;

// Initial set of tasks of the OS
void OperatingSystem_Initialize(int programsFromFileIndex)
{
	int i, selectedProcess;
	FILE *programFile; // For load Operating System Code

	// In this version, with original configuration of memory size (300) and number of processes (4)
	// every process occupies a 60 positions main memory chunk
	// and OS code and the system stack occupies 60 positions

	OS_address_base = MAINMEMORYSIZE - OS_MEMORY_SIZE;

	MAINMEMORYSECTIONSIZE = OS_address_base / PROCESSTABLEMAXSIZE;

	// -- V1 - 8 --------------------------------------------------------------
	if (initialPID < 0) // if not assigned in options...
		initialPID = PROCESSTABLEMAXSIZE - 1;

	// Space for the processTable
	processTable = (PCB *)malloc(PROCESSTABLEMAXSIZE * sizeof(PCB));

	// Space for the ready to run queues (one queue initially...)
	readyToRunQueue[USERPROCESSQUEUE] = Heap_create(PROCESSTABLEMAXSIZE);
	readyToRunQueue[DAEMONSQUEUE] = Heap_create(PROCESSTABLEMAXSIZE);

	// -- V2 - 5b -------------------------------------------------------------
	sleepingProcessesQueue = Heap_create(PROCESSTABLEMAXSIZE);

	programFile = fopen("OperatingSystemCode", "r");
	if (programFile == NULL)
	{
		// Show red message "FATAL ERROR: Missing Operating System!\n"
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 99, SHUTDOWN, "FATAL ERROR: Missing Operating System!\n");
		exit(1);
	}

	// Obtain the memory requirements of the program
	int processSize = OperatingSystem_ObtainProgramSize(programFile);

	// Load Operating System Code
	OperatingSystem_LoadProgram(programFile, OS_address_base, processSize);

	// Process table initialization (all entries are free)
	for (i = 0; i < PROCESSTABLEMAXSIZE; i++)
	{
		processTable[i].busy = 0;
		processTable[i].programListIndex = -1;
		processTable[i].initialPhysicalAddress = -1;
		processTable[i].processSize = -1;
		processTable[i].copyOfSPRegister = -1;
		processTable[i].state = -1;
		processTable[i].priority = -1;
		processTable[i].copyOfPCRegister = -1;
		processTable[i].copyOfPSWRegister = 0;
		processTable[i].programListIndex = -1;
	}
	// Initialization of the interrupt vector table of the processor
	Processor_InitializeInterruptVectorTable(OS_address_base + 2);

	// Include in program list all user or system daemon processes
	OperatingSystem_PrepareDaemons(programsFromFileIndex);

	// -- V3 - 1c -------------------------------------------------------------
	ComputerSystem_FillInArrivalTimeQueue();
	// -- V3 - 1d -------------------------------------------------------------
	OperatingSystem_PrintStatus();

	// -- V4 - 5b -------------------------------------------------------------
	OperatingSystem_InitializePartitionsAndHolesTable(MAINMEMORYSIZE);

	// Create all user processes from the information given in the command line
	// -- V1 - 14 -------------------------------------------------------------
	int createdProcesses = OperatingSystem_LongTermScheduler();
	// -- V3 - 4a -------------------------------------------------------------
	if (createdProcesses <= 1 &&
		numberOfNotTerminatedUserProcesses <= 0 &&
		OperatingSystem_IsThereANewProgram() == EMPTYQUEUE)
	{
		// End simulation -> no created processes and none in arrival queue (V3)
		OperatingSystem_ReadyToShutdown();
	}

	if (strcmp(programList[processTable[sipID].programListIndex]->executableName, "SystemIdleProcess") && processTable[sipID].state == READY)
	{
		// Show red message "FATAL ERROR: Missing SIP program!\n"
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 99, SHUTDOWN, "FATAL ERROR: Missing SIP program!\n");
		exit(1);
	}

	// At least, one process has been created
	// Select the first process that is going to use the processor
	selectedProcess = OperatingSystem_ShortTermScheduler();

	Processor_SetSSP(MAINMEMORYSIZE - 1);

	// Assign the processor to the selected process
	OperatingSystem_Dispatch(selectedProcess);

	// Initial operation for Operating System
	Processor_SetPC(OS_address_base);

	// -- V2 - 3a -------------------------------------------------------------
	OperatingSystem_PrintStatus();
}

// The LTS is responsible of the admission of new processes in the system.
// Initially, it creates a process from each program specified in the
// 			command line and daemons programs
int OperatingSystem_LongTermScheduler()
{
	int PID, i,
		numberOfSuccessfullyCreatedProcesses = 0;

	// -- V3 - 2 --------------------------------------------------------------
	// Create processes while IsThereANewProgram == YES
	while (OperatingSystem_IsThereANewProgram() == YES)
	{
		i = Heap_poll(arrivalTimeQueue, QUEUE_ARRIVAL, &numberOfProgramsInArrivalTimeQueue);
		PID = OperatingSystem_CreateProcess(i);
		char *name = programList[i]->executableName;

		// -- V1 - 4b ---------------------------------------------------------
		if (PID == NOFREEENTRY)
		{
			numberOfSuccessfullyCreatedProcesses--;
			ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 50, ERROR, name);

			// -- V1 - 5b ---------------------------------------------------------
			// run: ./Simulator programVerySimple prog
		}
		else if (PID == PROGRAMDOESNOTEXIST)
		{
			numberOfSuccessfullyCreatedProcesses--;
			ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 51, ERROR, name, "it does not exist");

			// -- V1 - 5c ---------------------------------------------------------
			// run: ./Simulator programVerySimple prog-invalidSize
			// run: ./Simulator programVerySimple prog-invalidPrio
		}
		else if (PID == PROGRAMNOTVALID)
		{
			numberOfSuccessfullyCreatedProcesses--;
			ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 51, ERROR, name, "invalid priority or size");

			// -- V1 - 6b ---------------------------------------------------------
			// run: ./Simulator programVerySimple prog-tooBig
			// -- V1 - 7b ---------------------------------------------------------
			// run: ./Simulator programVerySimple prog-NoSpace
		}
		else if (PID == TOOBIGPROCESS)
		{
			numberOfSuccessfullyCreatedProcesses--;
			ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 52, ERROR, name);
		}
		else if (PID == MEMORYFULL)
		{
			numberOfSuccessfullyCreatedProcesses--;
			ComputerSystem_DebugMessage(TIMED_MESSAGE, 39, ERROR, name);
		}
		else
		{
			numberOfSuccessfullyCreatedProcesses++;
			if (programList[processTable[PID].programListIndex]->type == USERPROGRAM)
				numberOfNotTerminatedUserProcesses++;
			// Move process to the ready state
			OperatingSystem_MoveToTheREADYState(PID);

			// -- V2 - 3e -----------------------------------------------------
			OperatingSystem_PrintStatus();
		}
	}

	// Return the number of succesfully created processes
	return numberOfSuccessfullyCreatedProcesses;
}

// This function creates a process from an executable program
int OperatingSystem_CreateProcess(int indexOfExecutableProgram)
{
	int PID;
	int processSize;
	int partition;
	int priority;
	FILE *programFile;
	PROGRAMS_DATA *executableProgram = programList[indexOfExecutableProgram];

	// Obtain a process ID
	PID = OperatingSystem_ObtainAnEntryInTheProcessTable();

	// -- V1 - 4a -------------------------------------------------------------
	if (PID == NOFREEENTRY)
	{
		return NOFREEENTRY;
	}

	// Check if programFile exists
	programFile = fopen(executableProgram->executableName, "r");
	// -- V1 - 5b -------------------------------------------------------------
	if (programFile == NULL)
	{
		return PROGRAMDOESNOTEXIST;
	}

	// Obtain the memory requirements of the program
	processSize = OperatingSystem_ObtainProgramSize(programFile);

	// -- V1 - 5c -------------------------------------------------------------
	if (processSize == PROGRAMNOTVALID)
	{
		return PROGRAMNOTVALID;
	}

	// Obtain the priority for the process
	priority = OperatingSystem_ObtainPriority(programFile);

	// -- V1 - 5c -------------------------------------------------------------
	if (priority == PROGRAMNOTVALID)
	{
		return PROGRAMNOTVALID;
	}

	// -- V4 - 6b -------------------------------------------------------------
	char *name = executableProgram->executableName;
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 42, SYSMEM, PID, name, processSize);

	// Obtain enough memory space
	// -- V4 - 6c -------------------------------------------------------------
	partition = OperatingSystem_ObtainMainMemory(processSize, PID);
	// -- V1 - 6a -------------------------------------------------------------
	if (partition == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}

	if (partition == MEMORYFULL)
	{
		return MEMORYFULL;
	}

	int initAddress = partitionsAndHolesTable[partition].initAddress;
	int size = partitionsAndHolesTable[partition].size;
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 43, SYSMEM, partition,
								initAddress,
								size,
								PID,
								name);
	if (hole == 1)
	{
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 44, SYSMEM, numberOfPartitionsAndHoles - 1,
									partitionsAndHolesTable[numberOfPartitionsAndHoles - 1].initAddress,
									partitionsAndHolesTable[numberOfPartitionsAndHoles - 1].size,
									PID,
									name);
		hole = 0;
	}

	// Load program in the allocated memory
	// -- V1 - 7a -------------------------------------------------------------
	// -- V4 - 6c -------------------------------------------------------------
	if (OperatingSystem_LoadProgram(programFile, partitionsAndHolesTable[partition].initAddress, processSize) == TOOBIGPROCESS)
	{
		return TOOBIGPROCESS;
	}
	else
	{
		OperatingSystem_LoadProgram(programFile, partitionsAndHolesTable[partition].initAddress, processSize);
	}

	// PCB initialization
	// -- V4 - 6c -------------------------------------------------------------
	OperatingSystem_PCBInitialization(PID, partitionsAndHolesTable[partition].initAddress, processSize, priority, indexOfExecutableProgram);

	// -- V4 - 7 --------------------------------------------------------------
	OperatingSystem_ShowPartitionsAndHolesTable("after allocating memory");

	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 54, SYSPROC,
								PID,
								statesNames[NEW],
								programList[processTable[PID].programListIndex]->executableName);

	return PID;
}

// Main memory is assigned in chunks. All chunks are the same size. A process
// always obtains the chunk whose position in memory is equal to the processor identifier
int OperatingSystem_ObtainMainMemory(int processSize, int PID)
{
	// -- V4 - 7 --------------------------------------------------------------
	OperatingSystem_ShowPartitionsAndHolesTable("before allocating memory");
	if (processSize > MAINMEMORYSECTIONSIZE)
		return TOOBIGPROCESS;

	// -- V4 - 6a -------------------------------------------------------------
	// Best Fit strategy:
	// 	Chooses smallest partition where the process fits

	int i;
	int partSize = 0;
	int partPID = 0;
	int partition = -1;

	for (i = 0; i < numberOfPartitionsAndHoles; i++)
	{
		partSize = partitionsAndHolesTable[i].size;
		partPID = partitionsAndHolesTable[i].PID;
		if (partSize > processSize && partPID == HOLE) // process fits and partition is empty
		{
			partition = i;
			break;
		}
	}

	// if partition found
	if (partition != -1)
	{
		partitionsAndHolesTable[partition].PID = PID;

		// Check if new hole is generated
		int remainingSize = partSize - processSize;
		// if there is remaining available space
		if (remainingSize > 0)
		{
			// new HOLE
			hole = 1; // flag
			int address = partitionsAndHolesTable[partition].initAddress;
			int partIndex = numberOfPartitionsAndHoles;
			OperatingSystem_InsertIntopartitionsAndHolesTable(partIndex, HOLE, address + processSize, remainingSize);
		}
	}
	else
	{
		return MEMORYFULL;
	}

	return partition;
}

// Assign initial values to all fields inside the PCB
void OperatingSystem_PCBInitialization(int PID, int initialPhysicalAddress, int processSize, int priority, int processPLIndex)
{

	processTable[PID].busy = 1;
	processTable[PID].initialPhysicalAddress = initialPhysicalAddress;
	processTable[PID].processSize = processSize;
	processTable[PID].copyOfSPRegister = initialPhysicalAddress + processSize;
	processTable[PID].state = NEW;
	processTable[PID].priority = priority;
	processTable[PID].programListIndex = processPLIndex;
	// Daemons run in protected mode and MMU use real address
	if (programList[processPLIndex]->type == DAEMONPROGRAM)
	{
		processTable[PID].copyOfPCRegister = initialPhysicalAddress;
		processTable[PID].copyOfPSWRegister = ((unsigned int)1) << EXECUTION_MODE_BIT;
		processTable[PID].queueID = DAEMONSQUEUE; // -- V1 - 11c --
	}
	else
	{
		processTable[PID].copyOfPCRegister = 0;
		processTable[PID].copyOfPSWRegister = 0;
		processTable[PID].queueID = USERPROCESSQUEUE; // -- V1 - 11c --
	}

	// -- V2 - 0b -------------------------------------------------------------
	// -- V2 - 5h -------------------------------------------------------------
	processTable[PID].whenToWakeUp = -1;
}

// Move a process to the READY state: it will be inserted, depending on its priority, in
// a queue of identifiers of READY processes
void OperatingSystem_MoveToTheREADYState(int PID)
{
	int prevstate = processTable[PID].state;

	// User processes first
	if (processTable[PID].queueID == USERPROCESSQUEUE)
	{
		if (Heap_add(PID, readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[USERPROCESSQUEUE])) >= 0)
		{
			processTable[PID].state = READY;
			// -- V1 - 11b --------------------------------------------------------
			ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 53, SYSPROC,
										PID,
										programList[processTable[PID].programListIndex]->executableName,
										statesNames[prevstate],
										statesNames[READY]);
		}
	}

	else if ((Heap_add(PID, readyToRunQueue[DAEMONSQUEUE], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[DAEMONSQUEUE])) >= 0))
	{
		processTable[PID].state = READY;
		// -- V1 - 11b --------------------------------------------------------
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 53, SYSPROC,
									PID,
									programList[processTable[PID].programListIndex]->executableName,
									statesNames[prevstate],
									statesNames[READY]);
	}

	// -- V1 - 9b -------------------------------------------------------------
	// OperatingSystem_PrintReadyToRunQueue();
}

// The STS is responsible of deciding which process to execute when specific events occur.
// It uses processes priorities to make the decission. Given that the READY queue is ordered
// depending on processes priority, the STS just selects the process in front of the READY queue
int OperatingSystem_ShortTermScheduler()
{
	int selectedProcess;

	selectedProcess = OperatingSystem_ExtractFromReadyToRun();

	return selectedProcess;
}

// Return PID of more priority process in the READY queue
int OperatingSystem_ExtractFromReadyToRun()
{

	int selectedProcess = NOPROCESS;

	// -- V1 - 11c ------------------------------------------------------------
	if ((numberOfReadyToRunProcesses[USERPROCESSQUEUE]) > 0)
	{
		selectedProcess = Heap_poll(readyToRunQueue[USERPROCESSQUEUE], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[USERPROCESSQUEUE]));
	}
	else
	{
		selectedProcess = Heap_poll(readyToRunQueue[DAEMONSQUEUE], QUEUE_PRIORITY, &(numberOfReadyToRunProcesses[DAEMONSQUEUE]));
	}

	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// Function that assigns the processor to a process
void OperatingSystem_Dispatch(int PID)
{

	// The process identified by PID becomes the current executing process
	executingProcessID = PID;
	int state = processTable[PID].state;
	// Change the process' state
	processTable[PID].state = EXECUTING;
	// -- V1 - 11b --------------------------------------------------------
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 53, SYSPROC,
								PID,
								programList[processTable[PID].programListIndex]->executableName,
								statesNames[state],
								statesNames[EXECUTING]);
	// Modify hardware registers with appropriate values for the process identified by PID
	OperatingSystem_RestoreContext(PID);
}

// Modify hardware registers with appropriate values for the process identified by PID
void OperatingSystem_RestoreContext(int PID)
{
	// New values for the CPU registers are obtained from the PCB
	Processor_PushInSystemStack(processTable[PID].copyOfPCRegister);
	Processor_PushInSystemStack(processTable[PID].copyOfPSWRegister);
	Processor_SetRegisterSP(processTable[PID].copyOfSPRegister);

	// Same thing for the MMU registers
	MMU_SetBase(processTable[PID].initialPhysicalAddress);
	MMU_SetLimit(processTable[PID].processSize);

	Processor_SetAccumulator(processTable[PID].copyAccumulator);
	Processor_SetRegisterA(processTable[PID].copyA);
	Processor_SetRegisterB(processTable[PID].copyB);
}

// Function invoked when the executing process leaves the CPU
void OperatingSystem_PreemptRunningProcess()
{
	// Save in the process' PCB essential values stored in hardware registers and the system stack
	OperatingSystem_SaveContext(executingProcessID);
	// Change the process' state
	OperatingSystem_MoveToTheREADYState(executingProcessID);
	// The processor is not assigned until the OS selects another process
	executingProcessID = NOPROCESS;
}

// -- V1 - 12 -----------------------------------------------------------------
void OperatingSystem_YieldRunningProcess()
{
	// Check if process located at top of priority queue has SAME PRIORITY
	int queue = processTable[executingProcessID].queueID;
	int pid = Heap_getFirst(readyToRunQueue[queue], (numberOfReadyToRunProcesses[queue]));

	char *nameExec = programList[processTable[executingProcessID].programListIndex]->executableName;
	int execPID = executingProcessID;

	if (processTable[pid].priority == processTable[executingProcessID].priority && pid > -1)
	{
		// Back to READY state
		OperatingSystem_PreemptRunningProcess();

		char *nameNew = programList[processTable[pid].programListIndex]->executableName;
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 55, SHORTTERMSCHEDULE,
									execPID,
									nameExec,
									pid,
									nameNew);
		// Execute the new one and preempt the other
		int newPid = OperatingSystem_ShortTermScheduler();

		OperatingSystem_Dispatch(newPid);

		// -- V2 - 3b ---------------------------------------------------------
		OperatingSystem_PrintStatus();
	}
	else
	{
		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 56, SHORTTERMSCHEDULE,
									executingProcessID,
									nameExec);
	}
}

// Save in the process' PCB essential values stored in hardware registers and the system stack
void OperatingSystem_SaveContext(int PID)
{
	// Load PSW saved for interrupt manager
	processTable[PID].copyOfPSWRegister = Processor_PopFromSystemStack();

	// Load PC saved for interrupt manager
	processTable[PID].copyOfPCRegister = Processor_PopFromSystemStack();

	// Save RegisterSP
	processTable[PID].copyOfSPRegister = Processor_GetRegisterSP();

	// -- V1 - 13 -------------------------------------------------------------
	processTable[PID].copyAccumulator = Processor_GetAccumulator();

	processTable[PID].copyA = Processor_GetRegisterA();
	processTable[PID].copyB = Processor_GetRegisterB();
}

// Exception management routine
void OperatingSystem_HandleException()
{
	// -- V4 - 2 --------------------------------------------------------------
	char *message = " ";
	switch (Processor_GetRegisterD())
	{
	case DIVISIONBYZERO:
		message = "division by zero";
		break;
	case INVALIDADDRESS:
		message = "invalid address";
		break;
	case INVALIDINSTRUCTION:
		message = "invalid instruction";
		break;
	case INVALIDPROCESSORMODE:
		message = "invalid processor mode";
		break;
	}

	// Show message "Process [executingProcessID] has generated an exception and is terminating\n"
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 40, INTERRUPT, executingProcessID,
								programList[processTable[executingProcessID].programListIndex]->executableName,
								message);

	OperatingSystem_TerminateExecutingProcess();

	// -- V2 - 3d -------------------------------------------------------------
	OperatingSystem_PrintStatus();
}

// -- V4 - 8 ------------------------------------------------------------------
void OperatingSystem_ReleaseMainMemory()
{
	int partIndex;
	int i;
	for (i = 0; i < numberOfPartitionsAndHoles; i++)
	{
		if (partitionsAndHolesTable[i].PID == executingProcessID)
		{
			partIndex = i;
			break;
		}
	}

	// Release -> empty partition
	partitionsAndHolesTable[partIndex].PID = HOLE;

	int partAddress = partitionsAndHolesTable[partIndex].initAddress;
	int size = partitionsAndHolesTable[partIndex].size;
	char *name = programList[processTable[executingProcessID].programListIndex]->executableName;

	ComputerSystem_DebugMessage(TIMED_MESSAGE, 45, SYSMEM, partIndex, partAddress, size, executingProcessID, name);

	OperatingSystem_CoalesceHoles();
}

// -- V4 - 8 ------------------------------------------------------------------
void OperatingSystem_CoalesceHoles()
{
	// check if it is necessary to coalesce holes
	int i;
	int hole = 0;
	int coalesced = 0;
	for (i = numberOfPartitionsAndHoles; i > -1; i--)
	{
		if (partitionsAndHolesTable[i].PID == HOLE)
		{
			if (hole == 1)
			{
				// coalesce
				int newSize = partitionsAndHolesTable[i].size;
				int newAddress = partitionsAndHolesTable[i].initAddress;
				OperatingSystem_RemovePartitionOrHole(i + 1);
				OperatingSystem_RemovePartitionOrHole(i);
				OperatingSystem_InsertIntopartitionsAndHolesTable(i, HOLE, newAddress, newSize);
				coalesced = 1;
			}
			hole = 1;
		}
		else
		{
			hole = 0;
		}
	}

	if (coalesced > 0)
	{
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 144, SYSMEM);
	}
}

// All tasks regarding the removal of the executing process
void OperatingSystem_TerminateExecutingProcess()
{
	int state = processTable[executingProcessID].state;

	// -- V1 - 11b --------------------------------------------------------
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 53, SYSPROC,
								executingProcessID,
								programList[processTable[executingProcessID].programListIndex]->executableName,
								statesNames[state],
								statesNames[EXIT]);

	processTable[executingProcessID].state = EXIT;

	// -- V4 - 8 --------------------------------------------------------------
	OperatingSystem_ShowPartitionsAndHolesTable("before releasing memory");
	OperatingSystem_ReleaseMainMemory();
	OperatingSystem_ShowPartitionsAndHolesTable("after releasing memory");

	if (executingProcessID == sipID)
	{
		// finishing sipID, change PC to address of OS HALT instruction
		Processor_SetSSP(MAINMEMORYSIZE - 1);
		Processor_PushInSystemStack(OS_address_base + 1);
		Processor_PushInSystemStack(Processor_GetPSW());
		executingProcessID = NOPROCESS;
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 99, SHUTDOWN, "The system will shut down now...\n");
		return; // Don't dispatch any process
	}

	Processor_SetSSP(Processor_GetSSP() + 2); // unstack PC and PSW stacked

	if (programList[processTable[executingProcessID].programListIndex]->type == USERPROGRAM)
		// One more user process that has terminated
		numberOfNotTerminatedUserProcesses--;

	// -- V3 - 4c -------------------------------------------------------------
	if (numberOfNotTerminatedUserProcesses == 0 && OperatingSystem_IsThereANewProgram() == EMPTYQUEUE)
	{
		// If all processes are terminated AND there are no processes in arrival queue (V3)
		// Simulation must finish, telling sipID to finish
		OperatingSystem_ReadyToShutdown();
	}
	// Select the next process to execute (sipID if no more user processes)
	int selectedProcess = OperatingSystem_ShortTermScheduler();

	// Assign the processor to that process
	OperatingSystem_Dispatch(selectedProcess);
}

// -- V2 - 5f -----------------------------------------------------------------
void OperatingSystem_MoveToBlockedState(int PID)
{
	int actualState = processTable[PID].state;
	OperatingSystem_SaveContext(executingProcessID);

	if (Heap_add(PID, sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses) != -1)
	{
		processTable[PID].state = BLOCKED;
		char *name = programList[processTable[PID].programListIndex]->executableName;

		ComputerSystem_DebugMessage(TIMED_MESSAGE, 53, SYSPROC, PID,
									name,
									statesNames[actualState],
									statesNames[BLOCKED]);

		int delay = Processor_GetRegisterD();
		int wakeup;
		if (delay > 0)
			wakeup = delay + numberOfClockInterrupts + 1;
		else
			wakeup = abs(Processor_GetAccumulator()) + numberOfClockInterrupts + 1;

		processTable[executingProcessID].whenToWakeUp = wakeup;
	}
}

int OperatingSystem_ExtractFromBlockedQueue()
{
	int selectedProcess = NOPROCESS;

	if (numberOfSleepingProcesses > 0)
	{
		selectedProcess = Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses);
	}
	// Return most priority process or NOPROCESS if empty queue
	return selectedProcess;
}

// System call management routine
void OperatingSystem_HandleSystemCall()
{
	int systemCallID;
	char *name;

	// Register A contains the identifier of the issued system call
	systemCallID = Processor_GetRegisterC();

	switch (systemCallID)
	{
	case SYSCALL_PRINTEXECPID:
		// Show message: "Process [executingProcessID] has the processor assigned\n"
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 72, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName, Processor_GetRegisterA(), Processor_GetRegisterB());
		break;

	case SYSCALL_END:
		// Show message: "Process [executingProcessID] has requested to terminate\n"
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 73, SYSPROC, executingProcessID, programList[processTable[executingProcessID].programListIndex]->executableName);
		OperatingSystem_TerminateExecutingProcess();

		// -- V2 - 3c ---------------------------------------------------------
		OperatingSystem_PrintStatus();

		break;

	// -- V1 - 12 -------------------------------------------------------------
	case SYSCALL_YIELD:
		OperatingSystem_YieldRunningProcess();
		break;

	// -- V2 - 5f -------------------------------------------------------------
	case SYSCALL_SLEEP:
		OperatingSystem_MoveToBlockedState(executingProcessID);
		// Dispatch first ready process
		int process = OperatingSystem_ShortTermScheduler();
		OperatingSystem_Dispatch(process);

		// -- V2 - 5g ---------------------------------------------------------
		OperatingSystem_PrintStatus();

		break;

	default:
		// -- V4 - 4a ---------------------------------------------------------
		name = programList[processTable[executingProcessID].programListIndex]->executableName;
		ComputerSystem_DebugMessage(TIMED_MESSAGE, 41, INTERRUPT, executingProcessID, name, systemCallID);

		// -- V4 - 4b ---------------------------------------------------------
		OperatingSystem_TerminateExecutingProcess();

		// -- V4 - 4c ---------------------------------------------------------
		OperatingSystem_PrintStatus();

		break;
	}
}

//	Implement interrupt logic calling appropriate interrupt handle
void OperatingSystem_InterruptLogic(int entryPoint)
{
	switch (entryPoint)
	{
	case SYSCALL_BIT: // SYSCALL_BIT=2
		OperatingSystem_HandleSystemCall();
		break;
	case EXCEPTION_BIT: // EXCEPTION_BIT=6
		OperatingSystem_HandleException();
		break;
	case CLOCKINT_BIT:
		// -- V2 - 1c ---------------------------------------------------------
		OperatingSystem_HandleClockInterrupt();
		break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// -- V1 - 9a -----------------------------------------------------------------
void OperatingSystem_PrintReadyToRunQueue()
{
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 103, SHORTTERMSCHEDULE); // -> Ready-to-run processes queue:

	// USER PROCESSES
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 104, SHORTTERMSCHEDULE);			 // -> \t\t
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 108, SHORTTERMSCHEDULE, "USER: "); // -> USER:
	int i;
	for (i = 0; i < numberOfReadyToRunProcesses[USERPROCESSQUEUE]; i++)
	{
		int PID = readyToRunQueue[USERPROCESSQUEUE][i].info; // access ith element of the first queue
		unsigned int prio = processTable[PID].priority;

		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 105, SHORTTERMSCHEDULE,
									PID,
									prio); // [PID, prio],
	}
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 106, SHORTTERMSCHEDULE); // \n

	// DAEMONS
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 104, SHORTTERMSCHEDULE);				// -> \t\t
	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 108, SHORTTERMSCHEDULE, "DAEMONS: "); // -> DAEMONS:
	for (i = 0; i < numberOfReadyToRunProcesses[DAEMONSQUEUE]; i++)
	{
		int PID = readyToRunQueue[DAEMONSQUEUE][i].info; // access ith element of the first queue
		unsigned int prio = processTable[PID].priority;

		ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 105, SHORTTERMSCHEDULE,
									PID,
									prio); // [PID, prio],
	}

	ComputerSystem_DebugMessage(NO_TIMED_MESSAGE, 106, SHORTTERMSCHEDULE); // \n
}

///////////////////////////////////////////////////////////////////////////////
// -- V2 - 1b - 1e ------------------------------------------------------------
void OperatingSystem_HandleClockInterrupt()
{
	numberOfClockInterrupts++;
	ComputerSystem_DebugMessage(TIMED_MESSAGE, 57, INTERRUPT, numberOfClockInterrupts);

	int awakened = 0;
	int process;

	// -- V2 - 6a --------------------------------------------------------------
	while (numberOfSleepingProcesses > 0 &&
		   processTable[Heap_poll(sleepingProcessesQueue, QUEUE_WAKEUP, &numberOfSleepingProcesses)].whenToWakeUp == numberOfClockInterrupts)
	{
		awakened++;
		process = OperatingSystem_ExtractFromBlockedQueue();
		OperatingSystem_MoveToTheREADYState(process);
	}

	// -- V3 - 3a -------------------------------------------------------------
	int createdProcesses = OperatingSystem_LongTermScheduler();

	// -- V2 - 6b -------------------------------------------------------------
	// -- V3 - 3b -------------------------------------------------------------
	if ((awakened > 0) || (createdProcesses > 0))
	{
		// If awakened -> Print status
		OperatingSystem_PrintStatus();

		// -- V2 - 6c ---------------------------------------------------------
		// Check if running process has the most priority -> if not change with most prio one
		int runningPrio = processTable[executingProcessID].priority;
		char *runningName = programList[processTable[executingProcessID].programListIndex]->executableName;
		int runningPID = executingProcessID;

		if (processTable[Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE])].priority < runningPrio &&
			Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]) != -1)
		{
			int newPid = Heap_getFirst(readyToRunQueue[USERPROCESSQUEUE], numberOfReadyToRunProcesses[USERPROCESSQUEUE]);
			char *newName = programList[processTable[newPid].programListIndex]->executableName;

			OperatingSystem_PreemptRunningProcess(); // Move running process back to READY queue
			int PID = OperatingSystem_ShortTermScheduler();
			OperatingSystem_Dispatch(PID); // Execute the new one

			ComputerSystem_DebugMessage(TIMED_MESSAGE, 58, SHORTTERMSCHEDULE, runningPID, runningName, newPid, newName);

			// -- V2 - 6d -----------------------------------------------------
			OperatingSystem_PrintStatus();
		}
	}

	return;
}