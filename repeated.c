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

int Min(int a, int b);

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
    printk("1\n");
    if (minorsListNodePtr == NULL)
    {
        printk("2\n");
        struct MinorsListNode *newMinorsListNode = kmalloc(sizeof(struct MinorsListNode), GFP_KERNEL);
        printk("3\n");
        newMinorsListNode->minorNumber = MINOR(inode->i_rdev);
        printk("4\n");
        list_add_tail(&(newMinorsListNode->ptr), &(myData.minorsListHead));
        printk("5\n");
        newMinorsListNode->string = NULL;
        printk("6\n");
        newMinorsListNode->maxSize = 0;
        printk("7\n");
        filp->private_data = newMinorsListNode;
        printk("8\n");
        filp->f_pos = 0;
        printk("9\n");
    }
    else
    {
        printk("10\n");
        filp->private_data = minorsListNodePtr;
        printk("11\n");
        filp->f_pos = 0;
        printk("12\n");
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
    if (buf == NULL)
    {
        return -EFAULT;
    }
    printk("1\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    printk("2\n");
    if (minorsListNodePtr == NULL)
    {
        printk("3\n");
        return -EFAULT; // TODO - is this the correct error?
    }
    printk("4\n");
    long stringLength = strlen_user(minorsListNodePtr->string) - 1;
    printk("5\n");
    printk("stringLength = %ld\n", stringLength);
    if (stringLength < 0)
    {
        printk("6\n");
        return -EFAULT;
    }
    unsigned int totalLengthCopied = 0;
    unsigned int currentLengthCopied = 0;
    unsigned int modPosition = (unsigned long)(*f_pos) % stringLength;
    int copyToUserReturnValue = 0;
    printk("7\n");
    while((*f_pos < minorsListNodePtr->maxSize) && (totalLengthCopied < count))
    {
        printk("8\n");
        currentLengthCopied = Min(stringLength - modPosition, count - totalLengthCopied);
        printk("9\n");
        copyToUserReturnValue = strncpy_from_user(buf + totalLengthCopied, minorsListNodePtr->string + modPosition, currentLengthCopied);
        printk("10\n");
        if (copyToUserReturnValue != 0)
        {
            printk("11\n");
            return -EBADF;
        }
        printk("12\n");
        totalLengthCopied += currentLengthCopied;
        printk("13\n");
        *f_pos += currentLengthCopied;
        printk("14\n");
        modPosition = (unsigned long)(*f_pos) % stringLength;
        printk("15\n");
    }
    printk("16\n");
    return totalLengthCopied;
}

loff_t my_llseek(struct file *filp, loff_t a, int num) // TODO - what is the a and num?
{
    //
    // Do lseek operation.
    // Return new position.
    printk("Starting my_llseek\n");
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
    printk("Starting my_ioctl\n");
    struct MinorsListNode *minorsListNodePtr = GetMinorListNodePtr(filp);
    printk("1\n");
    long stringLength;
    if (minorsListNodePtr == NULL)
    {
        printk("2\n");
        return -EBADF; // TODO - is this the correct error?
    }
    printk("3\n");
    switch (cmd)
    {
    case SET_STRING:
        printk("4\n");
        minorsListNodePtr->maxSize = 0;
        stringLength = strlen_user((char *)arg);
        printk("stringLength = %ld\n", stringLength);
        printk("5\n");
        if (stringLength == 0)
        {
            printk("6\n");
            return -EINVAL; // TODO - is this the correct error?
        }
        printk("7\n");
        minorsListNodePtr->string = kmalloc(stringLength * sizeof(char), GFP_KERNEL);
        printk("8\n");
        int copyFromUserReturnValue = strncpy_from_user(minorsListNodePtr->string, (char *)arg, stringLength); // TODO - how does it know the string?
        stringLength = strlen(minorsListNodePtr->string);
        printk("stringLength = %ld\n", stringLength);
        printk("string is %s\n", minorsListNodePtr->string);
        if(copyFromUserReturnValue != 0)
        {
            printk("8.5\n");
            return -EFAULT;
        }
        printk("9\n");
        break;

    case RESET:
        printk("10\n");
        minorsListNodePtr->maxSize = 0;
        kfree(minorsListNodePtr->string);
        minorsListNodePtr->string = NULL;
        printk("11\n");
        break;

    default:
        printk("12\n");
        return -ENOTTY; // TODO - is this the correct error?
    }

    printk("Finished my_ioctl\n");
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

int Min(int a, int b)
{
    return (a < b) ? a : b;
}
