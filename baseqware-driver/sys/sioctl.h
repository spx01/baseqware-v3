#pragma once

#if !defined(_WIN64)
#error "32 bit builds are not supported"
#endif

#include <windef.h>

//
// Device type           -- in the "User Defined" range."
//
#define SIOCTL_TYPE 40000
//
// The IOCTL function codes from 0x800 to 0xFFF are for customer use.
//
#define IOCTL_SIOCTL_READ \
  CTL_CODE(SIOCTL_TYPE, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SIOCTL_MODULE \
  CTL_CODE(SIOCTL_TYPE, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SIOCTL_PID \
  CTL_CODE(SIOCTL_TYPE, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SIOCTL_RECEIVER \
  CTL_CODE(SIOCTL_TYPE, 0x903, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct {
  /// Address from the game's space.
  PVOID GameAddress;
  /// Address in the userland cheat's space.
  PVOID DestAddress;
  SIZE_T Size;
} PASED_READ_REQUEST;

typedef enum {
  PASED_CLIENT_MODULE,
  PASED_MODULE_COUNT_,
} PASED_MODULE_TYPE;

typedef struct {
  PASED_MODULE_TYPE RequestedModule;

  PVOID OutAddress;
  SIZE_T OutSize;
} PASED_MODULE_REQUEST;

typedef struct {
  HANDLE OutPid;
} PASED_PID_REQUEST;

typedef struct {
  HANDLE ReceiverPID;
} PASED_RECEIVER_REQUEST;

#define DRIVER_FUNC_INSTALL 0x01
#define DRIVER_FUNC_REMOVE 0x02

#define DRIVER_NAME "SIoctl"
