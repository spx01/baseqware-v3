#include <ntifs.h>

#include <ntddk.h>

#include <stdbool.h>

#include "cheat.h"

static PCWSTR kGameExePath = L"\\bin\\win64\\cs2.exe";
static PEPROCESS gGameProcess = NULL;
// This is used for safety purposes, to avoid calling  PsGetProcessId with a
// NULL process.
static HANDLE gGamePID = 0;
static PEPROCESS gCheatProcess = NULL;

typedef struct {
  PCWSTR Path;
  SIZE_T Size;
  PVOID Address;
} GAME_MODULE_INFO;

static GAME_MODULE_INFO gGameModules[PASED_MODULE_COUNT_] = {
  {L"\\bin\\win64\\client.dll", 0, NULL},
  {L"\\bin\\win64\\engine2.dll", 0, NULL},
};

static bool gFoundAllModules = false;

#if DBG
#define SIOCTL_KDPRINT(_x_) \
  DbgPrint("[PASED] ");     \
  DbgPrint _x_;

#else
#define SIOCTL_KDPRINT(_x_)
#endif

bool wstr_ends_with(const wchar_t *str, const wchar_t *suffix) {
  PWCHAR p = wcsstr(str, suffix);
  if (p == NULL) {
    return false;
  }
  return p[wcslen(suffix)] == L'\0';
}

static void LoadImageNotifyRoutine(
  PUNICODE_STRING FullImageName, HANDLE ProcessId, PIMAGE_INFO ImageInfo
) {
  if (gFoundAllModules) {
    return;
  }

  if (wstr_ends_with(FullImageName->Buffer, kGameExePath)) {
    // Found the game process.
    NTSTATUS Status = PsLookupProcessByProcessId(ProcessId, &gGameProcess);
    gGamePID = ProcessId;
    if (!NT_SUCCESS(Status)) {
      SIOCTL_KDPRINT(("Failed to lookup process by id\n"));
    }
    SIOCTL_KDPRINT(("Game process id: %lu\n", ProcessId));
  }

  // Now try getting the modules.
  if (gGameProcess == NULL || ProcessId != gGamePID) {
    return;
  }

  int found = 0;
  for (int i = 0; i < PASED_MODULE_COUNT_; i++) {
    // Already found this module.
    if (gGameModules[i].Address != NULL) {
      ++found;
      continue;
    }

    if (wstr_ends_with(FullImageName->Buffer, gGameModules[i].Path)) {
      ++found;
      gGameModules[i].Size = ImageInfo->ImageSize;
      gGameModules[i].Address = ImageInfo->ImageBase;
      SIOCTL_KDPRINT(
        ("Module %d: %ls, Address: %p, Size: %lu\n",
         i,
         gGameModules[i].Path,
         gGameModules[i].Address,
         gGameModules[i].Size)
      );
    }
  }
  if (found == PASED_MODULE_COUNT_) {
    gFoundAllModules = true;
  }
}

static void
CreateProcessNotifyRoutine(HANDLE ParentId, HANDLE ProcessId, BOOLEAN Create) {
  UNREFERENCED_PARAMETER(ParentId);
  if (gGameProcess == NULL) {
    return;
  }

  if (Create || ProcessId != gGamePID) {
    return;
  }

  SIOCTL_KDPRINT(("Game process exited\n"));
  gGameProcess = NULL;
  gGamePID = 0;
  for (int i = 0; i < PASED_MODULE_COUNT_; i++) {
    gGameModules[i].Address = NULL;
    gGameModules[i].Size = 0;
  }
  gFoundAllModules = false;
}

void cheat_set_callbacks() {
  SIOCTL_KDPRINT(("Setting cheat callbacks\n"));
  PsSetLoadImageNotifyRoutine(LoadImageNotifyRoutine);
  PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, FALSE);
}

void cheat_remove_callbacks() {
  SIOCTL_KDPRINT(("Removing cheat callbacks\n"));
  PsRemoveLoadImageNotifyRoutine(LoadImageNotifyRoutine);
  PsSetCreateProcessNotifyRoutine(CreateProcessNotifyRoutine, TRUE);
}

void cheat_unload_cleanup() {
  // Drop the references we are holding.
  if (gGameProcess != NULL) {
    ObDereferenceObject(gGameProcess);
  }
  if (gCheatProcess != NULL) {
    ObDereferenceObject(gCheatProcess);
  }
}

