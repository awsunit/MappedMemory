/*
    Tests against creating the same region with the same name twice
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
    if ((errno = createSharedRegion(name, 512, RS_DEFAULT)) != ERR_OK) {
        error("failed to create region");
    }
    if ((errno = createSharedRegion(name, 512, RS_DEFAULT)) == ERR_OK) {
        error("created same shared region twice");
    }
    // try mapping then doing this
    if ((errno = map(name, (void**)&addr)) != ERR_OK) {
        error("failed to map to region");
    }
    if ((errno = createSharedRegion(name, 512, RS_DEFAULT)) == ERR_OK) {
        error("after mapping, created same shared region twice");
    }
    if ((errno = unmap(name, addr)) != ERR_OK) {
        error("failed to unmap from region");
    }
    destroySharedRegion(name);

    pass("dubreg-test");
    exit(0);
}
