#include <ntifs.h>
#include <ntstrsafe.h>

/*
 * A very simple device driver that loads but does not unload. It writes its
 * load time to C:\nulldrv.txt.
 */

/**
 * The name of this driver.
 */
#define DRIVER_NAME   "nulldrv"

/**
 * The name of this driver.
 */
#define LDRIVER_NAME  L"nulldrv"

DRIVER_INITIALIZE DriverEntry;

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#endif

/**
 * Work item for writing to file.
 */
static PIO_WORKITEM g_work_item = NULL;

/**
 * Global pointer to our singleton device object.
 */
PDEVICE_OBJECT g_dev = NULL;

/* ------------------------------------------------------------------------- */
/**
 * Schedules an IO callback to write the date/time to a file with the given
 * @a filename.
 *
 * See:
 * - http://stackoverflow.com/questions/850530/kernel-mode-driver-write-to-file
 * - http://support.microsoft.com/kb/891805
 * - http://msdn.microsoft.com/en-us/library/windows/hardware/ff566380(v=vs.85).aspx
 *
 * @param filename - description
 */
IO_WORKITEM_ROUTINE append_time_to_file;
VOID append_time_to_file(PDEVICE_OBJECT DeviceObject,
                         PVOID          Context)
{
    UNICODE_STRING    uname;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS          status = STATUS_SUCCESS;
    HANDLE            handle = NULL;
    IO_STATUS_BLOCK   io_status;

    DbgPrint("%s: Writing local time to \"C:\\nulldrv.txt\".\r\n", DRIVER_NAME);

    IoFreeWorkItem(g_work_item);
    g_work_item = NULL;

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        DbgPrint("%s: At IRQL=%u instead of PASSIVE_LEVEL!.\r\n", DRIVER_NAME, (unsigned) KeGetCurrentIrql());
        return;
    }

    RtlInitUnicodeString(&uname, L"\\DosDevices\\C:\\nulldrv.txt");
    InitializeObjectAttributes(&attr, &uname, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    status = ZwCreateFile(&handle,
                          FILE_APPEND_DATA,   // GENERIC_WRITE
                          &attr,
                          &io_status,
                          NULL,
                          FILE_ATTRIBUTE_NORMAL,
                          0,
                          FILE_OPEN_IF,
                          FILE_SYNCHRONOUS_IO_NONALERT,
                          NULL,
                          0);

    if (NT_SUCCESS(status))
    {
        char buf[0x40] = "";
        LARGE_INTEGER now;
        LARGE_INTEGER loc;
        TIME_FIELDS tim;
        KeQuerySystemTime(&now);
        ExSystemTimeToLocalTime(&now, &loc);
        RtlTimeToTimeFields(&loc, &tim);
        RtlStringCchPrintfA(buf, sizeof(buf)-1, "%04u-%02u-%02u %02u:%02u:%02u\r\n",
                            (unsigned) tim.Year, (unsigned) tim.Month,  (unsigned) tim.Day,
                            (unsigned) tim.Hour, (unsigned) tim.Minute, (unsigned) tim.Second);
        buf[sizeof(buf)-1] = 0;
        ZwWriteFile(handle, NULL, NULL, NULL, &io_status, (PVOID) buf, strlen(buf), NULL, NULL);
        ZwClose(handle);
        DbgPrint("%s: Loaded at %s\r\n", DRIVER_NAME, buf);
    }
    else
    {
        DbgPrint("%ws: ZwCreateFile() failed with 0x%08X.\n", (unsigned) status);
    }
}   /* append_time_to_file() */

/* ------------------------------------------------------------------------- */
/**
 * Entry point for driver.
 *
 * @param drv - a fresh driver object to be configured.
 *
 * @param registry_path - registry path in HKLM for this driver and its
 * associated device.
 *
 * @return STATUS_SUCCESS on success, something else on failure.
 */
NTSTATUS DriverEntry(IN PDRIVER_OBJECT drv, IN PUNICODE_STRING registry_path)
{
    UNICODE_STRING device_name_unicode;
    NTSTATUS result = 0;

    DbgPrint(DRIVER_NAME ": Loading driver.\r\n");

    drv->DriverUnload = NULL;

    // Need a device to be able to enqueue a work item below.
    RtlInitUnicodeString(&device_name_unicode, L"\\Device\\" LDRIVER_NAME);

    result = IoCreateDevice(drv,
                            0,     // Device extension.
                            &device_name_unicode,
                            0, //     FILE_DEVICE_DRFIFO,    // Used in ioctl() command values, but how else?
                            0,
                            TRUE, // FALSE,    // Exclusive.
                            &g_dev);

    if (!NT_SUCCESS(result))
    {
        DbgPrint("%s: IoCreateDevice() failed.\r\n", DRIVER_NAME);
        return result;
    }

    // This forces us to be auto-loaded. Nice, eh?
    {
        DWORD start_type = SERVICE_AUTO_START;

        result = RtlWriteRegistryValue(RTL_REGISTRY_SERVICES,
                                       LDRIVER_NAME,
                                       L"Start",
                                       REG_DWORD,
                                       &start_type,
                                       sizeof(start_type));

        if (!NT_SUCCESS(result))
        {
            DbgPrint("%s: Could not set HKLM ...\\Services\\%s\\Start to 0x%04X; no biggie.\r\n",
                     DRIVER_NAME, DRIVER_NAME, start_type);
        }
        else
        {
            DbgPrint("%s: Set HKLM ...\\Services\\%s\\Start to 0x%04X (auto start).\r\n",
                     DRIVER_NAME, DRIVER_NAME, start_type);
        }
    }

    g_work_item = IoAllocateWorkItem(g_dev);

    if (NULL == g_work_item)
    {
        DbgPrint("%s: Could not allocate work item for writing to file.\r\n", DRIVER_NAME);
    }
    else
    {
        IoQueueWorkItem(g_work_item, append_time_to_file, DelayedWorkQueue, NULL);
    }

    return STATUS_SUCCESS;
}   /* DriverEntry() */
