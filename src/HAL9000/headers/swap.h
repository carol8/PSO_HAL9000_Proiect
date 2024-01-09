#include "HAL9000.h"
#include "data_type.h"
#include "process_defs.h"

typedef struct _SwapSPT {
	LIST_ENTRY SwapSPTListElem;
	// The PID of the process
	PID ProcessPID;
	// The virtual address of the swapped page
	PVOID SwappedAddress;
	// The location of the page in the swap file
	QWORD SwappedFileLocation;
} SwapSPT, *PSwapSPT;

typedef struct _SwapPageList {
	LIST_ENTRY SwapPageListElem;
	PHYSICAL_ADDRESS pa;
	PVOID va;
} SwapPageList, *PSwapPageList;

STATUS
SwapSystemInit(
);

STATUS
SwapPageListInsert(
	PHYSICAL_ADDRESS pa,
	PVOID va
);

STATUS
SwapPageListGetVictim(
	OUT	SwapPageList* victim
);

//STATUS
//SwapPageListGetAccesed(
//	PVOID va
//);

STATUS
SwapPageListResetAccesed(
	void
);

STATUS
SwapPageListRemove(
	PVOID va
);


STATUS
SwapPageOut(

);

STATUS
SwapPageIn(

);