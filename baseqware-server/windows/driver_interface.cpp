/// Windows specific driver interface implementation.

#include "../src/driver_interface.hpp"

#include <Windows.h>

#include <spdlog/spdlog.h>

#include "../../baseqware-driver/sys/sioctl.h"

namespace {

extern "C" {
BOOLEAN
ManageDriver(
  _In_ LPCTSTR DriverName, _In_ LPCTSTR ServiceName, _In_ USHORT Function
);

BOOLEAN
SetupDriverName(
  _Inout_updates_bytes_all_(BufferLength) PCHAR DriverLocation,
  _In_ ULONG BufferLength
);
}

TCHAR gDriverLocation[MAX_PATH];
HANDLE gDevice;

HANDLE get_driver() {
  DWORD errNum = 0;
  HANDLE hDevice;

  hDevice = CreateFile(
    "\\\\.\\IoctlTest",
    GENERIC_READ | GENERIC_WRITE,
    0,
    NULL,
    CREATE_ALWAYS,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );
  if (hDevice == INVALID_HANDLE_VALUE) {
    errNum = GetLastError();

    if (errNum != ERROR_FILE_NOT_FOUND) {
      spdlog::error("CreateFile for driver failed: {}", errNum);
      return INVALID_HANDLE_VALUE;
    }

    //
    // The driver is not started yet so let us the install the driver.
    // First setup full path to driver name.
    //

    if (!bool(SetupDriverName(gDriverLocation, sizeof(gDriverLocation)))) {
      return INVALID_HANDLE_VALUE;
    }

    if (!bool(ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_INSTALL)
        )) {
      spdlog::error("Unable to install driver");
      ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_REMOVE);
      return INVALID_HANDLE_VALUE;
    }

    hDevice = CreateFile(
      "\\\\.\\IoctlTest",
      GENERIC_READ | GENERIC_WRITE,
      0,
      NULL,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
      spdlog::error("CreateFile for driver failed: {}", GetLastError());
      return INVALID_HANDLE_VALUE;
    }
  }
  return hDevice;
}

void ioctl_set_receiver(HANDLE pid) {
  PASED_RECEIVER_REQUEST request;
  request.ReceiverPID = pid;
  DWORD bytes;
  BOOL status = DeviceIoControl(
    gDevice,
    IOCTL_SIOCTL_RECEIVER,
    &request,
    sizeof(request),
    &request,
    sizeof(request),
    &bytes,
    nullptr
  );
  if (!bool(status)) {
    spdlog::error("Kernel set receiver: DeviceIoControl failed");
  }
}
} // namespace

namespace driver_interface {

bool initialize() {
  gDevice = get_driver();
  if (gDevice == INVALID_HANDLE_VALUE) {
    return false;
  }
  ioctl_set_receiver(HANDLE(uintptr_t(GetCurrentProcessId())));
  spdlog::info("Kernel driver initialized");
  return true;
}

void finalize() {
  CloseHandle(gDevice);
  ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_REMOVE);
  spdlog::info("Kernel driver stopped and removed");
}

bool read_raw(uint64_t game_address, void *dest, size_t size) {
  PASED_READ_REQUEST request;
  request.GameAddress = PVOID(game_address);
  request.Size = size;
  request.DestAddress = dest;
  DWORD bytes;
  BOOL status = DeviceIoControl(
    gDevice,
    IOCTL_SIOCTL_READ,
    &request,
    sizeof(request),
    &request,
    sizeof(request),
    &bytes,
    nullptr
  );
  if (!bool(status)) {
    spdlog::error("Kernel read: DeviceIoControl failed");
    return false;
  }
  return true;
}

bool is_game_running() {
  // Get the pid and check against 0.
  PASED_PID_REQUEST request;
  DWORD bytes;
  BOOL status = DeviceIoControl(
    gDevice,
    IOCTL_SIOCTL_PID,
    &request,
    sizeof(request),
    &request,
    sizeof(request),
    &bytes,
    nullptr
  );
  if (!bool(status)) {
    if (GetLastError() != ERROR_NOT_FOUND) {
      spdlog::error("Kernel pid: DeviceIoControl failed", GetLastError());
    }
    return false;
  }
  return true;
}

std::pair<bool, ModuleInfo> get_module_info(ModuleRequest module) {
  PASED_MODULE_REQUEST request;
  *reinterpret_cast<int *>(&request.RequestedModule) = module;
  DWORD bytes;
  BOOL status = DeviceIoControl(
    gDevice,
    IOCTL_SIOCTL_MODULE,
    &request,
    sizeof(request),
    &request,
    sizeof(request),
    &bytes,
    nullptr
  );
  if (!bool(status)) {
    if (GetLastError() != ERROR_NOT_FOUND) {
      spdlog::error("Kernel module: DeviceIoControl failed");
    }
    return {false, {}};
  }
  return {true, {uintptr_t(request.OutAddress), request.OutSize}};
}

} // namespace driver_interface
