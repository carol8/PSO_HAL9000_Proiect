#pragma once

#include "HAL9000.h"
#include "mmu.h"
#include "int15.h"
#include "bitmap.h"
#include "synch.h"

#define PmmReserveMemory(Frames)        PmmReserveMemoryEx((Frames), NULL )

typedef struct _MEMORY_REGION_LIST
{
	MEMORY_MAP_TYPE     Type;
	DWORD               NumberOfEntries;
} MEMORY_REGION_LIST, * PMEMORY_REGION_LIST;

typedef struct _PMM_DATA
{
	// Both of the highest physical address values are setup on initialization and
	// remain unchanged, i.e. if the highest available address becomes later reserved
	// HighestPhysicalAddressAvailable will not change.

	// This points to the end of the highest physical address available in the system
	// i.e. this is not reserved and can be used
	// E.g: if the last available memory region starts at 0xFFFE'0000 and occupies a
	// page HighestPhysicalAddressAvailable will be 0xFFFE'1000
	PHYSICAL_ADDRESS    HighestPhysicalAddressAvailable;

	// This includes the memory already reserved in the system, it is greater than or
	// equal to HighestPhysicalAddressAvailable, depending on the arrangement of
	// memory in the system.
	PHYSICAL_ADDRESS    HighestPhysicalAddressPresent;

	// Total size of available memory over 1MB
	QWORD               PhysicalMemorySize;

	MEMORY_REGION_LIST  MemoryRegionList[MemoryMapTypeMax];

	LOCK                AllocationLock;

	_Guarded_by_(AllocationLock)
		BITMAP              AllocationBitmap;

	LOCK SwapSPTLock;
	_Guarded_by_(SwapSPTLock)
		LIST_ENTRY SwapSPTListHead;

	LOCK SwapPageListLock;
	_Guarded_by_(SwapPageListLock)
		LIST_ENTRY SwapPageListHead;

	LOCK SwapSecondChanceIndexLock;
	_Guarded_by_(SwapSecondChanceIndexLock)
		QWORD SwapSecondChanceIndex;

	LOCK SwapBitmapLock;
	_Guarded_by_(SwapBitmapLock)
		BITMAP SwapBitmap;

} PMM_DATA, * PPMM_DATA;

_No_competing_thread_
void
PmmPreinitSystem(
    void
    );

_No_competing_thread_
STATUS
PmmInitSystem(
    IN          PVOID                   BaseAddress,
    IN          PHYSICAL_ADDRESS        MemoryEntries,
    IN          DWORD                   NumberOfMemoryEntries,
    OUT         DWORD*                  SizeReserved
    );

PMM_DATA
GetPmmData(
    void
);

//******************************************************************************
// Function:     PmmRequestMemoryEx
// Description:  Reserves the first free frames available after MinPhysAddr.
// Returns:      PHYSICAL_ADDRESS - start address of physical address reserved
// Parameter:    IN DWORD NoOfFrames - frames to reserved.
// Parameter:    IN_OPT PHYSICAL_ADDRESS MinPhysAddr - physical address from
//               which to start searching for free frames.
//******************************************************************************
PTR_SUCCESS
PHYSICAL_ADDRESS
PmmReserveMemoryEx(
    IN          DWORD                   NoOfFrames,
    IN_OPT      PHYSICAL_ADDRESS        MinPhysAddr
    );

//******************************************************************************
// Function:     PmmReleaseMemory
// Description:  Releases previously reserved memory
// Returns:      void
// Parameter:    IN PHYSICAL_ADDRESS PhysicalAddr
// Parameter:    IN DWORD NoOfFrames
//******************************************************************************
void
PmmReleaseMemory(
    IN          PHYSICAL_ADDRESS        PhysicalAddr,
    IN          DWORD                   NoOfFrames
    );

//******************************************************************************
// Function:     PmmGetTotalSystemMemory
// Description:
// Returns:      QWORD - Returns the number of bytes of physical memory
//               available in the system.
// Parameter:    void
//******************************************************************************
QWORD
PmmGetTotalSystemMemory(
    void
    );

// Note: This address may reserved by the firmware or some other device.
// If you want to retrieve the highest available physical address for software
// usage use PmmGetHighestPhysicalMemoryAddressAvailable
PHYSICAL_ADDRESS
PmmGetHighestPhysicalMemoryAddressPresent(
    void
    );

PHYSICAL_ADDRESS
PmmGetHighestPhysicalMemoryAddressAvailable(
    void
    );
