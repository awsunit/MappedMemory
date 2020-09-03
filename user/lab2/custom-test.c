#include <lib/test.h>
#include <lib/stddef.h>
#include <lib/string.h>

int main() {
    int pid, res, num = 0, fd, fd_dup;
    char buf[128];
    assert((fd = open("smallfile", FS_RDONLY, EMPTY_MODE)) > 0);
    assert((fd_dup = dup(fd)) >= 0);
    // Check fork called in children
    if ((pid = fork()) == 0) {
        num = 1;
        pid = fork();
        num = 2;
        if (pid == 0) {
            num = 3;
            exit(num);
        } else {
            wait(pid, &res);
            if (res != 3) {
                error("child return %d instead of 3", res);
            }
            if (wait(-1, NULL) != ERR_CHILD) {
                error("Wait on no child!?");
            }
            if ((num = read(fd_dup, buf, 10)) != 10) {
                error("try to read 10 bytes from dup fd failed, returned %d bytes", num);
            }
            exit(num);
        }
    } else {
        // Parent
        assert(wait(-1, &res) == pid);
        if ((num = read(fd, buf, 10)) != 10) {
            error("try to read 10 bytes from fd failed, returned %d bytes", num);
        }
        if (strcmp("bbbbbbbbbb", buf)) {
            error("should have 10 b's instead of %s", buf);
        }
        assert(close(fd_dup) == ERR_OK);
        assert(close(fd) == ERR_OK);
    }

    pass("custom-test");
    exit(0);
}