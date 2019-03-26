#include "userapp.h"
#define JOBS 10

int factorial(int n)
{
    int ret = 1;
    for(int i = 1; i <= n; i++)
        ret *= i;
    return ret;
}

// register current process
void registeration(unsigned int pid, unsigned long period, unsigned long computation){
    printf("registering pid %u\n", pid);
    char cmd[100];
    sprintf(cmd, "echo R %u %lu %lu > /proc/mp2/status", pid, period, computation);
    system(cmd);
    printf("registered pid %u\n", pid);
}

// determine whether a process is registered
bool registered(unsigned int pid){
    FILE *fp;
    unsigned int tmp;
    fp = fopen("/proc/mp2/status", "r");
    
    while(fscanf(fp, "pid: %u", &tmp) != EOF){
        if(tmp == pid){
            printf("pid %u exists in /proc\n", pid);
            return true;
        }
    }
    printf("pid %u does not exist in /proc\n", pid);
    return false;
}

// finished current period and yield to other process
void yield(unsigned int pid){
    printf("yielding pid %u\n", pid);
    char cmd[100];
    sprintf(cmd, "echo Y %u > /proc/mp2/status", pid);
    system(cmd);
} 

// de-register current process
void deregisteration(unsigned int pid){
    printf("de-registering pid %u\n", pid);
    char cmd[100];
    sprintf(cmd, "echo D %u > /proc/mp2/status", pid);
    system(cmd);
}

// Processing Time of each job expressed in milliseconds
unsigned long computation_time(){
    struct timeval t0, t1;
    unsigned long elapsed;
    gettimeofday(&t0, NULL);
    factorial(500000);
    gettimeofday(&t1, NULL);
    elapsed = (t1.tv_sec-t0.tv_sec)*1000 + (t1.tv_usec-t0.tv_usec)/1000;
    return elapsed;
}

int main(int argc, char* argv[])
{
    if(argc != 2){
        printf("Please enter period:\n");
        exit(1);
    }
	unsigned int pid;
    unsigned long period, computation;
    bool success;
    unsigned int num_jobs = JOBS;
	pid = getpid();
    period = atoi(argv[1]);
    computation = computation_time();
    registeration(pid, period, computation);
    success = registered(pid);
    if(!success){
        exit(1);
    }
    yield(pid);
    // real-time loop
    while(num_jobs){
        factorial(500000);
        yield(pid);
        num_jobs--;
    }
    deregisteration(pid);
	return 0;
}
