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
      del_timer(&tmp->wakeup_timer);
      list_del(pos);
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
   char op;
   printk("in write\n");
   buf = (char *)kmalloc(count, GFP_KERNEL);
   copy_from_user(buf, buffer, count);
   op = (char) buf[0];
   switch(op){
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
   printk("out write\n");
   kfree(buf);
   return count;
}

void wakeup_f(unsigned int pid){
   struct linkedlist *tmp = find_linkedlist_by_pid(pid);
   mutex_lock(&lock);
   tmp->task_state = READY;
   tmp->start_time = jiffies_to_msecs(jiffies);
   mutex_unlock(&lock);
   wake_up_process(dispatching_t);
}

// register handler
void registration_handler(char *buf){
   struct linkedlist *cur_task;
   cur_task = (struct linkedlist *)kmem_cache_alloc(cache, GFP_KERNEL);
   sscanf(&buf[2], "%u %lu %lu", &cur_task->pid, &cur_task->period, &cur_task->computation);
   printk("pid: %u\n", cur_task->pid);
   if(admission_control(cur_task->period, cur_task->computation) == -1){
      // printk("Not able to register pid %u due to admission_control");
      kmem_cache_free(cache, cur_task);
      return;
   }
   cur_task->linux_task = find_task_by_pid(cur_task->pid);
   cur_task->task_state = SLEEPING;
   cur_task->first_yield_call = 1;
   setup_timer(&(cur_task->wakeup_timer), (void *)wakeup_f, cur_task->pid);

   mutex_lock(&lock);
   list_add(&(cur_task->list), &(reglist.list));
   mutex_unlock(&lock);
   printk("out reg handler\n");
}
// yield handler
void yield_handler(char *buf){
   unsigned int pid;
   struct linkedlist *cur_task;
   unsigned long running_t, sleeping_t;
   // struct sched_param sparam; 
   printk("in yield_handler\n");
   sscanf(&buf[2], "%u", &pid);
   cur_task = find_linkedlist_by_pid(pid);
   // if it is the first yield call, the running time should be 0
   if(cur_task->first_yield_call){
      running_t = 0;
      cur_task->first_yield_call = 0;
   }
   else{
      running_t = jiffies_to_msecs(jiffies) - cur_task->start_time;
   }
   // set timer if running time is within one period
   sleeping_t = cur_task->period - running_t;
   if(sleeping_t > 0){
      mod_timer(&(cur_task->wakeup_timer), msecs_to_jiffies(sleeping_t) + jiffies);

      mutex_lock(&lock);
      cur_task->task_state = SLEEPING;
      mutex_unlock(&lock);
      set_task_state(cur_task->linux_task, TASK_UNINTERRUPTIBLE);
      // sparam.sched_priority=0; 
      // sched_setscheduler(cur_task->linux_task, SCHED_NORMAL, &sparam);
      schedule();
      if (running_task != NULL && cur_task->pid == running_task->pid){
         running_task = NULL;
      }
   }
   wake_up_process(dispatching_t); 
   printk("out yield_handler\n");
}
// de-register handler
void de_registration_handler(char *buf){
   unsigned int pid;
   struct linkedlist *tmp;
   printk("in de_registration_handler\n");
   sscanf(&buf[2], "%u", &pid);

   mutex_lock(&lock);
   // find the corresponing task and delete
   list_for_each_entry(tmp, &reglist.list, list){
      if(tmp->pid == pid){
         del_timer(&tmp->wakeup_timer);
         if (running_task && running_task->pid == tmp->pid)
            running_task = NULL;
         list_del(&tmp->list);
         kmem_cache_free(cache, tmp);
         break;
      }

   }
   mutex_unlock(&lock);
   printk("out de_registration_handler\n");
}

struct linkedlist *get_best_ready_task(void){
   struct linkedlist *next_task = NULL;
   list_for_each_safe(pos, q, &reglist.list){
      tmp= list_entry(pos, struct linkedlist, list);
      if (tmp->task_state != READY)  continue;
      if (next_task == NULL)  next_task = tmp;
      else if (tmp->period < next_task->period){
         next_task = tmp;
      }
   }
   return next_task;
}

struct linkedlist* find_linkedlist_by_pid(unsigned int pid){
   list_for_each_safe(pos, q, &reglist.list){
      tmp= list_entry(pos, struct linkedlist, list);
      if(tmp->pid == pid){
         return tmp;
      }
   }
   return NULL;
}

int admission_control(unsigned long period, unsigned long computation){
   unsigned long res = 0;
   list_for_each_safe(pos, q, &reglist.list){
      tmp= list_entry(pos, struct linkedlist, list);
      res += tmp->computation * 1000 / tmp->period;
   }
   res += computation * 1000 / period;
   if (res <= 693)   return 0;
   return -1;
}
int dispatching_t_fn(void *data){
   struct linkedlist *next_task;
   struct linkedlist *old_task = NULL;
   struct sched_param sparam;
   while(1){
      next_task = get_best_ready_task();
      if (running_task){
         old_task = find_linkedlist_by_pid(running_task->pid);
         sparam.sched_priority=0;
         sched_setscheduler(running_task, SCHED_NORMAL, &sparam);
      }
      else{
         old_task = NULL;
      }
      if (next_task){
         if(old_task && old_task->task_state == RUNNING && next_task->period < old_task->period){
            mutex_lock(&lock);
            old_task->task_state = READY;
            mutex_unlock(&lock);
         }
         else if(old_task && next_task->period < old_task->period){
            mutex_lock(&lock);
            next_task->task_state = RUNNING;
            mutex_unlock(&lock);
            wake_up_process(next_task->linux_task);
            sparam.sched_priority = 99;
            sched_setscheduler(next_task->linux_task, SCHED_FIFO, &sparam);
            running_task = next_task->linux_task;
         }
         else if(!old_task){
            mutex_lock(&lock);
            next_task->task_state = RUNNING;
            mutex_unlock(&lock);
            wake_up_process(next_task->linux_task);
            sparam.sched_priority = 99;
            sched_setscheduler(next_task->linux_task, SCHED_FIFO, &sparam);
            running_task = next_task->linux_task;
         }
      }

      set_current_state(TASK_INTERRUPTIBLE);
      schedule();
   }
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

   wake_up_process(dispatching_t);
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
