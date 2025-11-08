#ifndef SIGNALEDGE_IOCTL_H
#define SIGNALEDGE_IOCTL_H

#define SIGNALEDGE_IOC_MAGIC 's'

#define SIGNALEDGE_SET_UART_BAUD  _IOW(SIGNALEDGE_IOC_MAGIC, 1, int)
#define SIGNALEDGE_SET_I2C_MODE   _IOW(SIGNALEDGE_IOC_MAGIC, 2, int)
#define SIGNALEDGE_SET_BUFSIZE    _IOW(SIGNALEDGE_IOC_MAGIC, 3, int)
#define SIGNALEDGE_GET_STATS      _IOR(SIGNALEDGE_IOC_MAGIC, 4, struct signaledge_stats)

struct signaledge_stats {
    int temp;
    int fan_speed;
    int threshold;
    int alert_flag;
    int buf_size;
    int baud_rate;
    int i2c_mode;
};

#endif
