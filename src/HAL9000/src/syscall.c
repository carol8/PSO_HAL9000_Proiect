#include "HAL9000.h"
#include "syscall.h"
#include "gdtmu.h"
#include "syscall_defs.h"
#include "syscall_func.h"
#include "syscall_no.h"
#include "mmu.h"
#include "process_internal.h"
#include "dmp_cpu.h"
#include "thread_internal.h"
#include "handle_table.h"
#include "iomu.h"
#include "thread.h"

extern void SyscallEntry();

#define SYSCALL_IF_VERSION_KM       SYSCALL_IMPLEMENTED_IF_VERSION

void
SyscallHandler(
	INOUT   COMPLETE_PROCESSOR_STATE* CompleteProcessorState
)
{
	SYSCALL_ID sysCallId;
	PQWORD pSyscallParameters;
	PQWORD pParameters;
	STATUS status;
	REGISTER_AREA* usermodeProcessorState;

	ASSERT(CompleteProcessorState != NULL);

	// It is NOT ok to setup the FMASK so that interrupts will be enabled when the system call occurs
	// The issue is that we'll have a user-mode stack and we wouldn't want to receive an interrupt on
	// that stack. This is why we only enable interrupts here.
	ASSERT(CpuIntrGetState() == INTR_OFF);
	CpuIntrSetState(INTR_ON);

	LOG_TRACE_USERMODE("The syscall handler has been called!\n");

	status = STATUS_SUCCESS;
	pSyscallParameters = NULL;
	pParameters = NULL;
	usermodeProcessorState = &CompleteProcessorState->RegisterArea;

	__try
	{
		if (LogIsComponentTraced(LogComponentUserMode))
		{
			DumpProcessorState(CompleteProcessorState);
		}

		// Check if indeed the shadow stack is valid (the shadow stack is mandatory)
		pParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp];
		status = MmuIsBufferValid(pParameters, SHADOW_STACK_SIZE, PAGE_RIGHTS_READ, GetCurrentProcess());
		if (!SUCCEEDED(status))
		{
			LOG_FUNC_ERROR("MmuIsBufferValid", status);
			__leave;
		}

		sysCallId = usermodeProcessorState->RegisterValues[RegisterR8];

		LOG_TRACE_USERMODE("System call ID is %u\n", sysCallId);

		// The first parameter is the system call ID, we don't care about it => +1
		pSyscallParameters = (PQWORD)usermodeProcessorState->RegisterValues[RegisterRbp] + 1;

		// Dispatch syscalls
		switch (sysCallId)
		{
		case SyscallIdIdentifyVersion:
			status = SyscallValidateInterface((SYSCALL_IF_VERSION)*pSyscallParameters);
			break;
		case SyscallIdFileCreate:
			status = SyscallFileCreate((char*)pSyscallParameters[0], (QWORD)pSyscallParameters[1], (BOOLEAN)pSyscallParameters[2], (BOOLEAN)pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
			break;
		case SyscallIdFileClose:
			status = SyscallFileClose((UM_HANDLE)pSyscallParameters[0]);
			break;
		case SyscallIdFileRead:
			status = SyscallFileRead((UM_HANDLE)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (QWORD)pSyscallParameters[2], (QWORD*)pSyscallParameters[3]);
			break;
		case SyscallIdFileWrite:
			status = SyscallFileWrite((UM_HANDLE)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (QWORD)pSyscallParameters[2], (QWORD*)pSyscallParameters[3]);
			break;
		case SyscallIdProcessExit:
			status = SyscallProcessExit((STATUS)pSyscallParameters[0]);
			break;
		case SyscallIdProcessCreate:
			status = SyscallProcessCreate((char*)pSyscallParameters[0], (QWORD)pSyscallParameters[1], (char*)pSyscallParameters[2], (QWORD)pSyscallParameters[3], (UM_HANDLE*)pSyscallParameters[4]);
			break;
		case SyscallIdProcessGetPid:
			status = SyscallProcessGetPid((UM_HANDLE)pSyscallParameters[0], (PID*)pSyscallParameters[1]);
			break;
		case SyscallIdProcessWaitForTermination:
			status = SyscallProcessWaitForTermination((UM_HANDLE)pSyscallParameters[0], (STATUS*)pSyscallParameters[1]);
			break;
		case SyscallIdProcessCloseHandle:
			status = SyscallProcessCloseHandle((UM_HANDLE)pSyscallParameters[0]);
			break;
		case SyscallIdThreadExit:
			status = SyscallThreadExit((STATUS)pSyscallParameters[0]);
			break;
    case SyscallIdThreadCreate:
      status = SyscallThreadCreate((PFUNC_ThreadStart)pSyscallParameters[0], (PVOID)pSyscallParameters[1], (UM_HANDLE*)pSyscallParameters[2]);
			break;
    case SyscallIdThreadGetTid:
      status = SyscallThreadGetTid((UM_HANDLE)pSyscallParameters[0], (TID*)pSyscallParameters[1]);
      break;
    case SyscallIdThreadWaitForTermination:
      status = SyscallThreadWaitForTermination((UM_HANDLE)pSyscallParameters[0], (STATUS*)pSyscallParameters[1]);
      break;
		case SyscallIdThreadCloseHandle:
      status = SyscallThreadCloseHandle((UM_HANDLE)pSyscallParameters[0]);
			break;
		default:
			LOG_ERROR("Unimplemented syscall called from User-space!\n");
			status = STATUS_UNSUPPORTED;
			break;
		}

	}
	__finally
	{
		LOG_TRACE_USERMODE("Will set UM RAX to 0x%x\n", status);

		usermodeProcessorState->RegisterValues[RegisterRax] = status;

		CpuIntrSetState(INTR_OFF);
	}
}

