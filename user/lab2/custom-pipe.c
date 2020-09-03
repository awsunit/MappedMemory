/*
  This file tests the functionality of pipe()
*/
/**/
#include <lib/test.h>
#include <lib/stddef.h>
#include <lib/string.h>
/**/

int main() {
  int ret, pid;
  //  int pid, ret, i;
  int fds[2];
  char *buf = "ahoy matey";   // size = 12
  char readbuf[512];

  if ((ret = pipe(fds)) != ERR_OK) {
    error("can't open pipe, return val: %d", ret);
  }

  // attempt to write to read end
  if ((ret = write(fds[0], buf, 12)) != 0) {
    error("pipe allowed the read end to be written with %d bytes!", ret);
  }
  
  // attempt read from write end
  if ((ret = read(fds[1], readbuf, 12)) != 0) {
    error("pipe allowed write end of pipe to read %d bytes!", ret);
  }

  // stops writing when buffer full?
  int c = 0;
  int loop = 0;
  while (c != 512) {
    c += write(fds[1], buf, 12);
    loop++;
  }
  if (loop != 43) {  // ceiling of 512/12
    error("invalid write. Only 512 bytes available, wrote %d", c);
  }

  // stops reading when buffer empty?
  c = 0;
  loop = 0;
  while (c != 512) {
    c += read(fds[0], readbuf, 12);
    loop++;
  }
  if (loop != 43) {
    error("invalid read. Only 512 bytes available, read %d", c);
  }
  
  
  // abuse time
  // fork, while loop
  c = 0;
  while (c < 3) {
    if ((pid = fork()) == 0) {

      // parent
      int x = 1200000;
      int count = 0;
	while (count < x) {
	  count += read(fds[0], readbuf, 12);
	}
	// nested fork
	int pid2;
        int x2 = 1200000;
        int count2 = 0;
	if ((pid2 = fork()) == 0) {
	  while (count2 < x2) {
	    count2 += read(fds[0], buf, 12);
	  }
	} else if (pid2 > 0) {
	  while (count2 < x2) {
	    count2 += write(fds[1], buf, 12);
	  }
	} else {
	  error("bad fork");
	}

      exit(0);
    } else if (pid > 0) {

        int x = 1200000;
        int count = 0;
	while (count < x) {
	  count += write(fds[1], buf, 12);
	}

	// exits below
    } else {

      error("bad fork");

    }

    c++;
  }  // end-while
  pass("custom-pipe");
  exit(0);
  return 0;
}
/**/
/*EOF*/
