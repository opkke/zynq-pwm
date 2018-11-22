# xilinx-zynq-pwm

## Using the driver

### Compile
compile zynq-pwm.c
```
[gty@zedminer zynq-pwm]$ make
[gty@zedminer zynq-pwm]$ ls
Makefile  Module.symvers   modules.order zynq-pwm.c  zynq-pwm.ko  zynq-pwm.mod.c  zynq-pwm.mod.o  zynq-pwm.o

```

### Insmod
```
[gty@zedminer zynq-pwm]$ sudo insmod zynq-pwm.ko
```

### Export the pwm channel
```
[gty@zedminer zynq-pwm]$ echo 0 > /sys/class/pwm/pwmchip0/export
[gty@zedminer zynq-pwm]$ cd /sys/class/pwm/pwmchip0/pwm0/
[gty@zedminer zynq-pwm]$ ls
capture  duty_cycle  enable  period  polarity  power  uevent
```

### Set the duty cycle and period
 ```
 echo 100000 > period		//period is 100us		
 echo 30000  > duty_cycle	//duty_cycle of 30 us		
 echo 1 > enable	
 ```
 
