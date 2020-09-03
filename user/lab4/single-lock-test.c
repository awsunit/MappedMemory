#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>
int main()
{
    char* name = "channel 001";
    int errno = 0;
    char* addr = NULL;
    if ((errno = createSharedRegion(name, 255, RS_DEFAULT)) != ERR_OK) {
        error("Failed to create region");
    }
    if ((errno = map(name, (void**)&addr)) != ERR_OK) {
        error("Failed to map region");
    }
    if ((errno = lockSharedRegion(name)) != ERR_OK) {
        error("Failed to lock region");
    }
    *addr = 0xa;
    if ((errno = unlockSharedRegion(name)) != ERR_OK) {
        error("Failed to unlock region");
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
        error("Failed to unmap region %d", errno);
    }
    pass("single-lock-test");
    exit(0);
}