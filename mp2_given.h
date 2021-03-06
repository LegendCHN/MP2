#ifndef __MP2_GIVEN_INCLUDE__
#define __MP2_GIVEN_INCLUDE__

#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/timer.h>

struct linkedlist{
	struct list_head list;
	struct task_struct* linux_task;
	struct timer_list wakeup_timer;

	unsigned int task_state;
	unsigned int pid;
	unsigned int first_yield_call;
	unsigned long time;
	unsigned long period;
	unsigned long computation;
	unsigned long start_time;
};

struct task_struct* find_task_by_pid(unsigned int nr)
{
	struct task_struct* task;
	rcu_read_lock();
	task=pid_task(find_vpid(nr), PIDTYPE_PID);
	rcu_read_unlock();
	return task;
}

int admission_control(unsigned long period, unsigned long computation);
struct linkedlist* find_linkedlist_by_pid(unsigned int pid);
struct linkedlist *get_best_ready_task(void);
void free_linkedlist(void);
int dispatching_t_fn(void *data);
void wakeup_f(unsigned int pid);
void registration_handler(char *buf);
void yield_handler(char *buf);
void de_registration_handler(char *buf);
static ssize_t mp2_write (struct file *file, const char __user *buffer, size_t count, loff_t *data);
static ssize_t mp2_read (struct file *file, char __user *buffer, size_t count, loff_t *data);
int __init mp2_init(void);
void __exit mp2_exit(void);
#endif
