#include "HAL9000.h"
#include "data_type.h"
#include "process_defs.h"

typedef struct _SwapSPT {
	LIST_ENTRY SwapSPTListElem;
	// The PID of the process
	//dc trebuie?
	PID ProcessPID;
	// The virtual address of the swapped page
	PVOID SwappedAddress;
	// The location of the page in the swap file (multiple of PAGE_SIZE)
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
	IN	PHYSICAL_ADDRESS	pa,
	IN	PVOID				va
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
	IN	PVOID				va
);

STATUS
SwapSPTInsert(
	IN	PVOID				SwappedAddress,
	IN	QWORD				SwappedFileLocation
);

STATUS
SwapSPTSearch(
	IN	PVOID				SwappedAddress,
	OUT	PSwapSPT*			PSwapSPTEntry
);

STATUS
SwapSPTDelete(
	IN	PVOID				SwappedAddress
);


STATUS
SwapPageOut(
	OUT	PHYSICAL_ADDRESS*	ppa
);

STATUS
SwapPageIn(
	IN	PSwapSPT			swapSPTEntry
);