void
SyscallPreinitSystem(
	void
)
{

}

STATUS
SyscallInitSystem(
	void
)
{
	return STATUS_SUCCESS;
}

STATUS
SyscallUninitSystem(
	void
)
{

	return STATUS_SUCCESS;
}

void
SyscallCpuInit(
	void
)
{
	IA32_STAR_MSR_DATA starMsr;
	WORD kmCsSelector;
	WORD umCsSelector;

	memzero(&starMsr, sizeof(IA32_STAR_MSR_DATA));

	kmCsSelector = GdtMuGetCS64Supervisor();
	ASSERT(kmCsSelector + 0x8 == GdtMuGetDS64Supervisor());

	umCsSelector = GdtMuGetCS32Usermode();
	/// DS64 is the same as DS32
	ASSERT(umCsSelector + 0x8 == GdtMuGetDS32Usermode());
	ASSERT(umCsSelector + 0x10 == GdtMuGetCS64Usermode());

	// Syscall RIP <- IA32_LSTAR
	__writemsr(IA32_LSTAR, (QWORD)SyscallEntry);

	LOG_TRACE_USERMODE("Successfully set LSTAR to 0x%X\n", (QWORD)SyscallEntry);

	// Syscall RFLAGS <- RFLAGS & ~(IA32_FMASK)
	__writemsr(IA32_FMASK, RFLAGS_INTERRUPT_FLAG_BIT);

	LOG_TRACE_USERMODE("Successfully set FMASK to 0x%X\n", RFLAGS_INTERRUPT_FLAG_BIT);

	// Syscall CS.Sel <- IA32_STAR[47:32] & 0xFFFC
	// Syscall DS.Sel <- (IA32_STAR[47:32] + 0x8) & 0xFFFC
	starMsr.SyscallCsDs = kmCsSelector;

	// Sysret CS.Sel <- (IA32_STAR[63:48] + 0x10) & 0xFFFC
	// Sysret DS.Sel <- (IA32_STAR[63:48] + 0x8) & 0xFFFC
	starMsr.SysretCsDs = umCsSelector;

	__writemsr(IA32_STAR, starMsr.Raw);

	LOG_TRACE_USERMODE("Successfully set STAR to 0x%X\n", starMsr.Raw);
}

// SyscallIdIdentifyVersion
STATUS
SyscallValidateInterface(
	IN  SYSCALL_IF_VERSION          InterfaceVersion
)
{
	LOG_TRACE_USERMODE("Will check interface version 0x%x from UM against 0x%x from KM\n",
		InterfaceVersion, SYSCALL_IF_VERSION_KM);

	if (InterfaceVersion != SYSCALL_IF_VERSION_KM)
	{
		LOG_ERROR("Usermode interface 0x%x incompatible with KM!\n", InterfaceVersion);
		return STATUS_INCOMPATIBLE_INTERFACE;
	}

	return STATUS_SUCCESS;
}

