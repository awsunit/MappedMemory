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
    if ((errno = createSharedRegion(name, 512, RS_PERSIST)) != ERR_OK) {
        error("failed to create region");
    }
    if ((errno = map(name, (void**)&addr)) != ERR_OK) {
        error("failed to map to region");
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
        error("failed to unmap from region");
    }
    if ((errno = createSharedRegion(name, 512, RS_PERSIST)) == ERR_OK) {
        error("region has been removed");
    }
    if ((errno = destroySharedRegion(name)) != ERR_OK) {
        error("region cannot be destroyed");
    }
    pass("persist-region-test");
    exit(0);
}