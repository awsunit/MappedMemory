#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>

/*
    Test the behavior of lock and unlock
*/
int main()
{
    char* name = "~!@#!@@$@#^(*(^\t\n0xdeadbeef";
    int errno = 0;
    char* addr = NULL;
    if ((errno = lockSharedRegion(name)) == ERR_OK) {
        error("Locking a region that has not been created");
    }
    if ((errno = createSharedRegion(name, 512, RS_DEFAULT)) != ERR_OK) {
        error("Failed to create region");
    }
    if ((errno = lockSharedRegion(name)) != ERR_NOTEXIST) {
        error("Locking a region that has not been mapped");
    }
    if ((errno = map(name, (void**)&addr)) != ERR_OK) {
        error("Failed to create region");
    }
    if ((errno = lockSharedRegion(name)) != ERR_OK) {
        error("Failed to lock a region");
    }
    if ((errno = unlockSharedRegion(name)) != ERR_OK) {
        error("Failed to unlock a region");
    }
    if ((errno = lockSharedRegion(name)) != ERR_OK) {
        error("Failed to lock a region after unlock");
    }
    if ((errno = unlockSharedRegion(name)) != ERR_OK) {
        error("Failed to unlock region");
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
        error("Failed to unmap region");
    }
    if ((errno = lockSharedRegion(name)) == ERR_OK) {
        error("Locking region after unmap");
    }
    if ((errno = unlockSharedRegion(name)) != ERR_NOTEXIST) {
        error("unlocking region after unmap");
    }
    pass("bad-lock-test");
    exit(0);
}
