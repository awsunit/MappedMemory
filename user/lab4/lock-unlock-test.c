/*
  This file contains simple tests to ensure proper creation/destruction of a region.
  Ensures correctness for single process. 
*/
/**/
#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>
int main()
{
  
  int pid = 0;
  int errno = 0;
  int sum = 0;
  char* addr = NULL;
  char buffer[128];

  char* name = "Yet another secret communication channel";
  for (int i = 0; i < sizeof(buffer); i++) {
    buffer[i] = 1;
  }
  if ((errno = createSharedRegion(name, 128, RS_DEFAULT)) != ERR_OK) {
    error("Failed to create shared region");
  }

  if ((errno = map(name, (void**)&addr)) != ERR_OK) {
    error("Failed to map to shared region %d", errno);
  }

  if ((errno = lockSharedRegion(name)) != ERR_OK) {
    error("Failed to lock shared region");
  }

  pid = fork();
  if (pid > 0) {
    // Parent already locked the region
    memcpy(addr, buffer, sizeof(buffer));
    if ((errno = unlockSharedRegion(name)) != ERR_OK) {
      error("Failed to unlock shared region");
    }
    wait(pid, &sum);
    if (sum != 128) {
      error("Child sum is %d instead of 128", sum);
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
      error("Parent failed to unmap shared region");
    }
  } else {
    if ((errno = lockSharedRegion(name)) != ERR_OK) {
      error("Child failed to lock shared region");
    }
    for (int i = 0; i < sizeof(buffer); i++) {
      sum += *(addr + i);
    }
    if ((errno = unlockSharedRegion(name)) != ERR_OK) {
      error("Child failed to unlock shared region");
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
      error("Child failed to unmap shared region");
    }
    exit(sum);
  }
  pass("lock-unlock-test");
  
  exit(0);
}

/**/
/*EOF*/
