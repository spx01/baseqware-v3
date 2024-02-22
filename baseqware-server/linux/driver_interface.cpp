// Linux driver interface implementation

#include "../src/driver_interface.hpp"

#include "../../baseqware-driver-linux/module/ioctl.h"

#include <cstdint>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <utility>

// syscall macros for kernel module loading/unloading
#define finit_module(fd, param_values, flags) \
  syscall(__NR_finit_module, fd, param_values, flags)
#define delete_module(name, flags) syscall(__NR_delete_module, name, flags)

namespace {

const char *driverPath =
  "../../baseqware-driver-linux/build/linux/x86_64/release/baseq_module.ko";

bool driverLoadedBefore = false;
int driverFD = -1;

int getDriver() {
  const int fd = open(driverPath, O_RDONLY);
  if (fd < 0) {
    // Driver not found
    close(fd);
    return -1;
  }
  if (finit_module(fd, "", 0) != 0) {
    if (errno != EEXIST) {
      close(fd);
      return -1;
    }
    // Driver already loaded
    driverLoadedBefore = true;
  }
  close(fd);
  return open("/dev/baseq", O_RDONLY);
}

} // namespace

namespace driver_interface {

// TODO: LOG ERRORS
bool initialize() {
  driverFD = getDriver();
  return driverFD >= 0;
}

void finalize() {
  close(driverFD);
  if (driverLoadedBefore) {
    return;
  }
  if (delete_module("baseq_module", O_NONBLOCK) != 0) {
    // Driver unloading failed
    return;
  }
}

bool read_raw(uint64_t game_address, void *dest, size_t size) {
  baseq_rw_s request{
    .address = game_address,
    .len = size,
    .buffer_address = reinterpret_cast<uintptr_t>(dest),
  };
  if (ioctl(driverFD, baseq_READ, &request) != 0) {
    // Request failed
    return false;
  }
  return true;
}

bool is_game_running() {
  pid_t pid = 0;
  if (ioctl(driverFD, baseq_GET_PID, &pid) != 0) {
    // Request failed
    return false;
  }
  return pid != 0;
}

std::pair<bool, ModuleInfo> get_module_info(ModuleRequest module) {
  baseq_module_req_s request{
    .requested_module = static_cast<baseq_module_type>(module),
    .out_address = 0,
    .out_size = 0
  };
  if (ioctl(driverFD, baseq_GET_MODULE, &request) != 0) {
    // Request failed
    return {false, {}};
  }
  return {true, {request.out_address, request.out_size}};
}

} // namespace driver_interface
