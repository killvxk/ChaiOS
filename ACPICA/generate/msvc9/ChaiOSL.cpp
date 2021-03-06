extern "C" {
#include "acpi.h"
#include "acpiosxf.h"
}

#include <memory.h>
#include <spinlock.h>
#include <hal.h>

ACPI_MODULE_NAME("ChaiOSL.cpp")

#define ACPI_MAX_INIT_TABLES 16
static ACPI_TABLE_DESC tables[ACPI_MAX_INIT_TABLES];

//ACPICA Helpers

extern "C" ACPI_STATUS AcpiEarlyTables()
{
	//Early tables - don't even need dynamic memory
	return AcpiInitializeTables(tables, ACPI_MAX_INIT_TABLES, TRUE);
}

extern "C" ACPI_STATUS StartAcpi()
{
	ACPI_STATUS Status;
	//prepare the main subsystem
	Status = AcpiInitializeSubsystem();
	if (ACPI_FAILURE(Status))
		return Status;
	//Copy to dynamic memory
	Status = AcpiReallocateRootTable();
	if (ACPI_FAILURE(Status))
		return Status;
	//Create ACPI namespace
	//Status = AcpiLoadTables();
	if (ACPI_FAILURE(Status))
		return Status;
	//Start the hardware
	Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(Status))
		return Status;
	Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
	if (ACPI_FAILURE(Status))
		return Status;
	return AE_OK;
}

EXTERN CHAIOS_API_FUNC(ACPI_STATUS GetAcpiTable(char* TableName, uint32_t instance, void** table))
{
	return AcpiGetTable(TableName, instance, (ACPI_TABLE_HEADER**)table);
}

//ACPICA to OS interfaces

extern "C" void MpSaveGpioInfo()
{

}

extern "C" void MpSaveSerialInfo()
{

}

extern "C" ACPI_STATUS AcpiOsInitialize()
{
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsTerminate()
{
	return AE_OK;
}

extern "C" ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer()
{
#if (defined(X86) || defined(X64))
	ACPI_PHYSICAL_ADDRESS Ret;
	AcpiFindRootPointer(&Ret);
	return Ret;
#else
#error "ACPICA, ChaiOSL.cpp AcpiOsGetRootPointer() -> Unsupported processor architecture "
#endif
}

extern "C" ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject, ACPI_STRING *NewValue)
{
	*NewValue = (ACPI_STRING)NULL;
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable, ACPI_TABLE_HEADER **NewTable)
{
	*NewTable = (ACPI_TABLE_HEADER*)NULL;
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsPhysicalTableOverride(
ACPI_TABLE_HEADER       *ExistingTable,
ACPI_PHYSICAL_ADDRESS   *NewAddress,
UINT32                  *NewTableLength)
{
	*NewAddress = (ACPI_PHYSICAL_ADDRESS)NULL;
	*NewTableLength = 0;
	return AE_OK;
}

extern "C" void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length)
{
	const CVMemMngr::PAGING_ATTRIBUTES& attribs = getMemoryManager()->getVMemMngr()->getDefaultAttributes(true);
	void* addr = getMemoryManager()->getVMemMngr()->mapPhysicalAddress((physaddr)PhysicalAddress, Length, attribs);
	return addr;
}

extern "C" void AcpiOsUnmapMemory(void *where, ACPI_SIZE length)
{
	getMemoryManager()->getVMemMngr()->unmap(where, length);
}

extern "C" ACPI_STATUS AcpiOsGetPhysicalAddress(void *LogicalAddress, ACPI_PHYSICAL_ADDRESS *PhysicalAddress)
{
	*PhysicalAddress = (ACPI_PHYSICAL_ADDRESS)getMemoryManager()->getVMemMngr()->getPhysicalAddress(LogicalAddress);
	return AE_OK;
}

extern "C" void *AcpiOsAllocate(ACPI_SIZE Size)
{
	return new char[Size];
}

//TODO: Slab allocator. For now, we just use the heap
extern "C" ACPI_STATUS
AcpiOsCreateCache(
char                    *CacheName,
UINT16                  ObjectSize,
UINT16                  MaxDepth,
ACPI_CACHE_T            **ReturnCache)
{
	/* *ReturnCache = (ACPI_CACHE_T*)new char[ObjectSize*MaxDepth];
	if (!*ReturnCache)
		return AE_NO_MEMORY;*/
	*ReturnCache = (ACPI_CACHE_T*)ObjectSize;
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsDeleteCache(
ACPI_CACHE_T            *Cache)
{
	/*
	delete[] (char*)Cache;
	*/
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsPurgeCache(
ACPI_CACHE_T            *Cache)
{
	/*
	delete[](char*)Cache;
	*/
	return AE_OK;
}

extern "C" void *
AcpiOsAcquireObject(
ACPI_CACHE_T            *Cache)
{
	size_t size = (size_t)Cache;
	return (void*)new char[size];
}

extern "C" ACPI_STATUS
AcpiOsReleaseObject(
ACPI_CACHE_T            *Cache,
void                    *Object)
{
	delete [](char*)Object;
	return AE_OK;
}

extern "C" void AcpiOsFree(void *Memory)
{
	delete[] Memory;
}

extern "C" BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length)
{
	//Should check the pages are readable (i.e. present). BUT WE'RE THE KERNEL!
	return TRUE;
}

extern "C" BOOLEAN AcpiOsWritable(void *Memory, ACPI_SIZE Length)
{
	return AcpiOsReadable(Memory, Length);		//If we can read it, we can write it, we're the kernel
}

extern "C" ACPI_THREAD_ID AcpiOsGetThreadId()
{
	return 0;
}

extern "C" ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE Type, ACPI_OSD_EXEC_CALLBACK Function, void *Context)
{
	//Should create a new thread. For now, just execute it.
	UNUSED(Type);
	Function(Context);
	return AE_OK;
}

