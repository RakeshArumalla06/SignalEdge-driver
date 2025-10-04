#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define SIGNALEDGE_IOC_MAGIC 's'

#define SIGNALEDGE_SET_BAUD     _IOW(SIGNALEDGE_IOC_MAGIC, 1, int)
#define SIGNALEDGE_SET_SPI_MODE _IOW(SIGNALEDGE_IOC_MAGIC, 2, int)
#define SIGNALEDGE_SET_BUFSIZE  _IOW(SIGNALEDGE_IOC_MAGIC, 3, int)
#define SIGNALEDGE_GET_STATS    _IOR(SIGNALEDGE_IOC_MAGIC, 4, struct signaledge_stats)

struct signaledge_stats {
    unsigned long reads;
    unsigned long writes;
    int buf_size;
};

int main() {
    int fd;
    struct signaledge_stats stats;
    int choice;
    int val;

    fd = open("/dev/signaledge_driver", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    printf("Device opened successfully\n");

    while (1) {
        printf("\n--- SignalEdge Controller ---\n");
        printf("1. Set Baud Rate\n");
        printf("2. Set SPI Mode\n");
        printf("3. Set Buffer Size\n");
        printf("4. Get Stats\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // clear invalid input
            continue;
        }

        switch(choice) {
            case 1:
                printf("Enter new Baud Rate: ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_BAUD, &val) < 0)
                    perror("ioctl - set baud");
                else
                    printf("Baud rate set to %d\n", val);
                break;

            case 2:
                printf("Enter new SPI Mode: ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_SPI_MODE, &val) < 0)
                    perror("ioctl - set spi");
                else
                    printf("SPI mode set to %d\n", val);
                break;

            case 3:
                printf("Enter new Buffer Size: ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_BUFSIZE, &val) < 0)
                    perror("ioctl - set bufsize");
                else
                    printf("Buffer size set to %d\n", val);
                break;

            case 4:
                if (ioctl(fd, SIGNALEDGE_GET_STATS, &stats) < 0)
                    perror("ioctl - get stats");
                else
                    printf("Stats: reads=%lu, writes=%lu, buf_size=%d\n",
                           stats.reads, stats.writes, stats.buf_size);
                break;

            case 5:
                close(fd);
                printf("Exiting...\n");
                return 0;

            default:
                printf("Invalid choice!\n");
        }
    }
    return 0;
}
