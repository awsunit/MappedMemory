/*
    Test for writing across page boundry
*/
#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>
int PG_SIZE = 4096;

void printAddr(vaddr_t vaddr, vaddr_t end) {
  printf("printing from %p to %p\n", (void*)vaddr, (void*)end);
  while (vaddr != end) {
    printf("%d", *((char*)vaddr));
    vaddr += sizeof(char*);
  }
  printf("%d\n", 420);
}

vaddr_t sm_write (void *vaddr, char *buf, size_t count) {
  for (int i = 0; i < count; i++) {
    *((char*)vaddr) = *buf;
    buf++;
    vaddr += sizeof(char*);
  }
  return (vaddr_t)vaddr;
}

int main()
{

  int size = 2;
  int count = PG_SIZE/sizeof(char*);
  char name[4] = "reg\0";
  char buf[count]; 
  void* addr;


  createSharedRegion(name, size, 0);
  map(name, &addr);


  // populate buf:
  char *tb = &buf[0];
  for (int i = 0; i < count;i++) {
    *tb = (char)i;
    tb++;
  }
  tb = &buf[0];

  /*
  for (int i = 0; i < count; i++) {
    printf("%d", *tb);
    tb++;
  }
  printf("%d\n", 420);
  */

  // write buf twice to region
  int times = 2;
  vaddr_t nv;
  for (int i = 0; i < times; i++) {
    nv = sm_write(addr, buf, count);
    addr = (void*)nv;
  }
  
  // printAddr((vaddr_t)addr, nv);
  addr -= sizeof(char*); // went one too far
  // unmap(name, addr);
  destroySharedRegion(name);

  pass("w-x-boundry-test");
  exit(0);
}
