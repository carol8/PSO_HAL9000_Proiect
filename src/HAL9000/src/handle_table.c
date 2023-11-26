#include "HAL9000.h"
#include "handle_table.h"
#include "process.h"
#include "process_internal.h"

UM_HANDLE
HandleListInsertHandle(
	PVOID Handle,
	HANDLE_TYPE HandleType)
{
	PPROCESS pProcess = GetCurrentProcess();
	PLIST_ENTRY handleTable = &pProcess->HandleListHead;
	UM_HANDLE index;
	INTR_STATE dummy;
	PLIST_ENTRY it;

	LockAcquire(&pProcess->HandleListLock, &dummy);

	for (it = handleTable->Flink, index = 0;
		it != handleTable;
		it = it->Flink, index++)
	{
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		if (entry->Reserved == 0)
			break;
	}

	if (it != handleTable) {
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		entry->Reserved = 1;
		entry->Type = HandleType;
		entry->Handle = Handle;
	}
	else
	{
		PHANDLE_TABLE_ENTRY newEntry = ExAllocatePoolWithTag(PoolAllocateZeroMemory, sizeof(HANDLE_TABLE_ENTRY), HEAP_PROCESS_TAG, 0);
		if (newEntry == NULL) {
			LOG_FUNC_ERROR_ALLOC("ExAllocatePoolWithTag", sizeof(HANDLE_TABLE_ENTRY));
			index = UM_INVALID_HANDLE_VALUE;
		}
		else {
			newEntry->Handle = Handle;
			newEntry->Type = HandleType;
			newEntry->Reserved = 1;

			InsertTailList(handleTable, &newEntry->HandleListElem);
		}
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	return index;
}

PVOID
HandleListGetHandleByIndex(
	UM_HANDLE Handle,
	HANDLE_TYPE HandleType)
{
	PPROCESS pProcess = GetCurrentProcess();
	PLIST_ENTRY handleTable = &pProcess->HandleListHead;
	UM_HANDLE index;
	INTR_STATE dummy;
	PLIST_ENTRY it;
	PVOID returnValue = 0;

	LockAcquire(&pProcess->HandleListLock, &dummy);

	for (it = handleTable->Flink, index = 0;
		it != handleTable && index < Handle;
		it = it->Flink, index++);
	
	if (it != handleTable) {
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		if (entry->Reserved == 1 && entry->Type == HandleType) {
			returnValue = entry->Handle;
		}
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	return returnValue;
}

STATUS
HandleListRemoveHandle(
	UM_HANDLE Handle,
	HANDLE_TYPE HandleType)
{
	PPROCESS pProcess = GetCurrentProcess();
	PLIST_ENTRY handleTable = &pProcess->HandleListHead;
	UM_HANDLE index;
	INTR_STATE dummy;
	PLIST_ENTRY it;
	STATUS status = STATUS_ELEMENT_NOT_FOUND;

	LockAcquire(&pProcess->HandleListLock, &dummy);

	for (it = handleTable->Flink, index = 0;
		it != handleTable && index < Handle;
		it = it->Flink, index++);

	if (it != handleTable) {
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		if (entry->Type == HandleType) {
			entry->Reserved = 0;
			status = STATUS_SUCCESS;
		}
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	return status;
}