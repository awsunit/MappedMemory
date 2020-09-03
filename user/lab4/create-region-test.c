/*
  This file contains simple tests to ensure proper creation/destruction of a region.
  Ensures correctness for single process. 
*/
/**/
#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>
#include <lib/test.h>

int main()
{
    
    char* name = "I am sure no one can guess this name :)~~~";
    int errno = 0;
    if ((errno = createSharedRegion(name, 16, 0)) != ERR_OK) {
        error("Failed to create shared region, return is %d", errno);
    }
    if ((errno = createSharedRegion(name, 16, 0)) == ERR_OK) {
        error("Can create shared region with the same name!?");
    }
    if ((errno = destroySharedRegion(name)) != ERR_OK) {
        error("Failed to delete shared region, return is %d", errno);
    }
    if ((errno = destroySharedRegion(name)) == ERR_OK) {
        error("Shared region can be deleted twice!?");
    }
    pass("create-region-test");
    
    exit(0);
}

/**/
/*EOF*/
