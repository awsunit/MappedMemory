#include <lib/test.h>
#include <lib/stddef.h>
#define CHILDREN 100
int
main()
{
    int i, pid, ret;

    // spawn 100 children
    for (i = 0; i < CHILDREN; i++) {
        if ((pid = fork()) < 0) {
            error("racetest: fork failed, return value was %d", pid);
        }
        if (pid == 0) {
            if (i == CHILDREN - 1) {
                exit(666);
            }
            exit(i);
            error("racetest: children failed to exit");
        }
    }

    printf("hits=---------------------------------------------\n");
    // wait and reclaim all 100 children
    for (i = 0; i < CHILDREN; i++) {
        if ((ret = wait(-1, NULL)) < 0) {
            error("racetest: failed to collect children, return value was %d", ret);
        }
    }

    pass("race-test");
    exit(0);
    return 0;
}