extern "C" void AcpiOsSleep(UINT64 Milliseconds)
{
	//A crude implementation that just trys a bit of loop waiting
	auto lambda = [](int x) {
		x += 1;
	};
	for (int n = 0; n < Milliseconds * 10000; n++)
		lambda(n);
}

extern "C" void AcpiOsStall(UINT32 Microseconds)
{
	//A crude implementation that just trys a bit of loop waiting
	auto lambda = [](int x) {
		x += 1;
	};
	for (int n = 0; n < Microseconds * 10; n++)
		lambda(n);
}

extern "C" ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits, ACPI_SEMAPHORE *OutHandle)
{
	*OutHandle = new ACPI_SEMAPHORE;
	if (!*OutHandle)
		return AE_NO_MEMORY;
	*OutHandle = (ACPI_SEMAPHORE)InitialUnits;
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle)
{
	delete Handle;
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units, UINT16 Timeout)
{
	//TODO: when we have threads, implement these
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsSignalSemaphore(
ACPI_SEMAPHORE          Handle,
UINT32                  Units)
{
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK *OutHandle)
{
	return AE_OK;
	*OutHandle = new CSpinlock;
	if (!*OutHandle)
		return AE_NO_MEMORY;
	return AE_OK;
}

extern "C" void AcpiOsDeleteLock(void* Handle)
{
	return;
	delete (CSpinlock*)Handle;
}

extern "C" ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle)
{
	return AE_OK;
	CSpinlock* spnlk = (CSpinlock*)Handle;
	spnlk->acquireSpinlock();
	return 1;
}

extern "C" void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS Flags)
{
	return;
	CSpinlock* spnlk = (CSpinlock*)Handle;
	spnlk->releaseSpinlock();
}

static void __fastcall InterruptHelper(uint32_t vect, size_t context, size_t handler)
{
	((ACPI_OSD_HANDLER)handler)((void*)context);
}

extern "C" ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptLevel, ACPI_OSD_HANDLER Handler, void *Context)
{
	size_t pars[2] = { (size_t)Context, (size_t)Handler };
	size_t vect = getHal()->getIrqChip()->getInterruptVect(InterruptLevel);
	if (!getHal()->getInterruptManager()->addVect(vect, &InterruptHelper, pars))
		return AE_ERROR;
	return AE_OK;
}

extern "C" ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber, ACPI_OSD_HANDLER Handler)
{
	size_t vect = getHal()->getIrqChip()->getInterruptVect(InterruptNumber);
	getHal()->getInterruptManager()->releaseVect(vect);
	return AE_OK;
}

extern "C" void
AcpiOsWaitEventsComplete(void)
{
	return;
}

