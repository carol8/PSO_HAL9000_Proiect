#pragma once

#include "list.h"
#include "data_type.h"
#include "syscall_defs.h"

typedef enum _HANDLE_TYPE 
{
	PROCESS_HANDLE,
	THREAD_HANDLE,
	FILE_HANDLE
} HANDLE_TYPE;

typedef struct _Handle_Table_Entry
{
	// A list entry for connecting the handle entry with the handle list
	LIST_ENTRY HandleListElem;

	// The handle itself
	PVOID Handle;

	// The type of the handle
	HANDLE_TYPE Type;

	// A byte that signals if the handle is still valid or not
	BYTE Reserved;
} Handle_Table_Entry, *PHandle_Table_Entry;

UM_HANDLE
HandleListInsertNextList(
	PVOID Handle,
	HANDLE_TYPE HandleType);

PVOID
HandleListGetHandleByIndex(
	UM_HANDLE Handle,
	HANDLE_TYPE HandleType);

STATUS
HandleListRemoveHandle(
	UM_HANDLE Handle,
	HANDLE_TYPE HandleType);