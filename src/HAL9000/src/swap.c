#include "swap.h"
#include "pmm.h"
#include "vmm.h"

static PLIST_ENTRY lastVerifiedListEntry;

STATUS
SwapSystemInit(
) 
{
	PMM_DATA pmm_data = GetPmmData();
	lastVerifiedListEntry = &pmm_data.SwapPageListHead;
	return STATUS_SUCCESS;
}

STATUS
SwapPageListInsert(
	IN	PHYSICAL_ADDRESS	pa,
	IN	PVOID				va
)
{
	PMM_DATA pmm_data = GetPmmData();
	PLIST_ENTRY swapPageList = &pmm_data.SwapPageListHead;
	INTR_STATE dummy;
	PLIST_ENTRY it;

	LockAcquire(&pmm_data.SwapPageListLock, &dummy);

	LOG_TRACE_VMM("Inserting page with PA 0x%X and VA 0x%X into the list...\n", pa, va);

	PSwapPageList newElem = ExAllocatePoolWithTag(PoolAllocateZeroMemory, sizeof(SwapPageList), HEAP_PROCESS_TAG, 0);

	newElem->pa = pa;
	newElem->va = va;

	InsertTailList(swapPageList, &newElem->SwapPageListElem);

	LockRelease(&pmm_data.SwapPageListLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapPageListGetVictim(
	OUT	SwapPageList* victim
)
{
	PMM_DATA pmm_data = GetPmmData();
	INTR_STATE dummy;
	PLIST_ENTRY it;
	BOOLEAN accesed;
	PML4 cr3;
	cr3.Raw = __readcr3();

	LockAcquire(&pmm_data.SwapPageListLock, &dummy);

	LOG_TRACE_VMM("Getting next victim from list...\n");

	it = lastVerifiedListEntry->Flink;
	while (1) {
		PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);
		VmmGetPhysicalAddressEx(cr3, entry->va, &accesed, (void*)0);
		if (accesed == FALSE) {
			victim = entry;
			lastVerifiedListEntry = it;
			break;
		}
	}

	LockRelease(&pmm_data.SwapPageListLock, dummy);
	return STATUS_SUCCESS;
}

//STATUS
//SwapPageListGetAccesed(
//	IN	PVOID		va,
//	OUT	BOOLEAN*	accesed
//)
//{
//	PMM_DATA pmm_data = GetPmmData();
//	PLIST_ENTRY swapPageList = &pmm_data.SwapPageListHead;
//	INTR_STATE dummy;
//	PLIST_ENTRY it;
//	PML4 cr3;
//	cr3.Raw = __readcr3();
//
//	LockAcquire(&pmm_data.SwapPageListLock, &dummy);
//
//	LOG_TRACE_VMM("Getting page info for page with VA 0x%X...\n", va);
//
//	for (it = swapPageList->Flink; it != swapPageList; it = it->Flink) {
//		PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);
//		if (entry->va == va) {
//			break;
//		}
//	}
//
//	if (it == swapPageList) {
//		LOG_TRACE_VMM("Page with VA 0x%X not found!\n");
//		*accesed = FALSE;
//		return STATUS_ELEMENT_NOT_FOUND;
//	}
//		
//	LOG_TRACE_VMM("Page with VA 0x%X found, getting accesed bit...\n");
//	PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);
//	VmmGetPhysicalAddressEx(cr3, entry->va, accesed, (void*)0);
//	LOG_TRACE_VMM("Page with VA 0x%X has accesed bit = %d\n", *accesed);
//
//	LockRelease(&pmm_data.SwapPageListLock, dummy);
//
//	return STATUS_SUCCESS;
//}

STATUS
SwapPageListResetAccesed(
	void
)
{
	PMM_DATA pmm_data = GetPmmData();
	PLIST_ENTRY swapPageList = &pmm_data.SwapPageListHead;
	INTR_STATE dummy;
	PLIST_ENTRY it;
	BOOLEAN accesed;
	PML4 cr3;
	cr3.Raw = __readcr3();

	LockAcquire(&pmm_data.SwapPageListLock, &dummy);

	LOG_TRACE_VMM("Resetting page accesed for all pages...\n");

	for (it = swapPageList->Flink; it != swapPageList; it = it->Flink) {
		PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);
		VmmGetPhysicalAddressEx(cr3, entry->va, &accesed, (void*) 0);
	}

	LockRelease(&pmm_data.SwapPageListLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapPageListRemove(
	PVOID va
)
{
	PMM_DATA pmm_data = GetPmmData();
	PLIST_ENTRY swapPageList = &pmm_data.SwapPageListHead;
	INTR_STATE dummy;
	PLIST_ENTRY it;

	LockAcquire(&pmm_data.SwapPageListLock, &dummy);

	LOG_TRACE_VMM("Finding page with VA 0x%X...\n", va);

	for (it = swapPageList->Flink; it != swapPageList; it = it->Flink) {
		PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);
		if (entry->va == va) {
			break;
		}
	}

	if (it == swapPageList) {
		LOG_TRACE_VMM("Page with VA 0x%X not found!\n");
		return STATUS_ELEMENT_NOT_FOUND;
	}

	LOG_TRACE_VMM("Page with VA 0x%X found, deleting...\n");
	PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);

	RemoveEntryList(&entry->SwapPageListElem);

	ExFreePoolWithTag(entry, HEAP_PROCESS_TAG);

	LockRelease(&pmm_data.SwapPageListLock, dummy);

	return STATUS_SUCCESS;
}