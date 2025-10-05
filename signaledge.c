#include <linux/module.h>   
#include <linux/kernel.h>   
#include <linux/init.h> 
#include <linux/fs.h>    
#include <linux/slab.h>    // headers
#include <linux/mm.h>
#include <linux/uaccess.h> 
#include <asm/page.h>
#include <asm/io.h> 

static int major;
static char text[64];

//runt time parameters
static int baud_rate = 9600;
static int spi_mode  = 0;
static int buffer_size = sizeof(text);

// mmap the page into their memory for direct access
static void *my_data; // one page of kernel memory
static int my_mmap(struct file *fp, struct vm_area_struct *vma);

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


// write handeler --> user space to kernel memory
static ssize_t my_write(struct file *fp, const char __user *buf,size_t len, loff_t *off)
{
    if (len > PAGE_SIZE) len = PAGE_SIZE;  // clamp
    if (copy_from_user(my_data, buf, len))
        return -EFAULT;
    printk("my_write: wrote %zu bytes: %s\n", len, (char*)my_data);
    return len;
}

// read handler --> kernel memory to user space 
static ssize_t my_read(struct file *fp, char __user *buf,size_t len, loff_t *off)
{
    if (len > PAGE_SIZE) len = PAGE_SIZE;  // clamp
    if (copy_to_user(buf, my_data, len))
        return -EFAULT;
    printk("my_read: read %zu bytes: %s\n", len, (char*)my_data);
    return len;
}

// mmap-handler 
static int my_mmap(struct file *fp, struct vm_area_struct *vma)
{
    int status;
    vma->vm_pgoff=virt_to_phys(my_data)>>PAGE_SHIFT;// gets the physical address of the kernel buffer 
    // -- > maps that physical page into user space
    status=remap_pfn_range(vma,vma->vm_start,vma->vm_pgoff,vma->vm_end-vma->vm_start,vma->vm_page_prot);
    if(status)
    {
        pr_err("couldnot map memory to user space\n");
        return -EAGAIN;
    }
    return 0;

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
    .owner=THIS_MODULE,
    .mmap=my_mmap,
    .write=my_write,                
    .open=my_open,
    .release=my_close,
    .unlocked_ioctl = my_ioctl,      
};

//initializer
static int __init hello_init(void)
{
    printk("Module is loaded!\n");
    my_data=kzalloc(PAGE_SIZE,GFP_DMA); //allocate one page of memory in DMA safe memory
    if(!my_data)
    {
        pr_err("couldnot allocate memory\n");
        return -ENOMEM;
    }
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
    printk("Module is removed\n");
    unregister_chrdev(major,"my_char_driver");
   
}



module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Custom driver implementing a UART-SPI bridge");
