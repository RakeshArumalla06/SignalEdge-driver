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

#define DEVICE_NAME "vuart_sensor"
#define HIST_SIZE   16
#define SENSOR_INTERVAL 1000 

#define LED_BLUE   17
#define LED_GREEN  27
#define LED_YELLOW 22
#define LED_RED    23

#define I2C_BUS_NUM 1
#define LCD_I2C_ADDR 0x27

#define VUART_IOC_MAGIC       'v'
#define VUART_SET_FAN_SPEED    _IOW(VUART_IOC_MAGIC, 1, int)
#define VUART_SET_THRESHOLD    _IOW(VUART_IOC_MAGIC, 2, int)
#define VUART_GET_STATS        _IOR(VUART_IOC_MAGIC, 3, struct vuart_stats)

struct vuart_stats {
    int temp;
    int fan_speed;
    int threshold;
    int alert_flag;
};

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


static void lcd_display_text(const char *text)
{
    if (!lcd_i2c || !text) return;
    i2c_master_send(lcd_i2c, text, strnlen(text, 63));
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
    int noise = (prandom_u32() % 3) - 1;

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

    mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(SENSOR_INTERVAL));
}

static ssize_t vuart_read(struct file *fp, char __user *buf, size_t len, loff_t *off)
{
    ssize_t ret;

    mutex_lock(&vuart_lock);
    if (len > sizeof(*state)) len = sizeof(*state);

    if (copy_to_user(buf, state, len)) {
        ret = -EFAULT;
    } else {
        ret = len;
    }
    mutex_unlock(&vuart_lock);

    return ret;
}

static long vuart_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    struct vuart_stats stats;
    int tmp, ret = 0;

    mutex_lock(&vuart_lock);

    switch(cmd) {
        case VUART_SET_FAN_SPEED:
            if (copy_from_user(&tmp, (int __user *)arg, sizeof(int)))
                ret = -EFAULT;
            else
                state->fan_speed = tmp;
            break;
        case VUART_SET_THRESHOLD:
            if (copy_from_user(&tmp, (int __user *)arg, sizeof(int)))
                ret = -EFAULT;
            else
                state->threshold = tmp;
            break;
        case VUART_GET_STATS:
            stats.temp = state->temp;
            stats.fan_speed = state->fan_speed;
            stats.threshold = state->threshold;
            stats.alert_flag = state->alert_flag;
            if (copy_to_user((struct vuart_stats __user *)arg, &stats, sizeof(stats)))
                ret = -EFAULT;
            break;
        default:
            ret = -EINVAL;
    }

    mutex_unlock(&vuart_lock);
    return ret;
}

static int vuart_open(struct inode *inode, struct file *fp) { 
    pr_info("File is opened\n");
    return 0; }
static int vuart_release(struct inode *inode, struct file *fp) { 
    pr_info("File is closed\n");
    return 0; }

static int vuart_mmap(struct file *fp, struct vm_area_struct *vma)
{
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long pfn;
    int ret = 0;

    if (size > PAGE_SIZE) return -EINVAL;
    pfn = virt_to_phys(state) >> PAGE_SHIFT;
    ret = remap_pfn_range(vma, vma->vm_start, pfn, PAGE_SIZE, vma->vm_page_prot);

    return ret;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .read = vuart_read,
    .open = vuart_open,
    .release = vuart_release,
    .unlocked_ioctl = vuart_ioctl,
    .mmap = vuart_mmap,
};

static int setup_i2c_lcd(void)
{
    struct i2c_adapter *adapter;
    struct i2c_board_info info;

    adapter = i2c_get_adapter(I2C_BUS_NUM);
    if (!adapter) return -ENODEV;

    memset(&info, 0, sizeof(info));
    strlcpy(info.type, "i2c_lcd", I2C_NAME_SIZE);
    info.addr = LCD_I2C_ADDR;

    lcd_i2c = i2c_new_client_device(adapter, &info);
    i2c_put_adapter(adapter);

    if (!lcd_i2c) return -ENODEV;
    return 0;
}

static int __init vuart_init(void)
{
    int ret;

    state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state) return -ENOMEM;

    state->temp = 25;
    state->fan_speed = 0;
    state->threshold = 30;

    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major <0)
    {
      printk(KERN_ERR "char driver is failed\n");
      return major;
    }
    else
    {
        printk(KERN_INFO "char driver module no: %d\n",major);
    }
    return 0;

    gpio_request(LED_BLUE, "led_blue");   gpio_direction_output(LED_BLUE, 1);
    gpio_request(LED_GREEN, "led_green"); gpio_direction_output(LED_GREEN, 0);
    gpio_request(LED_YELLOW, "led_yellow"); gpio_direction_output(LED_YELLOW, 0);
    gpio_request(LED_RED, "led_red");     gpio_direction_output(LED_RED, 0);

    if (setup_i2c_lcd())
        pr_warn("VUART: I2C LCD not detected\n");

    timer_setup(&sensor_timer, sensor_update, 0);
    mod_timer(&sensor_timer, jiffies + msecs_to_jiffies(SENSOR_INTERVAL));

    pr_info("VUART Sensor Module Loaded: major=%d\n", major);
    return 0;
}

static void __exit vuart_exit(void)
{
    del_timer_sync(&sensor_timer);
    unregister_chrdev(major, DEVICE_NAME);

    gpio_set_value(LED_BLUE, 0);
    gpio_set_value(LED_GREEN, 0);
    gpio_set_value(LED_YELLOW, 0);
    gpio_set_value(LED_RED, 0);

    gpio_free(LED_BLUE);
    gpio_free(LED_GREEN);
    gpio_free(LED_YELLOW);
    gpio_free(LED_RED);

    if (lcd_i2c) i2c_unregister_device(lcd_i2c);

    kfree(state);
    pr_info("VUART Sensor Module Unloaded\n");
}

module_init(vuart_init);
module_exit(vuart_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Virtual Temp & Fan Driver with LEDs + I2C LCD + mmap(shared) + IOCTL + Mutex");
