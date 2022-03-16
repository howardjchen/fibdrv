#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

int main()
{
    long long sz;
    struct timespec t1, t2;
    char buf[160];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    FILE *data = fopen("data.txt", "w");
    if (!data) {
        perror("Failed to open data text");
        exit(2);
    }

    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        printf("mlockall failed!\n");

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        sz = read(fd, buf, 160);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        long long ut = (long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
                       (t1.tv_sec * 1e9 + t1.tv_nsec);  // ns

        long long time_val = write(fd, write_buf, strlen(write_buf));
        fprintf(data, "%03d %lld %lld %lld\n", i, time_val, ut,
                (ut - time_val));
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
        if (sz < 0)
            break;
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 160);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    fclose(data);
    return 0;
}
