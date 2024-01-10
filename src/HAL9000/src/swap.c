#include "swap.h"
#include "pmm.h"
#include "vmm.h"
#include "iomu.h"

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
	OUT	SwapPageList*		victim
)
{
	PMM_DATA pmm_data = GetPmmData();
	INTR_STATE dummy;
	PLIST_ENTRY it;
	BOOLEAN accesed;
	PML4 cr3;
	cr3.Raw = (QWORD) __readcr3();

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
	cr3.Raw = (QWORD) __readcr3();

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
		LOG_TRACE_VMM("Page with VA 0x%X not found!\n", va);
		LockRelease(&pmm_data.SwapPageListLock, dummy);
		return STATUS_ELEMENT_NOT_FOUND;
	}

	LOG_TRACE_VMM("Page with VA 0x%X found, deleting...\n", va);
	PSwapPageList entry = CONTAINING_RECORD(it, SwapPageList, SwapPageListElem);

	RemoveEntryList(&entry->SwapPageListElem);

	ExFreePoolWithTag(entry, HEAP_PROCESS_TAG);

	LockRelease(&pmm_data.SwapPageListLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapSPTInsert(
	IN	PVOID				SwappedAddress,
	IN	QWORD				SwappedFileLocation
)
{
	PMM_DATA pmm_data = GetPmmData();
	PLIST_ENTRY swapSPTList = &pmm_data.SwapSPTListHead;
	INTR_STATE dummy;

	LockAcquire(&pmm_data.SwapSPTLock, &dummy);

	LOG_TRACE_VMM("Inserting page with VA 0x%X and location 0x%X into the list...\n", SwappedAddress, SwappedFileLocation);

	PSwapSPT newElem = ExAllocatePoolWithTag(PoolAllocateZeroMemory, sizeof(SwapSPT), HEAP_PROCESS_TAG, 0);

	newElem->SwappedAddress = SwappedAddress;
	newElem->SwappedFileLocation = SwappedFileLocation;

	InsertTailList(swapSPTList, &newElem->SwapSPTListElem);

	LockRelease(&pmm_data.SwapSPTLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapSPTSearch(
	IN	PVOID				SwappedAddress,
	OUT	PSwapSPT*			PSwapSPTEntry
)
{
	PMM_DATA pmm_data = GetPmmData();
	PLIST_ENTRY swapSPTList = &pmm_data.SwapSPTListHead;
	INTR_STATE dummy;
	PLIST_ENTRY it;

	LockAcquire(&pmm_data.SwapSPTLock, &dummy);

	LOG_TRACE_VMM("Finding page with VA 0x%X...\n", SwappedAddress);

	for (it = swapSPTList->Flink; it != swapSPTList; it = it->Flink) {
		PSwapSPT entry = CONTAINING_RECORD(it, SwapSPT, SwapSPTListElem);
		if (entry->SwappedAddress == SwappedAddress) {
			break;
		}
	}

	if (it == swapSPTList) {
		LOG_TRACE_VMM("Page with VA 0x%X not found!\n", SwappedAddress);
		LockRelease(&pmm_data.SwapSPTLock, dummy);
		return STATUS_ELEMENT_NOT_FOUND;
	}

	LOG_TRACE_VMM("Page with VA 0x%X found!\n", SwappedAddress);
	PSwapSPT entry = CONTAINING_RECORD(it, SwapSPT, SwapSPTListElem);

	*PSwapSPTEntry = entry;

	LockRelease(&pmm_data.SwapSPTLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapSPTDelete(
	IN	PVOID				SwappedAddress
) 
{
	PMM_DATA pmm_data = GetPmmData();
	INTR_STATE dummy;
	PSwapSPT entry;
	STATUS status;
	status = SwapSPTSearch(SwappedAddress, &entry);
	if (status != STATUS_SUCCESS) {
		LOG_TRACE_VMM("Page with VA 0x%X not found, cannot delete!\n", SwappedAddress);
		return status;
	}

	LOG_TRACE_VMM("Page with VA 0x%X found, deleting...\n", SwappedAddress);
	
	LockAcquire(&pmm_data.SwapSPTLock, &dummy);

	RemoveEntryList(&entry->SwapSPTListElem);

	ExFreePoolWithTag(entry, HEAP_PROCESS_TAG);

	LockRelease(&pmm_data.SwapSPTLock, dummy);

	return STATUS_SUCCESS;
}

STATUS
SwapPageOut(
	OUT	PHYSICAL_ADDRESS*	ppa
)
{
	INTR_STATE dummy;
	PMM_DATA pmm_data = GetPmmData();
	PFILE_OBJECT swapFile = IomuGetSwapFile();
	SwapPageList victim;
	QWORD index, fileOffset;
	QWORD bytesWritten;
	STATUS status;


	SwapPageListGetVictim(&victim);

	LockAcquire(&pmm_data.SwapBitmapLock, &dummy);
	for (index = 0; BitmapGetBitValue(&pmm_data.SwapBitmap, (DWORD) index); index++);
	LockRelease(&pmm_data.SwapBitmapLock, dummy);
	fileOffset = index * PAGE_SIZE;

	status = IoWriteFile(swapFile,
		PAGE_SIZE,
		&fileOffset,
		victim.va,
		&bytesWritten);

	if (status != STATUS_SUCCESS) {
		return status;
	}

	BitmapSetBitValue(&pmm_data.SwapBitmap, (DWORD) index, TRUE);
	SwapSPTInsert(victim.va, fileOffset);
	MmuUnmapMemoryEx(victim.va, PAGE_SIZE, TRUE, NULL);

	*ppa = victim.pa;

	return STATUS_SUCCESS;
}

STATUS
SwapPageIn(
	IN	PSwapSPT			swapSPTEntry
)
{
	INTR_STATE dummy;
	PMM_DATA pmm_data = GetPmmData();
	STATUS status;
	QWORD bytes_read;
	
	LockAcquire(&pmm_data.SwapBitmapLock, &dummy);
	if (BitmapGetBitValue(&pmm_data.SwapBitmap, (DWORD) swapSPTEntry->SwappedFileLocation / PAGE_SIZE)) {
		status = IoReadFile(IomuGetSwapFile(),
			PAGE_SIZE,
			&swapSPTEntry->SwappedFileLocation,
			swapSPTEntry->SwappedAddress,
			&bytes_read);
		LockRelease(&pmm_data.SwapBitmapLock, dummy);
		return status;
	}
	LockRelease(&pmm_data.SwapBitmapLock, dummy);

	return STATUS_ELEMENT_NOT_FOUND;
}