STATUS
SyscallFileCreate(
	IN_READS_Z(PathLength)
	char* Path,
	IN          QWORD                   PathLength,
	IN          BOOLEAN                 Directory,
	IN          BOOLEAN                 Create,
	OUT         UM_HANDLE* FileHandle
)
{
	PFILE_OBJECT PFile;
	STATUS status = STATUS_SUCCESS;
	char FullFilePath[260] = { 0 };
	*FileHandle = UM_INVALID_HANDLE_VALUE;

	status = MmuIsBufferValid((const PVOID)Path, PathLength, PAGE_RIGHTS_READ, GetCurrentProcess());
	if (status != STATUS_SUCCESS) {
		return status;
	}

	if (2 <= PathLength && Path[1] == ':') {
		sprintf(FullFilePath, "%s", Path);
	}
	else {
		sprintf(FullFilePath, "%s%s", IomuGetSystemPartitionPath(), Path);
	}

	status = IoCreateFile(&PFile, FullFilePath, Directory, Create, 0);
	if (status == STATUS_SUCCESS) {
		LOG_TRACE_USERMODE("Inserting file with handle 0x%X\n", PFile);
		*FileHandle = HandleListInsertHandle(PFile, FILE_HANDLE);
	}

	return status;
}

STATUS
SyscallFileClose(
	IN          UM_HANDLE               FileHandle
)
{
	INTR_STATE dummy;

	if (FileHandle == UM_INVALID_HANDLE_VALUE) {
		return STATUS_INVALID_PARAMETER1;
	}
	LOG_TRACE_USERMODE("Handle value: 0x%X\n", FileHandle);
	if (FileHandle == UM_FILE_HANDLE_STDOUT) {
		PPROCESS PProcess = GetCurrentProcess();
		LockAcquire(&PProcess->IsStdoutFileOpenLock, &dummy);
		LOG_TRACE_USERMODE("Is stdout open: %d\n",PProcess->IsStdoutFileOpen);
		if (!PProcess->IsStdoutFileOpen) {
			LockRelease(&PProcess->IsStdoutFileOpenLock, dummy);
			return STATUS_ELEMENT_NOT_FOUND;
		}
		PProcess->IsStdoutFileOpen = FALSE;
		LockRelease(&PProcess->IsStdoutFileOpenLock, dummy);
		return STATUS_SUCCESS;
	}

	PFILE_OBJECT File = (PFILE_OBJECT)HandleListGetHandleByIndex(FileHandle, FILE_HANDLE);
	if (File == NULL) {
		return STATUS_ELEMENT_NOT_FOUND;
	}

	IoCloseFile(File);

	HandleListRemoveHandle(FileHandle, FILE_HANDLE);

	return STATUS_SUCCESS;
}

STATUS
SyscallFileRead(
	IN  UM_HANDLE                   FileHandle,
	OUT_WRITES_BYTES(BytesToRead)
	PVOID                       Buffer,
	IN  QWORD                       BytesToRead,
	OUT QWORD* BytesRead
)
{
	LOG_TRACE_USERMODE("Handle received by SyscallFileRead: %d\n", FileHandle);
	if (FileHandle == UM_INVALID_HANDLE_VALUE || FileHandle == UM_FILE_HANDLE_STDOUT) {
		return STATUS_INVALID_PARAMETER1;
	}

	STATUS status = MmuIsBufferValid(Buffer, BytesToRead, PAGE_RIGHTS_WRITE, GetCurrentProcess());
	if (BytesToRead != 0 && status != STATUS_SUCCESS) {
		*BytesRead = 0;
		return status;
	}

	PFILE_OBJECT File = (PFILE_OBJECT)HandleListGetHandleByIndex(FileHandle, FILE_HANDLE);

	if (File == NULL) {
		*BytesRead = 0;
		return STATUS_ELEMENT_NOT_FOUND;
	}

	QWORD fileOffset = 0;
	IoReadFile(File, BytesToRead, &fileOffset, Buffer, BytesRead);

	return STATUS_SUCCESS;
}

