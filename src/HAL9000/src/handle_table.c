#include "HAL9000.h"
#include "handle_table.h"
#include "process.h"
#include "process_internal.h"

UM_HANDLE
HandleListInsertNextList(
	PVOID Handle,
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
		it != handleTable;
		it = it->Flink, index++)
	{
		PHandle_Table_Entry entry = CONTAINING_RECORD(it, Handle_Table_Entry, HandleListElem);
		if (entry->Reserved == 0)
			break;
	}

	if (it != handleTable) {
		PHandle_Table_Entry entry = CONTAINING_RECORD(it, Handle_Table_Entry, HandleListElem);
		entry->Reserved = 1;
		entry->Type = HandleType;
		entry->Handle = Handle;
	}
	else
	{
		Handle_Table_Entry newEntry;
		newEntry.Handle = Handle;
		newEntry.Type = HandleType;
		newEntry.Reserved = 1;

		InsertTailList(handleTable, &newEntry.HandleListElem);
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
		PHandle_Table_Entry entry = CONTAINING_RECORD(it, Handle_Table_Entry, HandleListElem);
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
	PVOID returnValue = 0;	
	STATUS status = STATUS_SUCCESS;

	LockAcquire(&pProcess->HandleListLock, &dummy);

	for (it = handleTable->Flink, index = 0;
		it != handleTable && index < Handle;
		it = it->Flink, index++)
	{
		PHandle_Table_Entry entry = CONTAINING_RECORD(it, Handle_Table_Entry, HandleListElem);
		if (entry->Handle == Handle) {
			entry->Reserved = 0;
			break;
		}
	}

	if (it == handleTable) {
		status = STATUS_ELEMENT_NOT_FOUND;
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	return status;
}