#ifndef SIGNALEDGE_IOCTL_H
#define SIGNALEDGE_IOCTL_H

#include <linux/ioctl.h>

#define SIGNALEDGE_IOC_MAGIC 's'

// IOCTL Commands
#define SIGNALEDGE_SET_UART_BAUD   _IOW(SIGNALEDGE_IOC_MAGIC, 1, int)
#define SIGNALEDGE_SET_I2C_MODE    _IOW(SIGNALEDGE_IOC_MAGIC, 2, int)
#define SIGNALEDGE_SET_BUFSIZE     _IOW(SIGNALEDGE_IOC_MAGIC, 3, int)
#define SIGNALEDGE_SET_THRESHOLD   _IOW(SIGNALEDGE_IOC_MAGIC, 4, int)
#define SIGNALEDGE_SET_FAN_SPEED   _IOW(SIGNALEDGE_IOC_MAGIC, 5, int)
#define SIGNALEDGE_GET_STATS       _IOR(SIGNALEDGE_IOC_MAGIC, 6, struct signaledge_stats)

// Stats structure
struct signaledge_stats {
    int temp;          // Current sensor temperature
    int fan_speed;     // Current fan speed
    int threshold;     // Temperature threshold
    int alert_flag;    // 1 = Overheat, 0 = Normal
    int buf_size;      // Buffer size used in driver
    int baud_rate;     // UART baud rate
    int i2c_mode;      // I2C operating mode
};

#endif
