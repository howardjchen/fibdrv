#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"
#define ITERATION 100

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

    FILE *k_data = fopen("k_data.txt", "w");
    FILE *u_data = fopen("u_data.txt", "w");
    FILE *k2u_data = fopen("k2u_data.txt", "w");
    if (!k_data || !u_data || !k2u_data) {
        perror("Failed to open txt");
        exit(2);
    }

    if (mlockall(MCL_CURRENT | MCL_FUTURE))
        printf("mlockall failed!\n");

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    /* Calculating fib value */
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 160);
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
        if (sz < 0)
            break;
    }

    /* Measure performance */
    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);

        fprintf(k_data, "%03d ", i);
        fprintf(u_data, "%03d ", i);
        fprintf(k2u_data, "%03d ", i);
        for (int j = 0; j < ITERATION; j++) {
            /* Calculating fib value */
            clock_gettime(CLOCK_MONOTONIC, &t1);
            sz = read(fd, buf, 160);
            clock_gettime(CLOCK_MONOTONIC, &t2);

            /* Calcaulting time */
            long long user_time = (long long) (t2.tv_sec * 1e9 + t2.tv_nsec) -
                                  (t1.tv_sec * 1e9 + t1.tv_nsec);  // ns
            long long kernel_time = write(fd, write_buf, strlen(write_buf));

            fprintf(k_data, "%lld ", kernel_time);
            fprintf(u_data, "%lld ", user_time);
            fprintf(k2u_data, "%lld ", user_time - kernel_time);
        }
        fprintf(k_data, "\n");
        fprintf(u_data, "\n");
        fprintf(k2u_data, "\n");
        if (sz < 0)
            break;
    }

    close(fd);
    fclose(k_data);
    fclose(u_data);
    fclose(k2u_data);
    return 0;
}
