#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/random.h>
#include <linux/mutex.h>

#include "signaledge_ioctl.h" 

#define DEVICE_NAME "signaledge"
#define HIST_SIZE   16
#define SENSOR_INTERVAL_MS 1000

#define LED_BLUE   17
#define LED_GREEN  27
#define LED_YELLOW 22  //base offeset must be added
#define LED_RED    23

#define I2C_BUS_NUM 1
#define LCD_I2C_ADDR 0x27

struct vuart_state {
    int temp;
    int fan_speed;
    int threshold;
    int alert_flag;
    int history[HIST_SIZE];
    int hist_index;
};

static struct vuart_state *state;
static int major;
static struct timer_list sensor_timer;
static struct i2c_client *lcd_i2c;
static DEFINE_MUTEX(vuart_lock);

static int uart_baud = 9600;      
static int i2c_mode = 0;           
static int buffer_size = sizeof(struct vuart_state); 

static void lcd_display_text(const char *text)
{
    size_t len;
    if (!lcd_i2c || !text)
        return;

    len = strnlen(text, 63);
    i2c_master_send(lcd_i2c, text, len);
}

static void update_leds(void)
{
    gpio_set_value(LED_BLUE, 1);
    gpio_set_value(LED_RED, state->alert_flag ? 1 : 0);
    gpio_set_value(LED_YELLOW, (state->temp == state->threshold) ? 1 : 0);
    gpio_set_value(LED_GREEN, (!state->alert_flag && state->temp != state->threshold) ? 1 : 0);
}

static void sensor_update(struct timer_list *t)
{
    int k = 1;
    int noise;

    noise = (int)(prandom_u32() % 3) - 1;

    mutex_lock(&vuart_lock);

    state->temp = state->temp - k * state->fan_speed + noise;
    if (state->temp < 20) state->temp = 20;

    state->alert_flag = (state->temp > state->threshold);
    state->history[state->hist_index] = state->temp;
    state->hist_index = (state->hist_index + 1) % HIST_SIZE;

    update_leds();

    {
        char buf[64];
        snprintf(buf, sizeof(buf), "Temp:%dC Fan:%d %s",
                 state->temp, state->fan_speed,
                 state->alert_flag ? "Alert" : "OK");
        lcd_display_text(buf);
    }

    mutex_unlock(&vuart_lock);

    mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(SENSOR_INTERVAL_MS));
}

static ssize_t signaledge_read(struct file *fp, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret = 0;
    size_t want;

    mutex_lock(&vuart_lock);

    want = min((size_t)buffer_size, sizeof(*state));
    if (len > want) len = want;

    if (copy_to_user(buf, state, len))
        ret = -EFAULT;
    else
        ret = len;

    mutex_unlock(&vuart_lock);
    return ret;
}

static long signaledge_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    struct signaledge_stats stats;
    int tmp;
    int ret = 0;

    mutex_lock(&vuart_lock);

    switch (cmd) {
    case SIGNALEDGE_SET_UART_BAUD:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        uart_baud = tmp;
        pr_info("signaledge: UART baud set to %d (stored only)\n", uart_baud);
        break;

    case SIGNALEDGE_SET_I2C_MODE:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        i2c_mode = tmp;
        pr_info("signaledge: I2C mode set to %d (stored only)\n", i2c_mode);
        break;

    case SIGNALEDGE_SET_BUFSIZE:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        if (tmp <= 0 || tmp > (int)sizeof(struct vuart_state)) {
            pr_warn("signaledge: requested buffer_size %d invalid, keeping %d\n", tmp, buffer_size);
            ret = -EINVAL;
            break;
        }
        buffer_size = tmp;
        pr_info("signaledge: buffer_size set to %d\n", buffer_size);
        break;

    case SIGNALEDGE_SET_THRESHOLD:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        state->threshold = tmp;
        pr_info("signaledge: threshold set to %d\n", state->threshold);
        break;

    case SIGNALEDGE_SET_FAN_SPEED:
        if (copy_from_user(&tmp, (int __user *)arg, sizeof(int))) {
            ret = -EFAULT;
            break;
        }
        if (tmp < 0) tmp = 0;
        if (tmp > 5) tmp = 5;
        state->fan_speed = tmp;
        pr_info("signaledge: fan_speed set to %d\n", state->fan_speed);
        break;

    case SIGNALEDGE_GET_STATS:
        stats.temp = state->temp;
        stats.fan_speed = state->fan_speed;
        stats.threshold = state->threshold;
        stats.alert_flag = state->alert_flag;
        stats.buf_size = buffer_size;
        stats.baud_rate = uart_baud;
        stats.i2c_mode = i2c_mode;

        if (copy_to_user((struct signaledge_stats __user *)arg, &stats, sizeof(stats)))
            ret = -EFAULT;
        break;

    default:
        ret = -EINVAL;
        break;
    }

    mutex_unlock(&vuart_lock);
    return ret;
}

