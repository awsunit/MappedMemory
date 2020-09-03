#include <lib/usyscall.h>
#include <lib/stdio.h>
#include <lib/stddef.h>
#include <lib/string.h>

int main(int argc, char** argv) {
    char* name = "hello";
    void* ptr = NULL;
    int errno = 0;
    if (argc == 1) {
        if (createSharedRegion(name, 4099, RS_PERSIST) == ERR_OK) {
            printf("ok\n");
        } else {
            printf("cannot create\n");
        }
        if (map(name, &ptr) == ERR_OK) {
            printf("mapped at %p\n", ptr);
        }
        strcpy(ptr, name);
        printf("%s\n", ptr);
        unmap(name, ptr);
    } else {
        if ((errno = map(name, &ptr)) == ERR_OK) {
            printf("mapped at %p\n", ptr);
        } else {
            printf("error: %d\n", errno);
        }
        printf("%s\n", ptr);
        unmap(name, ptr);
        
        if ((errno = destroySharedRegion(name)) != ERR_OK) {
            printf("destroy failed: %d", errno);
        }
        
    }
    exit(0);
}
