#include <asm/page.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioctl.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>

#include "ioctl.h"
#include <asm/page_types.h>
#include <linux/mm_types.h>
#include <linux/printk.h>
#include <linux/types.h>

/**
 * Global variables
 */
dev_t dev = 0;
static struct class *dev_class;
static struct cdev baseq_cdev;

struct game_module_info_s {
  char *name;
  size_t size;
  uintptr_t address;
};

static const char *game_name = "cs2";
static struct task_struct *game_task = NULL;
static bool found_all_modules = false;

static struct game_module_info_s game_modules[] = {
  {"libclient.so", 0, 0}, {"libengine2.so", 0, 0}
};

/**
 * Forward declarations
 */
ssize_t
baseq_read(struct file *file, char __user *buf, size_t len, loff_t *offp);
ssize_t baseq_write(
  struct file *file, const char __user *buf, size_t len, loff_t *offp
);
int baseq_open(struct inode *inode, struct file *file);
int baseq_release(struct inode *inode, struct file *file);
long int baseq_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static void lookup_game_task(bool print_err);
static void load_game_modules_info(void);

/**
 * @brief Defines file operations supported by the device
 */
static struct file_operations fops = {
  .owner = THIS_MODULE,
  .read = baseq_read,
  .write = baseq_write,
  .open = baseq_open,
  .release = baseq_release,
  .unlocked_ioctl = baseq_ioctl
};

// use kprobe to detect game exit
static void game_exit_kprobe_handler(
  struct kprobe *p, struct pt_regs *regs, unsigned long flags
) {
  if (game_task == NULL || game_task->pid != current->pid) {
    return;
  }
  pr_info(
    "baseq: game exited: name=%s, pid=%d\n", game_task->comm, game_task->pid
  );
  game_task = NULL;
  found_all_modules = false;
}

static bool game_exit_kprobe_enabled = false;
static struct kprobe game_exit_kprobe = {
  .symbol_name = "do_exit", .post_handler = game_exit_kprobe_handler
};

// use kretprobe to detect game start

static int game_start_kretprobe_handler(
  struct kretprobe_instance *ri, struct pt_regs *regs
) {
  if (game_task != NULL) {
    return 0;
  }
  lookup_game_task(false);
  load_game_modules_info();
  if (game_task != NULL) {
    pr_info(
      "baseq: game started: name=%s, pid=%d\n", game_task->comm, game_task->pid
    );
    return 0;
  }
  return 1;
}

static bool game_start_kretprobe_enabled = false;
static struct kretprobe game_start_kretprobe = {
  .handler = game_start_kretprobe_handler, .kp.symbol_name = "copy_process"
};

/**
 * Finds the process struct by its name
 * @param name Name of the process to find
 * @return Pointer to the process struct, NULL otherwise
 */
static struct task_struct *find_process_by_name(const char *name) {
  struct task_struct *task;

  for_each_process(task) {
    if (!strcmp(task->comm, name)) {
      return task;
    }
  }

  return NULL;
}

/**
 * Reads user-space memory at a certain address
 * @param address Address to read from
 * @param pid Pid of the process to read memory from
 * @param len Length to read (in bytes)
 * @param buffer Buffer that will be filled with read data, should be large
 * enough to contain the data
 * @return Number of bytes read, or error code
 */
ssize_t rw_virtual_memory(uintptr_t address, size_t len, void *buf, int write) {
  if (game_task == NULL) {
    return -ESRCH;
  }

  struct mm_struct *mm;
  struct vm_area_struct *vma;
  void *old_buf;
  int flags;

  mm = get_task_mm(game_task);
  if (!mm) {
    return -ESRCH;
  }

  old_buf = buf;
  flags = write ? FOLL_WRITE : 0;

  // lock for reading
  if (down_read_killable(&mm->mmap_lock)) {
    return -EFAULT;
  }

  while (len) {
    int bytes, ret, offset;
    void *maddr;
    struct page *page = NULL;
    ret = get_user_pages_remote(mm, address, 1, flags, &page, NULL);

    if (ret <= 0) {
      vma = vma_lookup(mm, address);
      if (!vma) {
        break;
      }

      if (vma->vm_ops && vma->vm_ops->access) {
        ret = vma->vm_ops->access(vma, address, buf, len, flags);
      }

      if (ret <= 0) {
        break;
      }

      bytes = ret;
    } else {
      bytes = len;
      offset = address & (PAGE_SIZE - 1);

      if (bytes > PAGE_SIZE - offset) {
        bytes = PAGE_SIZE - offset;
      }

      maddr = kmap(page);

      if (write) {
        copy_to_user_page(vma, page, address, maddr + offset, buf, bytes);
        set_page_dirty_lock(page);
      } else {
        copy_from_user_page(vma, page, address, buf, maddr + offset, bytes);
      }

      kunmap(page);
      put_page(page);
    }

    len -= bytes;
    buf += bytes;
    address += bytes;
  }

  up_read(&mm->mmap_lock);
  mmput(mm);

  return buf - old_buf;
}