static int signaledge_open(struct inode *inode, struct file *fp)
{
    pr_info("signaledge: device opened\n");
    return 0;
}

static int signaledge_release(struct inode *inode, struct file *fp)
{
    pr_info("signaledge: device closed\n");
    return 0;
}

static int signaledge_mmap(struct file *fp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret;

    if (size > PAGE_SIZE)
        return -EINVAL;

    pfn = virt_to_phys((void *)state) >> PAGE_SHIFT;
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("signaledge: remap_pfn_range failed: %d\n", ret);
        return -EAGAIN;
    }
    pr_info("signaledge: mmap OK (size=%lu)\n", size);
    return 0;
}

static const struct file_operations fops = {
    .owner          = THIS_MODULE,
    .read           = signaledge_read,
    .unlocked_ioctl = signaledge_ioctl,
    .open           = signaledge_open,
    .release        = signaledge_release,
    .mmap           = signaledge_mmap,
};

static int setup_i2c_lcd(void)
{
    struct i2c_adapter *adapter;
    struct i2c_board_info info;

    adapter = i2c_get_adapter(I2C_BUS_NUM);
    if (!adapter)
        return -ENODEV;

    memset(&info, 0, sizeof(info));
    strlcpy(info.type, "i2c_lcd", I2C_NAME_SIZE);
    info.addr = LCD_I2C_ADDR;

    lcd_i2c = i2c_new_client_device(adapter, &info);
    i2c_put_adapter(adapter);

    if (!lcd_i2c)
        return -ENODEV;

    pr_info("signaledge: I2C LCD client created at 0x%02x\n", LCD_I2C_ADDR);
    return 0;
}

static int __init signaledge_init(void)
{
    int ret;

    pr_info("signaledge: loading module\n");

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state)
        return -ENOMEM;

    state->temp = 25;
    state->fan_speed = 0;
    state->threshold = 30;
    state->hist_index = 0;

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        pr_err("signaledge: register_chrdev failed: %d\n", major);
        kfree(state);
        return major;
    }
    pr_info("signaledge: registered major=%d\n", major);

    ret = gpio_request(LED_BLUE, "led_blue");
    if (ret) goto err_unregister;
    gpio_direction_output(LED_BLUE, 1);

    ret = gpio_request(LED_GREEN, "led_green");
    if (ret) goto err_free_blue;
    gpio_direction_output(LED_GREEN, 0);

    ret = gpio_request(LED_YELLOW, "led_yellow");
    if (ret) goto err_free_green;
    gpio_direction_output(LED_YELLOW, 0);

    ret = gpio_request(LED_RED, "led_red");
    if (ret) goto err_free_yellow;
    gpio_direction_output(LED_RED, 0);

    ret = setup_i2c_lcd();
    if (ret)
        pr_warn("signaledge: I2C LCD not present or failed to attach (err=%d)\n", ret);
    timer_setup(&sensor_timer, sensor_update, 0);
    mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(SENSOR_INTERVAL_MS));

    pr_info("signaledge: module loaded (buffer_size=%d, baud=%d, i2c_mode=%d)\n",
            buffer_size, uart_baud, i2c_mode);
    return 0;
err_free_yellow:
    gpio_free(LED_YELLOW);
err_free_green:
    gpio_free(LED_GREEN);
err_free_blue:
    gpio_free(LED_BLUE);
err_unregister:
    unregister_chrdev(major, DEVICE_NAME);
    kfree(state);
    return ret;
}

static void __exit signaledge_exit(void)
{
    pr_info("signaledge: unloading module\n");

    del_timer_sync(&sensor_timer);

    gpio_set_value(LED_BLUE, 0);
    gpio_set_value(LED_GREEN, 0);
    gpio_set_value(LED_YELLOW, 0);
    gpio_set_value(LED_RED, 0);

    gpio_free(LED_BLUE);
    gpio_free(LED_GREEN);
    gpio_free(LED_YELLOW);
    gpio_free(LED_RED);

    if (lcd_i2c)
        i2c_unregister_device(lcd_i2c);

    if (major >= 0)
        unregister_chrdev(major, DEVICE_NAME);

    kfree(state);

    pr_info("signaledge: module unloaded\n");
}

module_init(signaledge_init);
module_exit(signaledge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Virtual sensor driver (UART/I2C hybrid) — supports fan, threshold, stats and IOCTL configuration");
