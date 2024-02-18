#pragma once

#include <wdm.h>
#include <windef.h>

#include "sioctl.h"

void cheat_set_callbacks();
void cheat_remove_callbacks();
void cheat_unload_cleanup();
NTSTATUS cheat_device_control(PDEVICE_OBJECT DeviceObject, PIRP Irp);
