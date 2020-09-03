/*
    Tests for when a user creates, but doesn't destroy a region

    must be run back-to-back to pick up error
*/

#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>
#define TEST_STRING "password: deadbeefisdelicious"
#define TEST_STRING2 "hacked"

int main()
{
  char *name = "yoshi";
  int err;
  if ((err = createSharedRegion(name, 1, 0)) != ERR_OK) {
    error("regions are not being fully destroyed when process's terminate");
  }

  pass("forgot-destroy-test");
  exit(0);
}
