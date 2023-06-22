#ifndef _REPEATED_H_
#define _REPEATED_H_

#include <linux/ioctl.h>
#include <linux/types.h>

//
// Function prototypes
//
int my_open(struct inode *, struct file *);

int my_release(struct inode *, struct file *);

ssize_t my_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

ssize_t my_read(struct file *, char *, size_t, loff_t *);

loff_t my_llseek(struct file *filp, loff_t a, int num);

int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg);

#define MY_MAGIC 'r'
#define SET_STRING  _IOW(MY_MAGIC, 0, char*)
#define RESET  _IO(MY_MAGIC, 1)

#endif