STATUS
SyscallFileWrite(
	IN  UM_HANDLE                   FileHandle,
	IN_READS_BYTES(BytesToWrite)
	PVOID							Buffer,
	IN  QWORD                       BytesToWrite,
	OUT QWORD* BytesWritten
)
{
	INTR_STATE dummy;

	if (FileHandle == UM_INVALID_HANDLE_VALUE) {
		return STATUS_INVALID_PARAMETER1;
	}

	if (FileHandle == UM_FILE_HANDLE_STDOUT) {
		PPROCESS PProcess = GetCurrentProcess();
		PProcess->IsStdoutFileOpenLock;
		LockAcquire(&PProcess->IsStdoutFileOpenLock, &dummy);
		if (!PProcess->IsStdoutFileOpen) {
			LockRelease(&PProcess->IsStdoutFileOpenLock, dummy);
			*BytesWritten = BytesToWrite;
			return STATUS_SUCCESS;
		}
		LockRelease(&PProcess->IsStdoutFileOpenLock, dummy);

		*BytesWritten = BytesToWrite;

		LOG("[%s]:[%s]\n", ProcessGetName(NULL), Buffer);

		return STATUS_SUCCESS;
	}

	STATUS status = MmuIsBufferValid(Buffer, BytesToWrite, PAGE_RIGHTS_READ, GetCurrentProcess());
	if (status != STATUS_SUCCESS) {
		*BytesWritten = 0;
		return status;
	}

	PFILE_OBJECT File = (PFILE_OBJECT)HandleListGetHandleByIndex(FileHandle, FILE_HANDLE);

	if (File == NULL) {
		*BytesWritten = 0;
		return STATUS_ELEMENT_NOT_FOUND;
	}

	QWORD fileOffset = 0;
	IoWriteFile(File, BytesToWrite, &fileOffset, Buffer, BytesWritten);

	return STATUS_SUCCESS;
}

STATUS
SyscallProcessExit(
	IN STATUS ExitStatus
)
{
	ProcessTerminate(GetCurrentProcess());
	return ExitStatus;
}

STATUS
SyscallProcessCreate(
	IN_READS_Z(PathLength)
	char* ProcessPath,
	IN          QWORD               PathLength,
	IN_READS_OPT_Z(ArgLength)
	char* Arguments,
	IN          QWORD               ArgLength,
	OUT         UM_HANDLE* ProcessHandle
)
{
	PPROCESS pProcess;
	STATUS status;
	char FullProcessPath[260] = { 0 };
	*ProcessHandle = UM_INVALID_HANDLE_VALUE;

	UNREFERENCED_PARAMETER(ArgLength);

	status = MmuIsBufferValid((const PVOID)ProcessPath, PathLength, PAGE_RIGHTS_READ, GetCurrentProcess());
	if (status != STATUS_SUCCESS) {
		return status;
	}

	status = MmuIsBufferValid((const PVOID)Arguments, ArgLength, PAGE_RIGHTS_READ, GetCurrentProcess());
	if (ArgLength != 0 && status != STATUS_SUCCESS) {
		return status;
	}

	if (2 <= PathLength && ProcessPath[1] == ':') {
		sprintf(FullProcessPath, "%s", ProcessPath);
	}
	else {
		sprintf(FullProcessPath, "%sApplications\\%s", IomuGetSystemPartitionPath(), ProcessPath);
	}

	status = ProcessCreate(FullProcessPath, Arguments, &pProcess);
	if (status == STATUS_SUCCESS) {
		LOG_TRACE_USERMODE("Inserting process with handle 0x%X\n", pProcess);
		*ProcessHandle = HandleListInsertHandle(pProcess, PROCESS_HANDLE);
	}

	return status;
}

STATUS
SyscallProcessGetPid(
	IN_OPT      UM_HANDLE           ProcessHandle,
	OUT         PID* ProcessId
)
{
	*ProcessId = GetCurrentProcess()->Id;

	if (ProcessHandle != UM_INVALID_HANDLE_VALUE) {
		PPROCESS Process = (PPROCESS)HandleListGetHandleByIndex(ProcessHandle, PROCESS_HANDLE);
		if (Process != NULL) {
			*ProcessId = Process->Id;
		}
	}

	return STATUS_SUCCESS;
}

STATUS
SyscallProcessWaitForTermination(
	IN          UM_HANDLE           ProcessHandle,
	OUT         STATUS* TerminationStatus
)
{
	LOG_TRACE_USERMODE("Waiting for termination of process with handle: %d\n", ProcessHandle);

	if (ProcessHandle == UM_INVALID_HANDLE_VALUE) {
		return STATUS_INVALID_PARAMETER1;
	}

	PPROCESS Process = (PPROCESS)HandleListGetHandleByIndex(ProcessHandle, PROCESS_HANDLE);

	LOG_TRACE_USERMODE("Process found with handle 0x%X\n", Process);

	if (Process == NULL) {
		return STATUS_ELEMENT_NOT_FOUND;
	}

	ProcessWaitForTermination(Process, TerminationStatus);

	return STATUS_SUCCESS;
}


