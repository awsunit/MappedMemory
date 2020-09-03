/*
  This file contains simple tests shared-memory writing.
  Ensures correctness for single process and across fork calls. 
*/
/**/
#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>

int main()
{

//int errno, status;
  int status;
  int size = 1;
  char *name = "region\0";
  char *word = "ET PHONE HOME\0";
  //  char buf[20];
  void* addr;


  createSharedRegion(name, size, 0);

  map(name, &addr);

  *((char*)addr) = *name;

  printf("word is %s\n", word);

  if (fork() != 0) {
    printf("parent started with addr being: %s\n",  (char*)addr);
    // parent
    // write then let child read
    wait(-1, &status);
    printf("parent ended with addr being: %s\n",  (char*)addr);

  }else{

    lockSharedRegion(name);
    *((char*)addr) = *word;
    unlockSharedRegion(name);
    printf("child has: %s\n", (char*)addr);
    exit(getpid());

  }

  


  destroySharedRegion(name);

  pass("write-test");
  exit(0);
}

/**/
/*EOF*/
