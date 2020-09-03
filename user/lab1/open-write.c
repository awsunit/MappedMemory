#include <lib/test.h>

int main() {
    // Test open README file and print it to the console
    //  then close the file descriptor
    int fd;
    if ((fd = open("/largefile", FS_RDONLY, EMPTY_MODE)) < 0) {
        error("failed to create a new file");
    } else {
        char buf[500];
        int bytesRead = 0, curBytes = 0;
        while ((curBytes = read(fd, buf + bytesRead, sizeof(buf) - bytesRead)) > 0) {
            bytesRead += curBytes;
            if (bytesRead >= 150) {
                break;
            }
        }
	printf("buf: %s\n", buf);

        if (close(fd) != ERR_OK) {
            error("cannot close file");
        }

	// try writing to file
	int fd1, x = 0;
	if ((fd1 = open("/newsmallfile", FS_RDONLY | FS_WRONLY | FS_CREAT, EMPTY_MODE)) < 0) {
	  error("failed to create new file fd1");
	} else {
	  while ((curBytes = write(fd1, buf + x, sizeof(buf) - x)) > 0) {
	    x += curBytes;
	    if (x == bytesRead) {
	      break;
	    }
	  }
        }
	if (close(fd1) != ERR_OK) {
	  error("cant close newsmallfile");
	}

	// closing just to reset file offset -> method for this?
	char buf1[500];
	x = 0;
	int fd2 = open("/newsmallfile", FS_RDONLY, EMPTY_MODE);
	while ((curBytes = read(fd2, buf1 + x, sizeof(buf) - x)) > 0) {
	  if (x == bytesRead) {
	    break;
	  }
	} 
	
	// try writing -> should be 0
	curBytes = write(fd2, buf1, 1);
	if (curBytes != 0) {
	  error("shouldn't have been able to write to this file");
	}

        printf("next buf: %s\n", buf1);
        pass("open-write");
    }
    exit(0);
    return 0;
}
