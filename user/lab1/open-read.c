#include <lib/test.h>

int main() {
    // Test open README file and print it to the console
    //  then close the file descriptor
    int fd;
    if ((fd = open("/README", FS_RDONLY, EMPTY_MODE)) < 0) {
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
        if (close(fd) != ERR_OK) {
            error("cannot close file");
        }
    }

    // open file w/ FS_WRONLY, try reading
    // no requirement in spec on what method returns
    // in case of improper acccess
    if ((fd = open("/README", FS_WRONLY, FS_WRONLY)) < 0) {
      error("cant open knivesDeleter.txt");
    } else {
      char buf[500];
      int bytesRead = 0, c = 0;
      // should instafail
      if ((c = read(fd, buf + bytesRead, sizeof(buf) - bytesRead)) != 0) {
	error("should be 0: %d", c);
      }
      if (close(fd) != ERR_OK) {
	error("cannot close file");
      }
        printf("%s", buf);
        pass("open-read");
    }

    exit(0);
    return 0;
}
