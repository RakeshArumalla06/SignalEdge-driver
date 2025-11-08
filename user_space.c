#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define VUART_IOC_MAGIC 'v'

#define VUART_SET_FAN_SPEED _IOW(VUART_IOC_MAGIC, 1, int)
#define VUART_SET_THRESHOLD _IOW(VUART_IOC_MAGIC, 2, int)
#define VUART_GET_STATS     _IOR(VUART_IOC_MAGIC, 3, struct vuart_stats)

struct vuart_stats {
    int temp;
    int fan_speed;
    int threshold;
    int alert_flag;
};

int main(void)
{
    int fd;
    struct vuart_stats stats;
    int choice, val;

    fd = open("/dev/vuart_sensor", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    printf("Device /dev/vuart_sensor opened successfully!\n");

    while (1) {
        printf("\n--- Virtual UART Sensor Control ---\n");
        printf("1. Set Fan Speed\n");
        printf("2. Set Temperature Threshold\n");
        printf("3. Get Sensor Stats\n");
        printf("4. Exit\n");
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // clear bad input
            continue;
        }

        switch (choice) {
            case 1:
                printf("Enter new Fan Speed: ");
                scanf("%d", &val);
                if (ioctl(fd, VUART_SET_FAN_SPEED, &val) < 0)
                    perror("ioctl - set fan speed");
                else
                    printf("Fan speed updated to %d\n", val);
                break;

            case 2:
                printf("Enter new Threshold: ");
                scanf("%d", &val);
                if (ioctl(fd, VUART_SET_THRESHOLD, &val) < 0)
                    perror("ioctl - set threshold");
                else
                    printf("Threshold updated to %d°C\n", val);
                break;

            case 3:
                if (ioctl(fd, VUART_GET_STATS, &stats) < 0)
                    perror("ioctl - get stats");
                else
                    printf("Current Sensor Stats:\n"
                           "  Temperature: %d°C\n"
                           "  Fan Speed:   %d\n"
                           "  Threshold:   %d°C\n"
                           "  Alert:       %s\n",
                           stats.temp, stats.fan_speed, stats.threshold,
                           stats.alert_flag ? "OVERHEAT" : "OK");
                break;

            case 4:
                close(fd);
                printf("Exiting...\n");
                return 0;

            default:
                printf("Invalid choice!\n");
        }
    }

    return 0;
}
