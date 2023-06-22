/* repeated.c: Example char device module.
 *
 */
/* Kernel Programming */
#define MODULE
#define LINUX
#define __KERNEL__

#include <linux/kernel.h>  	
#include <linux/module.h>
#include <linux/fs.h>       		
#include <asm/uaccess.h>
#include <linux/errno.h>  
#include <asm/segment.h>
#include <asm/current.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ioctl.h>

#include "repeated.h"

#define MY_DEVICE "repeated"

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */

struct file_operations my_fops = { // TODO - do we need more functions?
    .open = my_open,
    .release = my_release,
    .write = my_write,
    .read = my_read,
    .llseek = my_llseek,
    .ioctl = my_ioctl,
};

struct MyData
{
    struct list_head minorsListHead;
} myData;

struct MinorsListNode
{
    unsigned int minorNumber;
    char *string;
    int maxSize;
    list_t ptr;
};

struct MinorsListNode *GetMinorListNodePtr(struct file *filp);

int init_module(void)
{
    // This function is called when inserting the module using insmod
    printk("Starting init_module\n");
    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
        printk(KERN_WARNING "can't get dynamic major\n");
        return my_major;
    }
    INIT_LIST_HEAD(&myData.minorsListHead);
    return 0;
}


void cleanup_module(void)
{
    // This function is called when removing the module using rmmod
    printk("Starting cleanup_module\n");
    unregister_chrdev(my_major, MY_DEVICE);
    list_t *currentSeekNodePtr;
    struct MinorsListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.minorsListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct MinorsListNode, ptr);
        kfree(currentSeekNode->string);
        list_del(currentSeekNodePtr);
        kfree(currentSeekNode);
    }
    return;
}


int my_open(struct inode *inode, struct file *filp) // TODO - is the filp initialized? 
{
    // handle open
    printk("Starting my_open\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if (minorsListNodePtr == NULL)
    {
        struct MinorsListNode *newMinorsListNode = kmalloc(sizeof(struct MinorsListNode), GFP_KERNEL);
        newMinorsListNode->minorNumber = MINOR(inode->i_rdev);
        list_add_tail(&(newMinorsListNode->ptr), &(myData.minorsListHead));
        newMinorsListNode->string = NULL;
        newMinorsListNode->maxSize = 0;
        filp->private_data = newMinorsListNode;
        filp->f_pos = 0;
    }
    else 
    {
        filp->private_data = minorsListNodePtr;
        filp->f_pos = 0;
    }

    printk("Finished my_open\n");
    return 0;
}


int my_release(struct inode *inode, struct file *filp)
{
    // handle file closing
    printk("Starting my_release\n");
    return 0; // TODO - should we return 0?
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp, buf, and f_pos?
{
    //
    // Do write operation.
    // Return number of bytes written.
    printk("Starting my_write\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if (minorsListNodePtr == NULL)
    {
        return -EFAULT; // TODO - is this the correct error?
    }
    minorsListNodePtr->maxSize += count;
    return 0; 
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp?, should we use the f_pos or flip->f_pos?
{
    //
    // Do read operation.
    // Return number of bytes read.
    printk("Starting my_read\n");
    if(buf == NULL)
    {
        return -EFAULT;
    }
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if(minorsListNodePtr == NULL)
    {
        return -EFAULT; // TODO - is this the correct error?
    }

    int copyToUserReturnValue = 0;
    unsigned int totalLengthCopied = 0;
    unsigned int currentLengthCopied = 0;
    unsigned int modPosition = (unsigned long)(*f_pos) % strlen(minorsListNodePtr->string);
    // read the string in a loop until we reach the end of the string or the end of the buffer
    while(*f_pos < minorsListNodePtr->maxSize)
    {
        currentLengthCopied = ((strlen(minorsListNodePtr->string) - modPosition) < (count - totalLengthCopied)) ? (strlen(minorsListNodePtr->string) - modPosition) : (count - totalLengthCopied);
        copyToUserReturnValue = copy_to_user(buf + totalLengthCopied, minorsListNodePtr->string + modPosition, currentLengthCopied);
        if(copyToUserReturnValue != 0)
        {
            return -EBADF;
        }
        
        totalLengthCopied += currentLengthCopied;
        *f_pos += currentLengthCopied;
        modPosition = (unsigned long)(*f_pos) % strlen(minorsListNodePtr->string);
    }
    
    return totalLengthCopied; 
}

loff_t my_llseek(struct file *filp, loff_t a, int num) // TODO - what is the a and num?
{
    //
    // Do lseek operation.
    // Return new position.
    printk("Starting my_llseek\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if(minorsListNodePtr == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }

    loff_t newSeekPosition = filp->f_pos + a;
    if(newSeekPosition < 0)
    {
        newSeekPosition = 0;
    }
    else if(newSeekPosition > minorsListNodePtr->maxSize)
    {
        newSeekPosition = minorsListNodePtr->maxSize;
    }
    filp->f_pos = newSeekPosition;
    return newSeekPosition;
}

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    printk("Starting my_ioctl\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    long stringLength;
    if(minorsListNodePtr == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }

    switch(cmd)
    {
        case SET_STRING:
        stringLength = strlen_user((char*)arg);
        if (stringLength == 0)
        {
            return -EINVAL; // TODO - is this the correct error?
            }
            copy_from_user(minorsListNodePtr->string, (char*)arg, stringLength); // TODO - how does it know the string?
        break;

        case RESET:
        minorsListNodePtr->maxSize = 0;
        minorsListNodePtr->string = NULL;
        break;

        default:
        return -ENOTTY; // TODO - is this the correct error?
    }

    printk("Finished my_ioctl\n");
    return 0;
}

struct MinorsListNode *GetMinorListNodePtr(struct file *filp)
{
    printk("Starting GetMinorListNodePtr\n");
    if(filp->private_data == NULL)
    {
        return NULL;
    }
    unsigned int minorNumber = ((struct MinorsListNode *)(filp->private_data))->minorNumber;
    list_t *currentSeekNodePtr;
    struct MinorsListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.minorsListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct MinorsListNode, ptr);
        if(currentSeekNode->minorNumber == minorNumber)
            return currentSeekNode;
    }
    printk("Finished GetMinorListNodePtr\n");
    return NULL;
}
