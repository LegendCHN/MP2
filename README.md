# MP2


step1:
In folder MP2, run "make" to generate kernel module nwu10.ko and userapp

step2:
run "sudo insmod nwu10.ko" to insert module to the kernel

step3:
run "./userapp period" to register a task to the kernel module

step4:
run multiple tasks using "./userapp period1 & ./userapp period2" in another terminal to schedule multiple tasks.

step5:
run "cat /proc/mp2/status" to see the process PID information.

step6:
run "sudo rmmod nwu10" to unload module.