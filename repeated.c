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
    char *string;
    int maxSize;
    struct list_head minorsListHead;
} myData;

struct SeeksListNode
{
    unsigned int minorNumber;
    list_t ptr;
};

int init_module(void)
{
    // This function is called when inserting the module using insmod

    my_major = register_chrdev(my_major, MY_DEVICE, &my_fops);

    if (my_major < 0)
    {
	printk(KERN_WARNING "can't get dynamic major\n");
	return my_major;
    }

    myData.string = NULL;
    myData.maxSize = 0;
    INIT_LIST_HEAD(&myData.minorsListHead);
    return 0;
}


void cleanup_module(void)
{
    // This function is called when removing the module using rmmod

    unregister_chrdev(my_major, MY_DEVICE);

    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.minorsListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
        list_del(currentSeekNodePtr);
        kfree(currentSeekNode);
    }
    return;
}


int my_open(struct inode *inode, struct file *filp) // TODO - is the filp initialized? 
{
    // handle open
    unsigned int minorNumber = MINOR(inode->i_rdev);
    unsigned int *minorPtr = IsMinorExist(&(myData.minorsListHead), minorNumber);
    if (minorPtr == NULL) // TODO - should we reset seek to 0 if it's already open?
    {
        struct SeeksListNode *newSeekNode = kmalloc(sizeof(struct SeeksListNode), GFP_KERNEL);
        newSeekNode->minorNumber = minorNumber;
        list_add_tail(&(newSeekNode->ptr), &(myData.minorsListHead));
        filp->private_data = &(newSeekNode->minorNumber);
    }
    else 
    {
        filp->private_data = minorPtr;
    }

    return 0;
}


int my_release(struct inode *inode, struct file *filp) // TODO - should we close and free the filp?
{
    // handle file closing
    unsigned int minorNumber = MINOR(inode->i_rdev);
    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.minorsListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
        if(currentSeekNode->minorNumber == minorNumber)
        {
            list_del(currentSeekNodePtr);
            kfree(currentSeekNode);
            break;
        }
    }
    filp->private_data = NULL;
    return 0;
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp, buf, and f_pos?
{
    //
    // Do write operation.
    // Return number of bytes written.
    if(!IsMinorExist(&(myData.minorsListHead), *(unsigned int*)(filp->private_data)) == NULL)
    {
        return -EFAULT; // TODO - is this the correct error?
    }
    myData.maxSize += count;
    return 0; 
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp?, should we use the f_pos or flip->f_pos?
{
    //
    // Do read operation.
    // Return number of bytes read.
    if(buf == NULL)
    {
        return -EFAULT;
    }

    if(!IsMinorExist(&(myData.minorsListHead), *(unsigned int*)(filp->private_data)) == NULL)
    {
        return -EFAULT; // TODO - is this the correct error?
    }

    int copyToUserReturnValue = 0;
    unsigned int totalLengthCopied = 0;
    unsigned int currentLengthCopied = 0;
    unsigned int modPosition = *f_pos % strlen(myData.string);
    // read the string in a loop until we reach the end of the string or the end of the buffer
    while(*f_pos < myData.maxSize)
    {
        currentLengthCopied = ((strlen(myData.string) - modPosition) < (count - totalLengthCopied)) ? (strlen(myData.string) - modPosition) : (count - totalLengthCopied);
        copyToUserReturnValue = copy_to_user(buf + totalLengthCopied, myData.string + modPosition, currentLengthCopied);
        if(copyToUserReturnValue != 0)
        {
            return -EBADF;
        }
        
        totalLengthCopied += currentLengthCopied;
        *f_pos += currentLengthCopied;
        modPosition = *f_pos % strlen(myData.string);
    }
    
    return totalLengthCopied; 
}

loff_t my_llseek(struct file *filp, loff_t a, int num) // TODO - why we need filp?
{
    //
    // Do lseek operation.
    // Return new position.

    if(!IsMinorExist(&(myData.minorsListHead), *(unsigned int*)(filp->private_data)) == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }

    loff_t newSeekPosition = a + num;
    if(newSeekPosition < 0)
    {
        newSeekPosition = 0;
    }
    else if(newSeekPosition > myData.maxSize)
    {
        newSeekPosition = myData.maxSize;
    }
    filp->f_pos = newSeekPosition;
    return newSeekPosition;
}

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg) // TODO - why we need filp and inode?
{
    if(!IsMinorExist(&(myData.minorsListHead), *(unsigned int*)(filp->private_data)) == NULL)
    {
        return -EBADF; // TODO - is this the correct error?
    }

    switch(cmd)
    {
        case SET_STRING:
            copy_from_user(myData.string, (char*)arg, strlen((char*)arg)); // TODO - how does it know the string?
        break;

        case RESET:
            myData.maxSize = 0;
            myData.string = NULL;
        break;

        default:
            return -ENOTTY;
    }

    return 0;
}

unsigned int *IsMinorExist(struct list_head* minorsListHead, unsigned int minorNumber)
{
    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, minorsListHead)
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
        if(currentSeekNode->minorNumber == minorNumber)
            return &(currentSeekNode->minorNumber);
    }
    return NULL;
}