extern "C" ACPI_STATUS
AcpiOsReadMemory(
ACPI_PHYSICAL_ADDRESS   Address,
UINT64                  *Value,
UINT32                  Width)
{
	UINT64* mapped = (UINT64*)AcpiOsMapMemory(Address, 8);
	if (!mapped)
		return AE_ERROR;
	if (Width == 64)
		*Value = *mapped;
	else
		*Value = *mapped & (((UINT64)1 << Width) - 1);
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsWriteMemory(
ACPI_PHYSICAL_ADDRESS   Address,
UINT64                  Value,
UINT32                  Width)
{
	UINT64* mapped = (UINT64*)AcpiOsMapMemory(Address, 8);
	if (!mapped)
		return AE_ERROR;
	if (Width == 64)
		*mapped = Value;
	else if (Width == 32)
		*(UINT32*)mapped = Value;
	else if (Width == 16)
		*(UINT16*)mapped = Value;
	else if (Width == 8)
		*(UINT8*)mapped = Value;
	else
		return AE_BAD_PARAMETER;
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsReadPort(
ACPI_IO_ADDRESS         Address,
UINT32                  *Value,
UINT32                  Width)
{
	switch (Width)
	{
	case 32:
		*Value = INPORTD(Address);
		break;
	case 16:
		*Value = INPORTW(Address);
		break;
	case 8:
		*Value = INPORTB(Address);
		break;
	default:
		return AE_BAD_PARAMETER;
		break;
	}
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsWritePort(
ACPI_IO_ADDRESS         Address,
UINT32                  Value,
UINT32                  Width)
{
	switch (Width)
	{
	case 32:
		OUTPORTD(Address, Value);
		break;
	case 16:
		OUTPORTW(Address, Value);
		break;
	case 8:
		OUTPORTB(Address, Value);
		break;
	default:
		return AE_BAD_PARAMETER;
		break;
	}
	return AE_OK;
}

extern "C" ACPI_STATUS
AcpiOsReadPciConfiguration(
ACPI_PCI_ID             *PciId,
UINT32                  Reg,
UINT64                  *Value,
UINT32                  Width)
{
#if defined X86 || defined X64
	const UINT32 CONFIG_ADDRESS = 0xCF8;
	const UINT32 CONFIG_DATA = 0xCFC;

	uint32_t address = (uint32_t)((PciId->Bus << 16) | (PciId->Device << 11) |
		(PciId->Function << 8) | (Reg & 0xfc) | ((uint32_t)0x80000000));

	OUTPORTD(CONFIG_ADDRESS, address);
	*Value = INPORTD(CONFIG_DATA);

	switch (Width)
	{
	case 64:
		OUTPORTD(CONFIG_ADDRESS, address+4);
		*Value |= (((UINT64)INPORTD(CONFIG_DATA))<<32);
		break;
	case 16:
		*Value &= 0xFFFF;
		break;
	case 8:
		*Value &= 0xFF;
		break;
	default:
		return AE_BAD_PARAMETER;
	}
	return AE_OK;
#endif
}

extern "C" ACPI_STATUS AcpiOsWritePciConfiguration(
	ACPI_PCI_ID             *PciId,
	UINT32                  Reg,
	UINT64                  Value,
	UINT32                  Width)
{
#if defined X86 || defined X64
	const UINT32 CONFIG_ADDRESS = 0xCF8;
	const UINT32 CONFIG_DATA = 0xCFC;

	uint32_t address = (uint32_t)((PciId->Bus << 16) | (PciId->Device << 11) |
		(PciId->Function << 8) | (Reg & 0xfc) | ((uint32_t)0x80000000));

	switch (Width)
	{
	case 64:
		OUTPORTD(CONFIG_ADDRESS, address);
		OUTPORTD(CONFIG_DATA, Value);
		OUTPORTD(CONFIG_ADDRESS, address + 4);
		OUTPORTD(CONFIG_DATA, Value >> 32);
		break;
	case 32:
		OUTPORTD(CONFIG_ADDRESS, address);
		OUTPORTD(CONFIG_DATA, Value);
	case 16:
		OUTPORTD(CONFIG_ADDRESS, address);
		OUTPORTW(CONFIG_DATA, Value);
	case 8:
		OUTPORTD(CONFIG_ADDRESS, address);
		OUTPORTB(CONFIG_DATA, Value);
		break;
	default:
		return AE_BAD_PARAMETER;
	}
	return AE_OK;
#endif
}

extern "C" UINT64
AcpiOsGetTimer(
void)
{
	return READ_TSC();
}

extern "C" ACPI_STATUS AcpiOsSignal(UINT32 Function, void* Info)
{
	return AE_MISSING_ARGUMENTS;
}

//TODO: do stuff beyond here

extern "C" void ACPI_INTERNAL_VAR_XFACE AcpiOsPrintf(
	const char              *Format,
	...)
{
	return;
}

extern "C" void AcpiOsVprintf(
const char              *Format,
va_list                 Args)
{
	return;
}

extern "C" void AcpiOsRedirectOutput(void *Destination)
{
	return;
}

extern "C" ACPI_STATUS
AcpiOsGetLine(
char                    *Buffer,
UINT32                  BufferLength,
UINT32                  *BytesRead)
{
	return AE_OK;
}

extern "C" void *
AcpiOsOpenDirectory(
char                    *Pathname,
char                    *WildcardSpec,
char                    RequestedFileType)
{
	return NULL;
}

extern "C" char *
AcpiOsGetNextFilename(
void                    *DirHandle)
{
	return (char*)NULL;
}

extern "C" void
AcpiOsCloseDirectory(
void                    *DirHandle)
{
	return;
}

extern "C" ACPI_FILE
AcpiOsOpenFile(
const char              *Path,
UINT8                   Modes)
{
	return NULL;
}

extern "C" void
AcpiOsCloseFile(
ACPI_FILE               File)
{
	return;
}

extern "C" int
AcpiOsWriteFile(
ACPI_FILE               File,
void                    *Buffer,
ACPI_SIZE               Size,
ACPI_SIZE               Count)
{
	return -1;
}

extern "C" long
AcpiOsGetFileOffset(
ACPI_FILE               File)
{
	return 0;
}

extern "C" ACPI_STATUS
AcpiOsSetFileOffset(
ACPI_FILE               File,
long                    Offset,
UINT8                   From)
{
	return AE_NOT_FOUND;
}
