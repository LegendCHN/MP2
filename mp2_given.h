#ifndef __MP2_GIVEN_INCLUDE__
#define __MP2_GIVEN_INCLUDE__

#include <linux/pid.h>
#include <linux/sched.h>
struct linkedlist{
  struct list_head list;
  int pid;
  unsigned long time;
  unsigned long period;
  unsigned long computation;
};

struct task_struct* find_task_by_pid(unsigned int nr)
{
    struct task_struct* task;
    rcu_read_lock();
    task=pid_task(find_vpid(nr), PIDTYPE_PID);
    rcu_read_unlock();
    return task;
}

static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data);
static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data);
int __init mp2_init(void);
void __exit mp2_exit(void);
#endif
