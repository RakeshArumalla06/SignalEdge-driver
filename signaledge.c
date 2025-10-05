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
static char text[64];//  starting the buffer size with 64 bytes

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

// to have control over size of the buffer 
static size_t clamp_len(size_t len)
{
    if (len > (size_t)buffer_size) len=buffer_size;
    if (len > PAGE_SIZE) len=PAGE_SIZE;
    return len;
}
// write handler --> user space to kernel memory
static ssize_t my_write(struct file *fp, const char __user *buf,size_t len, loff_t *off)
{
     size_t to_copy = clamp_len(len);

    if (!my_data)
        return -ENODEV;

    if (copy_from_user(my_data, buf, to_copy))
        return -EFAULT;

    write_count++;
    pr_info("signaledge: wrote %zu bytes (buffer_size=%d)\n", to_copy, buffer_size);
    return to_copy;
}

// read handler --> kernel memory to user space 
static ssize_t my_read(struct file *fp, char __user *buf,size_t len, loff_t *off)
{
     size_t to_copy = clamp_len(len);

    if (!my_data)
        return -ENODEV;

    if (copy_to_user(buf, my_data, to_copy))
        return -EFAULT;

    read_count++;
    pr_info("signaledge: read %zu bytes (buffer_size=%d)\n", to_copy, buffer_size);
    return to_copy;
}

// mmap-handler 
static int my_mmap(struct file *fp, struct vm_area_struct *vma)
{
    unsigned long size=vma->vm_end -vma->vm_start;
    unsigned long pfn;
    int ret;
    if(!my_data) return -ENODEV;

    if(size > PAGE_SIZE)
    {
        pr_err("signaledge: mmap size %lu > PAGE_SIZE\n", size);
        return -EINVAL;
    }

    vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_DONTDUMP); //setting up the flags
    pfn = (unsigned long)virt_to_phys(my_data) >> PAGE_SHIFT; //page frame number 
    ret = remap_pfn_range(vma, vma->vm_start, pfn, size, vma->vm_page_prot);
    if (ret) {
        pr_err("signaledge: remap_pfn_range failed: %d\n", ret);
        return -EAGAIN;
    }

    pr_info("signaledge: mmap OK (size %lu, pfn 0x%lx, buffer_size=%d)\n",
            size, pfn, buffer_size);
    return 0;

}

//open function 
static int my_open(struct inode *inode, struct file *fp) // declartion file opening function
{
    //printk(KERN_INFO "File opened with major no : %d and minor no : %d/n",imajor(inode),iminor(inode));
    pr_info("SignalEdge: Device Opened\n");
    return 0;
}

//close function 
static int my_close(struct inode *inode, struct file *fp) // declartion file opening function
{
    pr_info("SignalEdge: Device Closed\n");
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
        if (buffer_size <= 0 || buffer_size > PAGE_SIZE)
            return -EINVAL;
        pr_info("signaledge: buffer_size set to %d\n", buffer_size);
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
static int __init signaledge_init(void)
{
  
    pr_info("signaledge: module loading\n");
    my_data = (void *)__get_free_page(GFP_KERNEL);
    if (!my_data) {
        pr_err("signaledge: failed to allocate page\n");
        return -ENOMEM;
    }
    memset(my_data, 0, PAGE_SIZE);
    major = register_chrdev(0, "signaledge", &fops);
    if (major < 0) {
        pr_err("signaledge: register_chrdev failed: %d\n", major);
        free_page((unsigned long)my_data);
        return major;
    }
    pr_info("signaledge: registered major %d (initial buffer_size=%d)\n",
            major, buffer_size);
    return 0;
    
}

//destructor
static void __exit signaledge_exit(void)
{
    pr_info("signaledge: module unloading\n");
    if (major >= 0)
        unregister_chrdev(major, "signaledge");

    if (my_data)
        free_page((unsigned long)my_data);
   
}



module_init(signaledge_init);
module_exit(signaledge_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Custom driver implementing a UART-SPI bridge");
