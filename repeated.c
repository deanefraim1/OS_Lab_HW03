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
#define SEEK_NOT_FOUND -1

MODULE_AUTHOR("Anonymous");
MODULE_LICENSE("GPL");

/* globals */
int my_major = 0; /* will hold the major # of my device driver */

struct file_operations my_fops = {
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
    struct list_head seeksListHead;
} myData;

struct SeeksListNode
{
    unsigned int minorNumber;
    struct file *filp;
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
    INIT_LIST_HEAD(&myData.seeksListHead);
    return 0;
}


void cleanup_module(void)
{
    // This function is called when removing the module using rmmod

    unregister_chrdev(my_major, MY_DEVICE);

    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.seeksListHead))
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
    unsigned int seek = GetSeek(&(myData.seeksListHead), minorNumber);
    if(seek == SEEK_NOT_FOUND)
    {
        struct SeeksListNode *newSeekNode = kmalloc(sizeof(struct SeeksListNode), GFP_KERNEL);
        newSeekNode->minorNumber = minorNumber;
        newSeekNode->filp->f_pos = 0;
        list_add_tail(&(newSeekNode->ptr), &(myData.seeksListHead));
    }
    else // TODO - should we reset seek to 0 if it's already open?
    {
        struct SeeksListNode *currentSeekNode;
        list_t *currentSeekNodePtr;
        list_for_each(currentSeekNodePtr, &(myData.seeksListHead))
        {
            currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
            if(currentSeekNode->minorNumber == minorNumber)
            {
                currentSeekNode->filp->f_pos = 0;
                break;
            }
        }
    }

    return 0;
}


int my_release(struct inode *inode, struct file *filp) // TODO - should we close and free the filp?
{
    // handle file closing
    unsigned int minorNumber = MINOR(inode->i_rdev);
    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, &(myData.seeksListHead))
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
        if(currentSeekNode->minorNumber == minorNumber)
        {
            list_del(currentSeekNodePtr);
            kfree(currentSeekNode);
            break;
        }
    }
    return 0;
}

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp, buf, and f_pos?
{
    //
    // Do write operation.
    // Return number of bytes written.
    myData.maxSize += count;
    return 0; 
}

ssize_t my_read(struct file *filp, char *buf, size_t count, loff_t *f_pos) // TODO - why we need filp and buf?, should we use the f_pos or the one in our struct?
{
    //
    // Do read operation.
    // Return number of bytes read.
    if(buf == NULL)
    {
        return -EFAULT;
    }

    int copyToUserReturnValue = 0;
    unsigned int currentFailedLngthCopied = 0;
    unsigned int totalLengthCopied = 0;
    unsigned int currentLengthCopied = 0;
    unsigned int modPosition = *f_pos % strlen(myData.string);
    // read the string in a loop until we reach the end of the string or the end of the buffer
    while(*f_pos < myData.maxSize)
    {
        currentLengthCopied = strlen(myData.string) - modPosition < count - totalLengthCopied ? strlen(myData.string) - modPosition : count - totalLengthCopied;
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

unsigned int GetSeek(struct list_head* secretsList, unsigned int minorNumber)
{
    list_t *currentSeekNodePtr;
    struct SeeksListNode *currentSeekNode;
    list_for_each(currentSeekNodePtr, secretsList)
    {
        currentSeekNode = list_entry(currentSeekNodePtr, struct SeeksListNode, ptr);
        if(currentSeekNode->minorNumber == minorNumber)
            return currentSeekNode->filp->f_pos;
    }
    return SEEK_NOT_FOUND;
}
