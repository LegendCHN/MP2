#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp2_given.h"

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h> 
#include <linux/slab.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP2");

#define FILENAME "status"
#define DIRECTORY "mp2"
#define DEBUG 1
#define MASK 0666

// file operations for file status
static const struct file_operations mp2_file = {
   .owner = THIS_MODULE,
   .read = mp2_read,
   .write = mp2_write,
};

// variables declaration
static struct proc_dir_entry *proc_dir;
static struct proc_dir_entry *proc_entry;
static struct linkedlist reglist;
static struct linkedlist *tmp;
static struct list_head *pos, *q;
static struct mutex lock;

// helper function to free linkedlist space
void free_linkedlist(void){
   list_for_each_safe(pos, q, &reglist.list){
       tmp= list_entry(pos, struct linkedlist, list);
       list_del(pos);
       kfree(tmp);
   }
}

// read function to show pid and cpu time
static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
   int copied;
   int length;
   char * buf;
   buf = (char *) kmalloc(count, GFP_KERNEL);
   copied = 0;
   // destroy to prevent furthur read
   if(*data > 0){
      kfree(buf);
      return 0;
   }

   list_for_each_entry(tmp, &reglist.list, list){
      length = sprintf(buf + copied, "PID: %d, Period: %lu, Processing Time: %lu\n", tmp->pid, tmp->period, tmp->computation);
      copied += length;
   }
   copy_to_user(buffer, buf, copied);
   kfree(buf);

   *data += copied;
   return copied ;
}

// write function to add pid list entry to linkedlist
static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){
   // char buf[100];
   // tmp = (struct linkedlist*)kmalloc(sizeof(struct linkedlist), GFP_KERNEL);
   // copy_from_user(buf, buffer, count);
   // // read in cpu number and initialize time to 0
   // sscanf(buf, "%d", &tmp->pid);
   // tmp->time = 0;
   // // critical section to write
   // mutex_lock(&lock);
   // list_add(&(tmp->list), &(reglist.list));
   // mutex_unlock(&lock);
   return count;
}

// mp2_init - Called when module is loaded
int __init mp2_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif
   // create dir and file under /proc
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, MASK, proc_dir, & mp2_file);

   // initialize linkedlist and lock
   INIT_LIST_HEAD(&reglist.list);
   mutex_init(&lock);

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
   #endif

   // destroy linked list
   free_linkedlist();
   // destroy dir and file
   remove_proc_entry(FILENAME, proc_dir);
   remove_proc_entry(DIRECTORY, NULL);
   printk(KERN_ALERT "MP2 MODULE UNLOADED\n");
}

// Register init and exit funtions
module_init(mp2_init);
module_exit(mp2_exit);
