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

	//LOG_TRACE_USERMODE("Inserting handle 0x%X with type %d into the table\n", Handle, HandleType);

	for (it = handleTable->Flink, index = 0;
		it != handleTable;
		it = it->Flink, index++)
	{
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		if (entry->Reserved == 0)
			break;
	}

	//LOG_TRACE_USERMODE("Handle index: %d\n", index);
	if (it != handleTable) {
		//LOG_TRACE_USERMODE("Inserted onto existing allocated space\n", index);
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		entry->Reserved = 1;
		entry->Type = HandleType;
		entry->Handle = Handle;
	}
	else
	{
		//LOG_TRACE_USERMODE("Allocating new space for handle entry...\n", index);
		PHANDLE_TABLE_ENTRY newEntry = ExAllocatePoolWithTag(PoolAllocateZeroMemory, sizeof(HANDLE_TABLE_ENTRY), HEAP_PROCESS_TAG, 0);
		if (newEntry == NULL) {
			LOG_FUNC_ERROR_ALLOC("ExAllocatePoolWithTag", sizeof(HANDLE_TABLE_ENTRY));
			index = UM_INVALID_HANDLE_VALUE;
		}
		else {
			//LOG_TRACE_USERMODE("Allocated new space at address 0x%X\n", newEntry);
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
  
	//LOG_TRACE_USERMODE("Getting handle for index %d from the table\n", Handle);

	for (it = handleTable->Flink, index = 0;
		it != handleTable && index < Handle;
		it = it->Flink, index++);

	if (it != handleTable) {
		//LOG_TRACE_USERMODE("Searching stopped because we reached the correct index!\n");
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		//LOG_TRACE_USERMODE("Reserved byte: expected 1, got %d\n", entry->Reserved);
		//LOG_TRACE_USERMODE("Handle type: expected %d, got %d\n", HandleType, entry->Type);
		if (entry->Reserved == 1 && entry->Type == HandleType) {
			//LOG_TRACE_USERMODE("Handle of correct type found at index %d with address 0x%X\n", index, entry->Handle);
			returnValue = entry->Handle;
		}
	}
	else {
		//LOG_TRACE_USERMODE("Searching stopped because we reached the end of the list!\n");
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	//LOG_TRACE_USERMODE("Found handle address: 0x%X\n", returnValue);

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

	//LOG_TRACE_USERMODE("Removing handle from index %d from the table\n", Handle);

	for (it = handleTable->Flink, index = 0;
		it != handleTable && index < Handle;
		it = it->Flink, index++);

	if (it != handleTable) {
		//LOG_TRACE_USERMODE("Entry found at index %d\n", index);
		PHANDLE_TABLE_ENTRY entry = CONTAINING_RECORD(it, HANDLE_TABLE_ENTRY, HandleListElem);
		if (entry->Type == HandleType) {
			//LOG_TRACE_USERMODE("Entry deleted.\n");
			entry->Reserved = 0;
			status = STATUS_SUCCESS;
		}
	}

	LockRelease(&pProcess->HandleListLock, dummy);

	return status;
}