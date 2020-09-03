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

  char *name = "REGION";
  int errno;
  void * addr;

  createSharedRegion(name, 1, 0);

  // map
  if ((errno = map(name, &addr)) != ERR_OK) {
    error("map failed with err: %d", errno);
  }

  if ((errno = unmap(name, addr)) != ERR_OK) {
    error("unmapping failed with err: %d", errno);
  }

  destroySharedRegion(name);

  pass("map-unmap-test");
  exit(0);
}

/**/
/*EOF*/