static void lookup_game_task(bool print_err) {
  if (game_task) {
    return;
  }
  struct task_struct *task;
  task = find_process_by_name(game_name);
  int status = -ESRCH;
  if (task) {
    game_task = task;
    status = 0;
  }
  if (!print_err && status != 0) {
    return;
  }
  pr_info(
    "baseq: lookup_game_task: status=%d, name=%s, pid=%d\n",
    status,
    status == 0 ? game_task->comm : "error",
    status == 0 ? game_task->pid : 0
  );
}

static int get_game_module(struct game_module_info_s *module) {
  if (game_task == NULL) {
    return -EINVAL;
  }
  struct vm_area_struct *vma;
  struct mm_struct *mm = get_task_mm(game_task);
  int status = 0;
  if (mm == NULL) {
    status = -EINVAL;
    goto get_game_module_out;
  }

  VMA_ITERATOR(vmi, mm, 0);

  for_each_vma(vmi, vma) {
    if (vma->vm_file == NULL) {
      continue;
    }

    // if (strcmp(vma->vm_file->f_path.dentry->d_name.name, module->name) == 0
    // && (vma->vm_flags & VM_EXEC)) {
    if (strcmp(vma->vm_file->f_path.dentry->d_name.name, module->name) == 0) {
      module->address = module->address == 0 ? vma->vm_start : module->address;
      module->size = vma->vm_end - module->address;
    }
  }

get_game_module_out:
  pr_info(
    "baseq: get_game_module: status=%d, name=%s\n",
    status,
    status == 0 ? module->name : "error"
  );
  return status;
}

static void load_game_modules_info(void) {
  if (found_all_modules || game_task == NULL) {
    return;
  }

  int found = 0;

  for (int i = 0; i < BASEQ_MODULE_COUNT; ++i) {
    if (game_modules[i].address != 0 && game_modules[i].size != 0) {
      ++found;
      continue;
    }
    found += get_game_module(&game_modules[i]) == 0;
  }
  if (found == 1) {
    found_all_modules = true;
  }
}

/**
 * Ioctl handler function
 * @param inode File being worked on
 * @param file File pointer
 * @param cmd Ioctl command
 * @param arg Ioctl argument
 * @return 0 on success
 */
long int baseq_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  switch (cmd) {
  case baseq_GET_PID: {
    int status = 0;
    if (game_task == NULL) {
      status = -ENOENT;
      goto get_pid_out;
    }
    if (copy_to_user((void *)arg, &game_task->pid, sizeof(pid_t)) != 0) {
      status = -EFAULT;
      goto get_pid_out;
    }
  get_pid_out:
    pr_info("baseq: get_pid: status=%d\n", status);
    return status;
  }
  case baseq_GET_MODULE: {
    struct baseq_module_req_s req;
    int status = 0;

    if (copy_from_user(&req, (void *)arg, sizeof(struct baseq_module_req_s)) != 0) {
      status = -EFAULT;
      goto get_module_out;
    }

    load_game_modules_info();

    if(game_modules[req.requested_module].address == 0 || game_modules[req.requested_module].size == 0) {
      status = -EINVAL;
      goto get_module_out;
    }

    req.out_address = game_modules[req.requested_module].address;
    req.out_size = game_modules[req.requested_module].size;

    if (copy_to_user((void *)arg, &req, sizeof(struct baseq_module_req_s)) != 0) {
      status = -EFAULT;
      goto get_module_out;
    }

  get_module_out:
    pr_info(
      "baseq: get_module: status=%d, name=%s\n",
      status,
      status == 0 ? game_modules[req.requested_module].name : "error"
    );

    return status;
  }
  case baseq_WRITE: {
    struct baseq_rw_s req;
    void *temp;
    int status = 0;
    ssize_t written_bytes;

    if (copy_from_user(&req, (void *)arg, sizeof(struct baseq_rw_s)) != 0) {
      status = -EFAULT;
      goto write_out;
    }

    pr_info(
      "baseq: write_memory: address=0x%px, len=%lu\n",
      (void *)req.address,
      req.len
    );

    temp = vmalloc(req.len);
    if (!temp) {
      status = -ENOMEM;
      goto write_out;
    }

    if (copy_from_user(temp, (void *)req.buffer_address, req.len) != 0) {
      status = -EFAULT;
      goto write_out;
    }

    written_bytes = rw_virtual_memory(req.address, req.len, temp, 1);
    if (written_bytes < req.len) {
      status = -EAGAIN;
      goto write_out;
    }

  write_out:
    if (temp) {
      vfree(temp);
    }

    pr_info(
      "baseq: write_memory: status=%d, written_bytes=%ld/%lu\n",
      status,
      written_bytes,
      req.len
    );

    return status;
  }
  case baseq_READ: {
    struct baseq_rw_s req;
    void *temp;
    int status = 0;
    ssize_t read_bytes;

    if (copy_from_user(&req, (void *)arg, sizeof(struct baseq_rw_s)) != 0) {
      status = -EFAULT;
      goto read_out;
    }

    pr_info(
      "baseq: read_memory: address=0x%px, len=%lu\n",
      (void *)req.address,
      req.len
    );

    temp = vmalloc(req.len);
    if (!temp) {
      status = -ENOMEM;
      goto read_out;
    }

    read_bytes = rw_virtual_memory(req.address, req.len, temp, 0);
    if (read_bytes < req.len) {
      status = -EAGAIN;
      goto read_out;
    }

    if (copy_to_user((void *)req.buffer_address, temp, req.len) != 0) {
      status = -ENOMEM;
      goto read_out;
    }

    if (copy_to_user((void *)arg, &req, sizeof(struct baseq_rw_s)) != 0) {
      status = -ENOMEM;
      goto read_out;
    }

  read_out:
    if (temp) {
      vfree(temp);
    }

    pr_info(
      "baseq: read_memory: status=%d, read_bytes=%ld/%lu\n",
      status,
      read_bytes,
      req.len
    );

    return status;
  }

  default: {
    pr_info("baseq: unknown ioctl\n");
    break;
  }
  }

  return 0;
}

