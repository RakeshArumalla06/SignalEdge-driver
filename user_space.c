#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include "signaledge_ioctl.h"   // ioctl header

int main(void)
{
    int fd;
    struct signaledge_stats stats;
    int choice, val;

    fd = open("/dev/signaledge", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    printf("Device /dev/signaledge opened successfully!\n");

    while (1) {
        printf("\n--- SignalEdge Virtual Sensor Control ---\n");
        printf("1. Set UART Baud Rate\n");
        printf("2. Set I2C Mode\n");
        printf("3. Set Buffer Size\n");
        printf("4. Set Temperature Threshold\n");
        printf("5. Set Fan Speed\n");
        printf("6. Get Device Stats\n");
        printf("7. Exit\n");
        printf("Enter your choice: ");

        if (scanf("%d", &choice) != 1) {
            while (getchar() != '\n'); // clear invalid input
            continue;
        }

        switch (choice) {
            case 1:
                printf("Enter new UART Baud Rate: ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_UART_BAUD, &val) < 0)
                    perror("ioctl - set baud rate");
                else
                    printf("UART Baud Rate updated to %d\n", val);
                break;

            case 2:
                printf("Enter new I2C Mode (0 = Standard, 1 = Fast, 2 = High-speed): ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_I2C_MODE, &val) < 0)
                    perror("ioctl - set I2C mode");
                else
                    printf("I2C Mode updated to %d\n", val);
                break;

            case 3:
                printf("Enter new Buffer Size (max 4096): ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_BUFSIZE, &val) < 0)
                    perror("ioctl - set buffer size");
                else
                    printf("Buffer Size updated to %d bytes\n", val);
                break;

            case 4:
                printf("Enter new Temperature Threshold (°C): ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_THRESHOLD, &val) < 0)
                    perror("ioctl - set threshold");
                else
                    printf("Temperature threshold updated to %d°C\n", val);
                break;

            case 5:
                printf("Enter new Fan Speed (RPM): ");
                scanf("%d", &val);
                if (ioctl(fd, SIGNALEDGE_SET_FAN_SPEED, &val) < 0)
                    perror("ioctl - set fan speed");
                else
                    printf("Fan speed updated to %d RPM\n", val);
                break;

            case 6:
                if (ioctl(fd, SIGNALEDGE_GET_STATS, &stats) < 0)
                    perror("ioctl - get stats");
                else {
                    printf("Current Device Stats:\n");
                    printf("  Temperature     : %d°C\n", stats.temp);
                    printf("  Fan Speed       : %d RPM\n", stats.fan_speed);
                    printf("  Threshold Temp  : %d°C\n", stats.threshold);
                    printf("  Alert Flag      : %s\n", stats.alert_flag ? "OVERHEAT" : "OK");
                    printf("  Buffer Size     : %d bytes\n", stats.buf_size);
                    printf("  UART Baud Rate  : %d\n", stats.baud_rate);
                    printf("  I2C Mode        : %d\n", stats.i2c_mode);
                }
                break;

            case 7:
                close(fd);
                printf("Exiting...\n");
                return 0;

            default:
                printf("Invalid choice! Try again.\n");
        }
    }

    return 0;
}
