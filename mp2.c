#define LINUX

#include <linux/module.h>
#include <linux/kernel.h>
#include "mp2_given.h"

#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h> 
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/list.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Group_ID");
MODULE_DESCRIPTION("CS-423 MP2");

#define FILENAME "status"
#define DIRECTORY "mp2"
#define DEBUG 1
#define MASK 0666
#define READY 0
#define RUNNING 1
#define SLEEPING 2
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
static struct kmem_cache *cache;
static struct task_struct *dispatching_t;
static struct task_struct *running_task = NULL;
// helper function to free linkedlist space
void free_linkedlist(void){
   list_for_each_safe(pos, q, &reglist.list){
       tmp= list_entry(pos, struct linkedlist, list);
       list_del(pos);
       kfree(tmp);
       kmem_cache_free(cache, tmp);
   }
}

// read function to show pid, period and processing time
static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data){
   int copied;
   int length;
   char *buf;
   buf = (char *) kmalloc(count, GFP_KERNEL);
   copied = 0;
   // destroy to prevent furthur read
   if(*data > 0){
      kfree(buf);
      return 0;
   }

   list_for_each_entry(tmp, &reglist.list, list){
      length = sprintf(buf + copied, "pid: %u, period: %lu, processing time: %lu\n", tmp->pid, tmp->period, tmp->computation);
      copied += length;
   }
   copy_to_user(buffer, buf, copied);
   kfree(buf);

   *data += copied;
   return copied ;
}

// write function to add pid list entry to linkedlist
static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data){
   char *buf;
   char type;
   buf = (char *)kmalloc(count, GFP_KERNEL);
   copy_from_user(buf, buffer, count);
   type = (char) buf[0];
   switch(type){
      case 'R':
         registration_handler(buf);
         break;
      case 'Y':
         yield_handler(buf);
         break;
      case 'D':
         de_registration_handler(buf);
         break;
   }
   kfree(buf);
   return count;
}

void wakeup_f(unsigned int pid){
   mutex_lock(&lock);
   find_task_by_pid(pid)->state = READY;
   mutex_unlock(&lock);
   wake_up_process(dispatching_t);
}

// register handler
void registration_handler(char *buf){
   struct linkedlist *cur_task;
   cur_task = (struct linkedlist *)kmem_cache_alloc(cache, GFP_KERNEL);
   sscanf(&buf[2], "%u %lu %lu", &cur_task->pid, &cur_task->period, &cur_task->computation);

   cur_task->linux_task = find_task_by_pid(cur_task->pid);
   cur_task->task_state = SLEEPING;
   setup_timer(&(cur_task->wakeup_timer), (void *)wakeup_f, (unsigned long)cur_task);

   mutex_lock(&lock);
   list_add(&(tmp->list), &(reglist.list));
   mutex_unlock(&lock);
}
// yield handler
void yield_handler(char *buf){
   
}
// de-register handler
void de_registration_handler(char *buf){
   unsigned int pid;
   struct linkedlist *tmp;
   sscanf(&buf[2], "%u", &pid);

   mutex_lock(&lock);
   // find the corresponing task and delete
   list_for_each_entry(tmp, &reglist.list, list){
      if(tmp->pid == pid){
         del_timer(&tmp->wakeup_timer);
         if (!running_task && running_task == tmp->linux_task)
            running_task = NULL;
         list_del(&tmp->list);
         kmem_cache_free(cache, tmp);
         break;
      }

   }
   mutex_unlock(&lock);
}

int dispatching_t_fn(void *data){
   return 0;
}
// mp2_init - Called when module is loaded
int __init mp2_init(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE LOADING\n");
   #endif
   // create dir and file under /proc
   proc_dir = proc_mkdir(DIRECTORY, NULL);
   proc_entry = proc_create(FILENAME, MASK, proc_dir, &mp2_file);

   // initialize linkedlist and lock
   INIT_LIST_HEAD(&reglist.list);
   mutex_init(&lock);

   // initialize cache
   cache = kmem_cache_create("cache", sizeof (struct linkedlist), 0, SLAB_HWCACHE_ALIGN, NULL);
   dispatching_t = kthread_create(dispatching_t_fn, NULL, "dispatching_t");

   printk(KERN_ALERT "MP2 MODULE LOADED\n");
   return 0;   
}

// mp2_exit - Called when module is unloaded
void __exit mp2_exit(void)
{
   #ifdef DEBUG
   printk(KERN_ALERT "MP2 MODULE UNLOADING\n");
   #endif

   kthread_stop(dispatching_t);
   kmem_cache_destroy(cache);
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
