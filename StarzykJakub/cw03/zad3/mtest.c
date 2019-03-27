#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <unistd.h>


void printMem()
{
    FILE *file = fopen("/proc/self/status", "r");
    char line[128];

    while (fgets(line, 128, file) != NULL)
    {
        if (strncmp(line, "VmSize:", 7) == 0)
        {
            printf("%s", line);
            break;
        }
    }

    fclose(file);
}

int main(int argc, char *argv[])
{
    struct rlimit *rlptr = malloc(sizeof(struct rlimit));

    if (getrlimit(RLIMIT_AS, rlptr) == -1)
    {
        fprintf(stderr, "getrlimit failed for PID=%d\n", getpid());
        exit(EXIT_FAILURE);
    }

    printf("cur: %lu\nmax: %lu\n", rlptr->rlim_cur, rlptr->rlim_max);

    char *pt = NULL;

    for (int i = 0; i < 50; i++)
    {
        printMem();
        pt = malloc(1000 * 1000);
        printf("i=%d: %p\n", i, pt);

        if (pt == NULL)
        {
            printf("errno=%d\n", errno);
            break;
        }
    }

}
