/*++

Copyright (c) 1990-98  Microsoft Corporation All Rights Reserved

Module Name:

    sioctl.c

Abstract:

    Purpose of this driver is to demonstrate how the four different types
    of IOCTLs can be used, and how the I/O manager handles the user I/O
    buffers in each case. This sample also helps to understand the usage of
    some of the memory manager functions.

Environment:

    Kernel mode only.

--*/

//
// Include files.
//

#include <ntddk.h> // various NT definitions
#include <string.h>

#include "cheat.h"
#include "sioctl.h"

#define NT_DEVICE_NAME L"\\Device\\SIOCTL"
#define DOS_DEVICE_NAME L"\\DosDevices\\IoctlTest"

#if DBG
#define SIOCTL_KDPRINT(_x_) \
  DbgPrint("SIOCTL.SYS: "); \
  DbgPrint _x_;

#else
#define SIOCTL_KDPRINT(_x_)
#endif

//
// Device driver routine declarations.
//

DRIVER_INITIALIZE DriverEntry;

_Dispatch_type_(IRP_MJ_CREATE)
  _Dispatch_type_(IRP_MJ_CLOSE) DRIVER_DISPATCH SioctlCreateClose;

_Dispatch_type_(IRP_MJ_DEVICE_CONTROL) DRIVER_DISPATCH SioctlDeviceControl;

DRIVER_UNLOAD SioctlUnloadDriver;

VOID PrintIrpInfo(PIRP Irp);
VOID PrintChars(
  _In_reads_(CountChars) PCHAR BufferAddress, _In_ size_t CountChars
);

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, SioctlCreateClose)
#pragma alloc_text(PAGE, SioctlDeviceControl)
#pragma alloc_text(PAGE, SioctlUnloadDriver)
#pragma alloc_text(PAGE, PrintIrpInfo)
#pragma alloc_text(PAGE, PrintChars)
#endif // ALLOC_PRAGMA

NTSTATUS
DriverEntry(
  _In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath
) {
  NTSTATUS ntStatus;
  UNICODE_STRING ntUnicodeString;     // NT Device Name "\Device\SIOCTL"
  UNICODE_STRING ntWin32NameString;   // Win32 Name "\DosDevices\IoctlTest"
  PDEVICE_OBJECT deviceObject = NULL; // ptr to device object

  UNREFERENCED_PARAMETER(RegistryPath);

  RtlInitUnicodeString(&ntUnicodeString, NT_DEVICE_NAME);

  ntStatus = IoCreateDevice(
    DriverObject,            // Our Driver Object
    0,                       // We don't use a device extension
    &ntUnicodeString,        // Device name "\Device\SIOCTL"
    FILE_DEVICE_UNKNOWN,     // Device type
    FILE_DEVICE_SECURE_OPEN, // Device characteristics
    FALSE,                   // Not an exclusive device
    &deviceObject
  ); // Returned ptr to Device Object

  if (!NT_SUCCESS(ntStatus)) {
    SIOCTL_KDPRINT(("Couldn't create the device object\n"));
    return ntStatus;
  }
  //
  // Initialize the driver object with this driver's entry points.
  //

  DriverObject->MajorFunction[IRP_MJ_CREATE] = SioctlCreateClose;
  DriverObject->MajorFunction[IRP_MJ_CLOSE] = SioctlCreateClose;
  DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = SioctlDeviceControl;
  DriverObject->DriverUnload = SioctlUnloadDriver;

  //
  // Initialize a Unicode String containing the Win32 name
  // for our device.
  //

  RtlInitUnicodeString(&ntWin32NameString, DOS_DEVICE_NAME);

  //
  // Create a symbolic link between our device name  and the Win32 name
  //

  ntStatus = IoCreateSymbolicLink(&ntWin32NameString, &ntUnicodeString);

  if (!NT_SUCCESS(ntStatus)) {
    //
    // Delete everything that this routine has allocated.
    //
    SIOCTL_KDPRINT(("Couldn't create symbolic link\n"));
    IoDeleteDevice(deviceObject);
  }

  cheat_set_callbacks();
  return ntStatus;
}

NTSTATUS
SioctlCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);

  PAGED_CODE();

  Irp->IoStatus.Status = STATUS_SUCCESS;
  Irp->IoStatus.Information = 0;

  IoCompleteRequest(Irp, IO_NO_INCREMENT);

  return STATUS_SUCCESS;
}

VOID SioctlUnloadDriver(_In_ PDRIVER_OBJECT DriverObject) {
  PDEVICE_OBJECT deviceObject = DriverObject->DeviceObject;
  UNICODE_STRING uniWin32NameString;

  PAGED_CODE();

  cheat_remove_callbacks();

  //
  // Create counted string version of our Win32 device name.
  //

  RtlInitUnicodeString(&uniWin32NameString, DOS_DEVICE_NAME);

  //
  // Delete the link from our device name to a name in the Win32 namespace.
  //

  IoDeleteSymbolicLink(&uniWin32NameString);

  if (deviceObject != NULL) {
    IoDeleteDevice(deviceObject);
  }
}

NTSTATUS
SioctlDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  UNREFERENCED_PARAMETER(DeviceObject);

  PAGED_CODE();

  return cheat_device_control(DeviceObject, Irp);
}