static NTSTATUS copy_memory(PVOID GameSource, PVOID CheatTarget, SIZE_T Size) {
  extern NTKERNELAPI NTSTATUS MmCopyVirtualMemory(
    PEPROCESS SourceProcess,
    PVOID SourceAddress,
    PEPROCESS TargetProcess,
    PVOID TargetAddress,
    SIZE_T BufferSize,
    KPROCESSOR_MODE PreviousMode,
    PSIZE_T ReturnSize
  );

  SIZE_T Result;
  NTSTATUS status = MmCopyVirtualMemory(
    gGameProcess,
    GameSource,
    gCheatProcess,
    CheatTarget,
    Size,
    KernelMode,
    &Result
  );
  if (NT_SUCCESS(status)) {
    return Result == Size ? STATUS_SUCCESS : STATUS_PARTIAL_COPY;
  }
  return status;
}

NTSTATUS cheat_device_control(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
  PIO_STACK_LOCATION irpSp;
  NTSTATUS ntStatus = STATUS_SUCCESS;
  ULONG inBufLength;
  ULONG outBufLength;

  UNREFERENCED_PARAMETER(DeviceObject);

  PAGED_CODE();

  irpSp = IoGetCurrentIrpStackLocation(Irp);
  inBufLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
  outBufLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

  if (!inBufLength || !outBufLength) {
    ntStatus = STATUS_INVALID_PARAMETER;
    goto End;
  }

  switch (irpSp->Parameters.DeviceIoControl.IoControlCode) {
  case IOCTL_SIOCTL_READ: {
    // TODO: remove
    SIOCTL_KDPRINT(("Read request\n"));

    // Shouldn't happen.
    if (gCheatProcess == NULL) {
      ntStatus = STATUS_NOT_FOUND;
      goto End;
    }

    PASED_READ_REQUEST *request =
      (PASED_READ_REQUEST *)Irp->AssociatedIrp.SystemBuffer;
    ntStatus =
      copy_memory(request->GameAddress, request->DestAddress, request->Size);

    if (!NT_SUCCESS(ntStatus)) {
      goto End;
    }

    // Doesn't matter what we put here, we don't use it.
    Irp->IoStatus.Information = sizeof(*request);
    break;
  }
  case IOCTL_SIOCTL_MODULE: {
    SIOCTL_KDPRINT(("Module request\n"));

    PASED_MODULE_REQUEST *request =
      (PASED_MODULE_REQUEST *)Irp->AssociatedIrp.SystemBuffer;
    if (request->RequestedModule >= PASED_MODULE_COUNT_) {
      ntStatus = STATUS_INVALID_PARAMETER;
      goto End;
    }

    if (gGameModules[request->RequestedModule].Address == NULL) {
      ntStatus = STATUS_NOT_FOUND;
      goto End;
    }

    request->OutAddress = gGameModules[request->RequestedModule].Address;
    request->OutSize = gGameModules[request->RequestedModule].Size;

    // Doesn't matter what we put here, we don't use it.
    Irp->IoStatus.Information = sizeof(*request);
    break;
  }
  case IOCTL_SIOCTL_PID: {
    SIOCTL_KDPRINT(("PID request\n"));
    if (gGameProcess == NULL) {
      ntStatus = STATUS_NOT_FOUND;
      goto End;
    }

    PASED_PID_REQUEST *request =
      (PASED_PID_REQUEST *)Irp->AssociatedIrp.SystemBuffer;
    request->OutPid = PsGetProcessId(gGameProcess);

    // Doesn't matter what we put here, we don't use it.
    Irp->IoStatus.Information = sizeof(*request);
    break;
  }
  case IOCTL_SIOCTL_RECEIVER: {
    SIOCTL_KDPRINT(("Receiver request\n"));
    PASED_RECEIVER_REQUEST *request =
      (PASED_RECEIVER_REQUEST *)Irp->AssociatedIrp.SystemBuffer;
    SIOCTL_KDPRINT(("Receiver PID: %lx\n", request->ReceiverPID));

    if (request->ReceiverPID != 0) {
      PEPROCESS process;
      ntStatus = PsLookupProcessByProcessId(request->ReceiverPID, &process);
      if (!NT_SUCCESS(ntStatus)) {
        ntStatus = STATUS_NOT_FOUND;
        goto End;
      }
      gCheatProcess = process;
    } else {
      gCheatProcess = NULL;
    }

    Irp->IoStatus.Information = sizeof(*request);
    break;
  }
  default:
    ntStatus = STATUS_INVALID_DEVICE_REQUEST;
    SIOCTL_KDPRINT(
      ("ERROR: unrecognized IOCTL %x\n",
       irpSp->Parameters.DeviceIoControl.IoControlCode)
    );
    break;
  }

End:
  Irp->IoStatus.Status = ntStatus;
  IoCompleteRequest(Irp, IO_NO_INCREMENT);
  return ntStatus;
}
