/*
    Checks that a shared region can be mapped multiple times by a single proc.
*/
#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>

int main()
{
    char* name = "! #T^@Q#Q$Y&AQA#";
    int errno = 0;
    char* addr = NULL;
    char *addr1 = NULL;
    if ((errno = createSharedRegion(name, 512, RS_DEFAULT)) != ERR_OK) {
        error("failed to create region");
    }
    if ((errno = map(name, (void**)&addr)) != ERR_OK) {
        error("failed to map to region");
    }
    if ((errno = map(name, (void**)&addr1)) != ERR_OK) {
        error("failed to map second region");
    }

    if ((errno = unmap(name, addr)) != ERR_OK) {
        error("failed to unmap from region");
    }
    if ((errno = unmap(name, addr1)) != ERR_OK) {
        error("failed to unmap from region");
    }
    if ((errno = destroySharedRegion(name)) != ERR_OK) {
        error("region cannot be removed");
    }
    pass("default-region-test");
    exit(0);
}
