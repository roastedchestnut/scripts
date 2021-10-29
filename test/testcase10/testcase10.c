#define _GNU_SOURCE

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#define BITMASK(bf_off, bf_len) (((1ull << (bf_len)) - 1) << (bf_off))
#define STORE_BY_BITMASK(type, htobe, addr, val, bf_off, bf_len)               \
  *(type*)(addr) =                                                             \
      htobe((htobe(*(type*)(addr)) & ~BITMASK((bf_off), (bf_len))) |           \
            (((type)(val) << (bf_off)) & BITMASK((bf_off), (bf_len))))

static bool write_file(const char* file, const char* what, ...)
{
  char buf[1024];
  va_list args;
  va_start(args, what);
  vsnprintf(buf, sizeof(buf), what, args);
  va_end(args);
  buf[sizeof(buf) - 1] = 0;
  int len = strlen(buf);
  int fd = open(file, O_WRONLY | O_CLOEXEC);
  if (fd == -1)
    return false;
  if (write(fd, buf, len) != len) {
    int err = errno;
    close(fd);
    errno = err;
    return false;
  }
  close(fd);
  return true;
}

static int inject_fault(int nth)
{
  int fd;
  fd = open("/proc/thread-self/fail-nth", O_RDWR);
  if (fd == -1)
    exit(1);
  char buf[16];
  sprintf(buf, "%d", nth);
  if (write(fd, buf, strlen(buf)) != (ssize_t)strlen(buf))
    exit(1);
  return fd;
}

static void setup_fault()
{
  static struct {
    const char* file;
    const char* val;
    bool fatal;
  } files[] = {
      {"/sys/kernel/debug/failslab/ignore-gfp-wait", "N", true},
      {"/sys/kernel/debug/fail_futex/ignore-private", "N", false},
      {"/sys/kernel/debug/fail_page_alloc/ignore-gfp-highmem", "N", false},
      {"/sys/kernel/debug/fail_page_alloc/ignore-gfp-wait", "N", false},
      {"/sys/kernel/debug/fail_page_alloc/min-order", "0", false},
  };
  unsigned i;
  for (i = 0; i < sizeof(files) / sizeof(files[0]); i++) {
    if (!write_file(files[i].file, files[i].val)) {
      if (files[i].fatal)
        exit(1);
    }
  }
}

uint64_t r[1] = {0xffffffffffffffff};

int main(void)
{
  syscall(__NR_mmap, 0x1ffff000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
  syscall(__NR_mmap, 0x20000000ul, 0x1000000ul, 7ul, 0x32ul, -1, 0ul);
  syscall(__NR_mmap, 0x21000000ul, 0x1000ul, 0ul, 0x32ul, -1, 0ul);
  setup_fault();
  intptr_t res = 0;
  res = syscall(__NR_socket, 0x10ul, 3ul, 0);
  if (res != -1)
    r[0] = res;
  *(uint64_t*)0x20000140 = 0;
  *(uint32_t*)0x20000148 = 0;
  *(uint64_t*)0x20000150 = 0x20000000;
  *(uint64_t*)0x20000000 = 0x20000080;
  *(uint32_t*)0x20000080 = 0x34;
  *(uint16_t*)0x20000084 = 0x10;
  *(uint16_t*)0x20000086 = 0x581;
  *(uint32_t*)0x20000088 = 0;
  *(uint32_t*)0x2000008c = 0;
  *(uint8_t*)0x20000090 = 0;
  *(uint8_t*)0x20000091 = 0;
  *(uint16_t*)0x20000092 = 0;
  *(uint32_t*)0x20000094 = 0;
  *(uint32_t*)0x20000098 = 0x88a8ffff;
  *(uint32_t*)0x2000009c = 0;
  *(uint16_t*)0x200000a0 = 0x14;
  STORE_BY_BITMASK(uint16_t, , 0x200000a2, 0x12, 0, 14);
  STORE_BY_BITMASK(uint16_t, , 0x200000a3, 0, 6, 1);
  STORE_BY_BITMASK(uint16_t, , 0x200000a3, 1, 7, 1);
  *(uint16_t*)0x200000a4 = 0xb;
  *(uint16_t*)0x200000a6 = 1;
  memcpy((void*)0x200000a8, "batadv\000", 7);
  *(uint16_t*)0x200000b0 = 4;
  STORE_BY_BITMASK(uint16_t, , 0x200000b2, 2, 0, 14);
  STORE_BY_BITMASK(uint16_t, , 0x200000b3, 0, 6, 1);
  STORE_BY_BITMASK(uint16_t, , 0x200000b3, 1, 7, 1);
  *(uint64_t*)0x20000008 = 0x34;
  *(uint64_t*)0x20000158 = 1;
  *(uint64_t*)0x20000160 = 0;
  *(uint64_t*)0x20000168 = 0;
  *(uint32_t*)0x20000170 = 0;
  inject_fault(15);
  syscall(__NR_sendmsg, r[0], 0x20000140ul, 0ul);
  return 0;
}
