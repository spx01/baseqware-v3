#pragma once

#include <linux/types.h>
#include <linux/ioctl.h>

enum baseq_module_type{
  BASEQ_LIBCLIENT_MODULE,
  BASEQ_LIBENGINE2_MODULE,
  BASEQ_MODULE_COUNT,
};

/**
 * @brief Contains the info about a specific module 
 *@param requested_module The module to request (In)
 * @param out_address Start address of the region (Out)
 * @param out_size Size of the region (Out)
 */
struct baseq_module_req_s {
  enum baseq_module_type requested_module;
  uintptr_t out_address;
  size_t out_size;
};

/**
 * @brief Contains info about a memory request (Read or Write)
 * @param pid Pid of the process to scan (In)
 * @param address Address of the block of memory to read (In)
 * @param len Amount of bytes to read from the address (In)
 * @param buffer_address Address of a buffer containing data for Write
 * operation, empty for Read operation
 */
struct baseq_rw_s {
  uintptr_t address;
  size_t len;
  uintptr_t buffer_address;
};

/**
 * Ioctl declarations
 */
#define baseq_GET_MODULE _IOWR(0x22, 1, struct baseq_module_req_s *)
#define baseq_READ _IOWR(0x22, 2, struct baseq_rw_s *)
#define baseq_WRITE _IOWR(0x22, 3, struct baseq_rw_s *)