/**
 * Gets called when an application read the device file in /dev/<device name>
 * @param file File pointer
 * @param buf User-space data buffer pointer
 * @param len Size of the requested data transfer
 * @param offp Indicates the file position the user is accessing
 * @return Number of bytes read, Negative value on error
 */
ssize_t
baseq_read(struct file *file, char __user *buf, size_t len, loff_t *offp) {
  pr_info("baseq: read\n");

  return 0;
}

/**
 * Gets called when an application writes to the device file in /dev/<device
 * name>
 * @param file File pointer
 * @param buf User-space data buffer pointer
 * @param len Size of the requested data transfer
 * @param offp Indicates the file position the user is accessing
 * @return Number of bytes written, Negative value on error
 */
ssize_t baseq_write(
  struct file *file, const char __user *buf, size_t len, loff_t *offp
) {
  pr_info("baseq: write\n");

  return len;
}

/**
 * Gets called when the device file gets opened
 * @param inode File information
 * @param file File pointer
 * @return 0 on success
 */
int baseq_open(struct inode *inode, struct file *file) {
  pr_info("baseq: open\n");

  return 0;
}

/**
 * Gets called when the device file gets released
 * @param inode File information
 * @param file File pointer
 * @return 0 on success
 */
int baseq_release(struct inode *inode, struct file *file) {
  pr_info("baseq: release\n");

  return 0;
}

/**
 * Called by the os on module insertion
 * @return 0 on success, -1 otherwise
 */
static int __init baseq_init(void) {
  // register the device
  if (alloc_chrdev_region(&dev, 0, 1, "baseq") < 0) {
    pr_err("baseq: failed to allocate major number\n");
    return -1;
  }

  cdev_init(&baseq_cdev, &fops);

  if (cdev_add(&baseq_cdev, dev, 1) < 0) {
    pr_err("baseq: failed to add the device to the system\n");
    goto class_fail;
  }

  dev_class = class_create("baseq");
  if (IS_ERR(dev_class)) {
    pr_err("baseq: failed to create struct class for device\n");
    goto class_fail;
  }

  if (IS_ERR(device_create(dev_class, NULL, dev, NULL, "baseq"))) {
    pr_err("baseq: failed to create the device\n");
    goto device_fail;
  }

  pr_info("baseq: loaded: major=%d, minor=%d\n", MAJOR(dev), MINOR(dev));

  lookup_game_task(true);
  load_game_modules_info();

  if (register_kprobe(&game_exit_kprobe) < 0) {
    pr_err("baseq: failed to register game_exit_kprobe\n");
  } else {
    pr_info("baseq: registered game_exit_kprobe\n");
    game_exit_kprobe_enabled = true;
  }
  if (register_kretprobe(&game_start_kretprobe) < 0) {
    pr_err("baseq: failed to register game_start_kretprobe\n");
  } else {
    pr_info("baseq: registered game_start_kretprobe\n");
    game_start_kretprobe_enabled = true;
  }

  return 0;

device_fail:
  device_destroy(dev_class, dev);

class_fail:
  unregister_chrdev_region(dev, 1);

  return -1;
}

/**
 * Called by OS on module unload event
 */
static void __exit baseq_exit(void) {
  device_destroy(dev_class, dev);
  class_destroy(dev_class);
  unregister_chrdev_region(dev, 1);
  if (game_exit_kprobe_enabled)
    unregister_kprobe(&game_exit_kprobe);
  if (game_start_kretprobe_enabled)
    unregister_kretprobe(&game_start_kretprobe);

  pr_info("baseq: unloaded\n");
}

module_init(baseq_init);
module_exit(baseq_exit);

MODULE_LICENSE("GPL");
