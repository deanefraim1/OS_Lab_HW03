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

struct file_operations my_fops = {
    // TODO - do we need more functions?
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

int Min(int a, int b, int c);

int init_module(void)
{
    // This function is called when inserting the module using insmod
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

    return 0;
}

int my_release(struct inode *inode, struct file *filp)
{
    // handle file closing
    return 0; // TODO - should we return 0?
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp, buf, and f_pos?
{
    //
    // Do write operation.
    // Return number of bytes written.
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
    if (buf == NULL)
    {
        return -EFAULT;
    }
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if (minorsListNodePtr == NULL)
    {
        return -EFAULT; // TODO - is this the correct error?
    }
    long stringLength = strlen(minorsListNodePtr->string);
    printk("stringLength = %ld\n", stringLength);
    printk("string is %s\n", minorsListNodePtr->string);
    printk("count = %ld\n", count);
    if (stringLength < 0)
    {
        return -EFAULT;
    }
    unsigned int totalLengthCopied = 0;
    unsigned int currentLengthCopied = 0;
    unsigned int modPosition = (unsigned long)(*f_pos) % stringLength;
    int copyToUserReturnValue = 0;
    int round = 0;
    while((*f_pos < minorsListNodePtr->maxSize) && (totalLengthCopied < count))
    {
        printk("round = %d\n", round);
        printk("modPosition = %d, totalLengthCopied = %d\n", modPosition, totalLengthCopied);
        printk("stringLength - modPosition = %d, count - totalLengthCopied = %d, minorsListNodePtr->maxSize - *f_pos = %d\n", stringLength - modPosition, count - totalLengthCopied, minorsListNodePtr->maxSize - *f_pos);
        currentLengthCopied = Min(stringLength - modPosition, count - totalLengthCopied, minorsListNodePtr->maxSize - *f_pos);
        printk("currentLengthCopied = %d\n", currentLengthCopied);
        copyToUserReturnValue = copy_to_user(buf + totalLengthCopied, minorsListNodePtr->string + modPosition, currentLengthCopied);
        printk("buf is %s\n", buf);
        if (copyToUserReturnValue != 0)
        {
            return -EBADF;
        }
        totalLengthCopied += currentLengthCopied;
        *f_pos += currentLengthCopied;
        modPosition = (unsigned long)(*f_pos) % stringLength;
        round++;
    }
    return totalLengthCopied;
}

loff_t my_llseek(struct file *filp, loff_t a, int num) // TODO - what is the a and num?
{
    //
    // Do lseek operation.
    // Return new position.
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    if (minorsListNodePtr == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }

    loff_t newSeekPosition = filp->f_pos + a;
    if (newSeekPosition < 0)
    {
        newSeekPosition = 0;
    }
    else if (newSeekPosition > minorsListNodePtr->maxSize)
    {
        newSeekPosition = minorsListNodePtr->maxSize;
    }
    filp->f_pos = newSeekPosition;
    return newSeekPosition;
}

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    long stringLength;
    if (minorsListNodePtr == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }
    switch (cmd)
    {
    case SET_STRING:
        if(minorsListNodePtr->string != NULL)
        {
            kfree(minorsListNodePtr->string);
        }
        stringLength = strlen_user((char *)arg); // including the null terminator
        if (stringLength == 0)
        {
            return -EINVAL; // TODO - is this the correct error?
        }
        minorsListNodePtr->string = kmalloc(stringLength * sizeof(char), GFP_KERNEL);
        int copyFromUserReturnValue = copy_from_user(minorsListNodePtr->string, (char *)arg, stringLength); // returns the number of bytes that were not copied not including the null terminator
        stringLength = strlen(minorsListNodePtr->string); // including the null terminator
        if(copyFromUserReturnValue != 0)
        {
            return -EFAULT;
        }
        break;

    case RESET:
        minorsListNodePtr->maxSize = 0;
        kfree(minorsListNodePtr->string);
        minorsListNodePtr->string = NULL;
        break;

    default:
        return -ENOTTY; // TODO - is this the correct error?
    }

    return 0;
}

struct MinorsListNode *GetMinorListNodePtr(struct file *filp)
{
    printk("Starting GetMinorListNodePtr\n");
    if (filp->private_data == NULL)
    {
        return NULL;
    }
    unsigned int minorNumber = ((struct MinorsListNode *)(filp->private_data))->minorNumber;
    list_t *currentSeekNodePtr;
    struct MinorsListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.minorsListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct MinorsListNode, ptr);
        if (currentSeekNode->minorNumber == minorNumber)
        {
            printk("Finished GetMinorListNodePtr and found minor\n");
            return currentSeekNode;
        }
    }
    printk("Finished GetMinorListNodePtr and couldn't find minor\n");
    return NULL;
}

int Min(int a, int b, int c)
{
    if (a <= b && a <= c)
    {
        return a;
    }
    else if (b <= a && b <= c)
    {
        return b;
    }
    else
    {
        return c;
    }
}
