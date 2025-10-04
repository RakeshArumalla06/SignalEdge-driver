#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>       // headers
#include <linux/ioctl.h>

static int major;
static char text[64];

//runt time parameters
static int baud_rate = 9600;
static int spi_mode  = 0;
static int buffer_size = sizeof(text);

static unsigned long read_count = 0;
static unsigned long write_count = 0;

// IOCTL Commands
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


// read function 
static ssize_t	my_read(struct file *fp, char __user *user_buf, size_t len, loff_t *offset)// reading function declartion
{
    int not_copied, delta ,to_copy =(len +*offset ) < sizeof(text) ? len :(sizeof(text))-*offset;
    if (*offset>=sizeof(text))
    {
        return 0;
    }
    not_copied=copy_to_user(user_buf,&text[*offset],to_copy);
    printk( KERN_INFO "reading is done\n");
    delta=to_copy - not_copied;
    if (not_copied)
    {
        pr_warn("could only copy %d bytes\n",delta);
    }
    *offset+=delta;
    return delta;
    
}

// write function
static ssize_t	my_write(struct file *fp, const char __user *user_buf, size_t len, loff_t *offset)// writing  function declartion
{
    int not_copied, delta ,to_copy =(len +*offset ) < sizeof(text) ? len :(sizeof(text))-*offset;
    if (*offset>=sizeof(text))
    {
        return 0;
    }
    not_copied=copy_from_user(&text[*offset],user_buf,to_copy);
    printk( KERN_INFO "writing is done\n");
    delta=to_copy - not_copied;
    if (not_copied)
    {
        pr_warn("could only copy %d bytes\n",delta);
    }
    *offset+=delta;
    return delta;
   
}

//open function 
static int my_open(struct inode *inode, struct file *fp) // declartion file opening function
{
    //printk(KERN_INFO "File opened with major no : %d and minor no : %d/n",imajor(inode),iminor(inode));
    pr_info("File is opened\n");
    return 0;
}

//close function 
static int my_close(struct inode *inode, struct file *fp) // declartion file opening function
{
    pr_info("File is closed\n");
    return 0;
}

// IOCTL
static long my_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
    struct signaledge_stats stats;

    switch (cmd) {
    case SIGNALEDGE_SET_BAUD:
        if (copy_from_user(&baud_rate, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        pr_info("Baud rate set to %d\n", baud_rate);
        break;

    case SIGNALEDGE_SET_SPI_MODE:
        if (copy_from_user(&spi_mode, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        pr_info("SPI mode set to %d\n", spi_mode);
        break;

    case SIGNALEDGE_SET_BUFSIZE:
        if (copy_from_user(&buffer_size, (int __user *)arg, sizeof(int)))
            return -EFAULT;
        if (buffer_size > sizeof(text))
            buffer_size = sizeof(text);
        pr_info("Buffer size set to %d\n", buffer_size);
        break;

    case SIGNALEDGE_GET_STATS:
        stats.reads = read_count;
        stats.writes = write_count;
        stats.buf_size = buffer_size;
        if (copy_to_user((struct signaledge_stats __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;
        pr_info("Stats returned\n");
        break;

    default:
        return -EINVAL;
    }
    return 0;
}

//declaring the file operations
static struct file_operations fops = { 
    .read=my_read,
    .write=my_write,                
    .open=my_open,
    .release=my_close,
    .unlocked_ioctl = my_ioctl,      
};

//initializer
static int __init hello_init(void)
{
    major=register_chrdev(0,"my_char_driver",&fops);
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
    
}

//destructor
static void __exit hello_exit(void)
{
    printk("module_exit: entry\n");
    unregister_chrdev(major,"my_char_driver");
   
}



module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Custom driver implementing a UART-SPI bridge");