STATUS
SyscallProcessCloseHandle(
	IN          UM_HANDLE           ProcessHandle
)
{
	if (ProcessHandle == UM_INVALID_HANDLE_VALUE) {
		return STATUS_INVALID_PARAMETER1;
	}

	PPROCESS Process = (PPROCESS)HandleListGetHandleByIndex(ProcessHandle, PROCESS_HANDLE);

	if (Process == NULL) {
		return STATUS_ELEMENT_NOT_FOUND;
	}

	ProcessCloseHandle(Process);

	HandleListRemoveHandle(ProcessHandle, PROCESS_HANDLE);

	return STATUS_SUCCESS;
}

STATUS
SyscallThreadExit(
    IN      STATUS                  ExitStatus
)
{
    LOG_TRACE_USERMODE("Syscall Thread Exit");
    ThreadExit(ExitStatus);

    return STATUS_SUCCESS;
}

STATUS SyscallThreadCreate(
    IN PFUNC_ThreadStart StartFunction,
    IN_OPT PVOID Context,
    OUT UM_HANDLE* ThreadHandle
)
{
    PTHREAD pThread = NULL;
    //PVOID userStack = pThread->UserStack;

    // StartFunction = (PFUNC_ThreadStart)userStack;
    // Context = (PVOID)((QWORD)userStack + sizeof(PFUNC_ThreadStart));

    //UNREFERENCED_PARAMETER(StartFunction);
    //UNREFERENCED_PARAMETER(Context);

    //PFUNC_ThreadStart pStartFunction = (PFUNC_ThreadStart)userStack;
    //PVOID pContext = (PVOID)((QWORD)userStack + sizeof(PFUNC_ThreadStart));

    STATUS status;


    PPROCESS cProcess = GetCurrentProcess();

	//if (NULL == StartFunction) {
	//	return STATUS_INVALID_PARAMETER1;
	//}
 //   if (NULL == ThreadHandle) {
 //       return STATUS_INVALID_PARAMETER3;
 //   }
	//if (STATUS_SUCCESS != MmuIsBufferValid((PVOID)ThreadHandle, sizeof(UM_HANDLE), PAGE_RIGHTS_WRITE, cProcess)) {
	//	return STATUS_UNSUCCESSFUL;
	//}
		
    status = ThreadCreateEx("ThreadCreatedBySyscall",
			ThreadPriorityDefault,
			StartFunction,
			Context,
			&pThread,
			cProcess
		);

    if (!SUCCEEDED(status))
    {
        return status;
    }
    
    *ThreadHandle = HandleListInsertHandle(pThread, THREAD_HANDLE);

    return status;
}

STATUS
SyscallThreadGetTid(
    IN_OPT  UM_HANDLE               ThreadHandle,
    OUT     TID*                    ThreadId
)
{
	if (ThreadHandle == UM_INVALID_HANDLE_VALUE) {
		*ThreadId = GetCurrentThread()->Id;
		return STATUS_SUCCESS;
	}
    PVOID Handle = HandleListGetHandleByIndex(ThreadHandle, THREAD_HANDLE);
    if (Handle == 0)
    {
        return STATUS_UNSUCCESSFUL;
    }

    PTHREAD pThread = (PTHREAD)Handle;

    *ThreadId = pThread->Id;

    return STATUS_SUCCESS;
}

STATUS
SyscallThreadWaitForTermination(
	IN      UM_HANDLE               ThreadHandle,
	OUT     STATUS*                 TerminationStatus
)
{
    PVOID Handle = HandleListGetHandleByIndex(ThreadHandle, THREAD_HANDLE);
    if (Handle == 0)
    {
        return STATUS_UNSUCCESSFUL;
    }

    PTHREAD pThread = (PTHREAD)Handle;
    ThreadWaitForTermination(pThread, TerminationStatus);

	return *TerminationStatus;
}

STATUS
SyscallThreadCloseHandle(
	IN      UM_HANDLE               ThreadHandle
)
{
	STATUS status;

	status = HandleListRemoveHandle(ThreadHandle, THREAD_HANDLE);

	return status;
}
