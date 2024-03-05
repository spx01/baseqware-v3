// Linux driver interface implementation

#include "../src/driver_interface.hpp"

#include "../../baseqware-driver-linux/ko/ioctl.h"
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstring>
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

const char *driverPath = "./baseq_module.ko";

bool driverLoadedBefore = false;
int driverFD = -1;

int get_driver() {
  const int fd = open(driverPath, O_RDONLY);
  if (fd < 0) {
    // Driver not found
    spdlog::error("Failed to open driver file: {}", driverPath);
    close(fd);
    return -1;
  }
  if (finit_module(fd, "", 0) != 0) {
    if (errno != EEXIST) {
      // Driver loading Failed
      spdlog::error("Failed to load driver");
      close(fd);
      return -1;
    }
    // Driver already loaded
    spdlog::info("Driver already loaded");
    driverLoadedBefore = true;
  }
  close(fd);
  return open("/dev/baseq", O_RDONLY);
}

} // namespace

namespace driver_interface {

bool initialize() {
  driverFD = get_driver();
  if (driverFD < 0) {
    return false;
  }
  spdlog::info("Kernel driver initialized");
  return true;
}

void finalize() {
  close(driverFD);
  if (driverLoadedBefore) {
    return;
  }
  if (delete_module("baseq_module", O_NONBLOCK) != 0) {
    // Driver unloading failed
    spdlog::error("Failed to unload driver");
    return;
  }
  spdlog::info("Kernel driver stopped and unloaded");
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
    spdlog::error("Kernel GET_PID: IOCTL failed: {}", strerror(errno));
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
    spdlog::error("Kernel GET_MODULE: IOCTL failed: {}", strerror(errno));
    return {false, {}};
  }
  return {true, {request.out_address, request.out_size}};
}

} // namespace driver_interface
