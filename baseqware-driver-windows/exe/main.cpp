#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <print>

#include <Windows.h>

#include "../sys/sioctl.h"

using namespace std::chrono_literals;

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
      std::println("CreateFile failed : {}", errNum);
      return INVALID_HANDLE_VALUE;
    }

    //
    // The driver is not started yet so let us the install the driver.
    // First setup full path to driver name.
    //

    if (!SetupDriverName(gDriverLocation, sizeof(gDriverLocation))) {
      return INVALID_HANDLE_VALUE;
    }

    if (!ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_INSTALL)) {
      std::println("Unable to install driver.");
      ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_REMOVE);
      return INVALID_HANDLE_VALUE;
    }

    hDevice = CreateFile(
      "\\\\.\\IoctlTest",
      GENERIC_READ | GENERIC_WRITE,
      FILE_SHARE_READ | FILE_SHARE_WRITE,
      NULL,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
      std::println("Error: CreateFile Failed : {}", GetLastError());
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
  if (!status) {
    std::println("DeviceIoControl failed");
  }
}

int main() {
  gDevice = get_driver(); // NOLINT
  if (gDevice == INVALID_HANDLE_VALUE) {
    return 1;
  }
  std::println("Self PID : {:x}", GetCurrentProcessId());
  ioctl_set_receiver((HANDLE)GetCurrentProcessId());

  std::println("Driver location : {}", gDriverLocation);
  auto _ = std::getchar();

  ioctl_set_receiver(0);
  CloseHandle(gDevice);
  ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_REMOVE);
  return 0;
}
