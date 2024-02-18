#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <print>
#include <stop_token>
#include <thread>

#include <Windows.h>

#include "../../offsets.hpp"
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
      0,
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

std::mutex gExitMutex;
void sig_handler(int signum) {
  if (signum == SIGINT) {
    std::lock_guard lock(gExitMutex);
    std::exit(0);
  }
}

void *get_client_module() {
  void *client_module = nullptr;
  PASED_MODULE_REQUEST req;
  req.OutAddress = nullptr;
  req.RequestedModule = PASED_CLIENT_MODULE;
  DWORD bytes;
  BOOL status = DeviceIoControl(
    gDevice,
    IOCTL_SIOCTL_MODULE,
    &req,
    sizeof(req),
    &req,
    sizeof(req),
    &bytes,
    nullptr
  );
  if (!status) {
    std::println("DeviceIoControl failed");
  }
  return req.OutAddress;
}

int main() {
  gDevice = get_driver(); // NOLINT
  if (gDevice == INVALID_HANDLE_VALUE) {
    return 1;
  }
  std::println("Self PID : {:x}", GetCurrentProcessId());
  ioctl_set_receiver((HANDLE)GetCurrentProcessId());

  {
    auto _ = std::atexit([]() {
      ioctl_set_receiver(0);
      CloseHandle(gDevice);
      ManageDriver(DRIVER_NAME, gDriverLocation, DRIVER_FUNC_REMOVE);
    });
  }

  { auto _ = std::signal(SIGINT, sig_handler); }

  std::println("Driver location : {}", gDriverLocation);
  std::jthread th([&](std::stop_token st) {
    while (!st.stop_requested()) {
      std::this_thread::sleep_for(1s);
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
      if (!status) {
      } else {
        std::println("PID : {}", request.OutPid);
        void *local_player = nullptr;
        void *client_module = get_client_module();
        if (!client_module)
          continue;
        PASED_READ_REQUEST read_request;
        read_request.DestAddress = &local_player;
        read_request.GameAddress =
          (PVOID)((uintptr_t)client_module + client_dll::dwLocalPlayerPawn);
        read_request.Size = sizeof(local_player);
        DeviceIoControl(
          gDevice,
          IOCTL_SIOCTL_READ,
          &read_request,
          sizeof(read_request),
          &read_request,
          sizeof(read_request),
          &bytes,
          nullptr
        );
        std::println("Local player : {:x}", (uintptr_t)local_player);
      }
    }
  });
  auto _ = std::getchar();
  th.request_stop();
  th.join();
  return 0;
}
