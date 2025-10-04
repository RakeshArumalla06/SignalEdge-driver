#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>

static int major;
static char text[64];
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
static int my_open(struct inode *inode, struct file *fp) // declartion file opening function
{
    //printk(KERN_INFO "File opened with major no : %d and minor no : %d/n",imajor(inode),iminor(inode));
    pr_info("File is opened\n");
    return 0;
}
static int my_close(struct inode *inode, struct file *fp) // declartion file opening function
{
    pr_info("File is closed\n");
    return 0;
}
static struct file_operations fops = { //declaring the file operations
    .read=my_read,
    .write=my_write,                
    .open=my_open,
    .release=my_close        
};// this is the exact synatax

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

static void __exit hello_exit(void)
{
    printk("module_exit: entry\n");
    unregister_chrdev(major,"my_char_driver");
   
}



module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rakesh Arumalla");
MODULE_DESCRIPTION("Char driver Kernel Module");
