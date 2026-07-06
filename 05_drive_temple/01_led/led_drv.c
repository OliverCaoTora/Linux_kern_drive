#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/gpio.h>

#include <linux/timer.h>

#include "asm-generic/errno-base.h"
#include "asm-generic/gpio.h"
#include "asm/uaccess.h"

struct gpio_dev
{
    int gpio_num;
    int irq;
    char *name;
    int key;
	struct timer_list key_timer;
    
};

static struct gpio_dev gpios[2] = {
    {131, 0, "led0"},
    {132, 0, "led1"}
};

static struct class *led_class;

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .write = led_write;
}

/* init */
static int __init gpio_drv_init(void)
{
    int err;
    int dev_count = sizeof(gpios)/sizeof(gpios[0]);
    int ret;

    /* 1. GPIO request */
    for (int i = 0; i < dev_count; i++)
    {
        err = gpio_request(gpios[i].gpio_num, gpio[i].name);
        if(err < 0)
        {
            printk("can not request gpio %s %d\n", gpios[i].name, gpios[i].gpio_num);
            return -ENODEV;
        }

        ret = gpio_direction_output(gpios[i].gpio_num, 1);
    }

    /* 2. register device */
    major = register_chrdev(0, "myled", &led_fops);
    if (major < 0) {
        printk(KERN_ERR "myled: failed to register char device\n");
        return major;
    }

    /* 3. create device class */
    led_class = class_create(THIS_MODULE, "myled_class");


}