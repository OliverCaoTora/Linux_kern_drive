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

static int major;

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
    // {132, 0, "led1"} // imx6ull setting
};

static struct class *led_class;

static ssize_t led_read(struct file *file, 
                        char __user *buf,
                        size_t size,
                        loff_t *ppos)
{
    int err;
    unsigned char tmp_buf[2];
    int index;
    int value;
    int count = sizeof(gpios) / sizeof(gpios[0]);

    if (size != 2)
        return -EINVAL;

    if (copy_from_user(tmp_buf, buf, 2))
        return -EFAULT;

    index = tmp_buf[0];
    value = tmp_buf[1];

    if (index < 0 || index >= count)
        return -EINVAL;

    if (value != 0 && value != 1)
        return -EINVAL;

    tmp_buf[1] = gpio_get_value(gpios[tmp_buf[0]].gpio_num);

	err = copy_to_user(buf, tmp_buf, 2);

    return 2;
}

static ssize_t led_write(struct file *file, const char __user *buf, size_t size, loff_t *offset)
{
    unsigned char command_buf[2];
    int err;

    if (size != 2)
        return -EINVAL;

    err = copy_from_user(command_buf, buf, size);
    
    if (command_buf[0] >= sizeof(gpios)/sizeof(gpios[0]))
        return -EINVAL;

    gpio_set_value(gpios[command_buf[0]].gpio_num, command_buf[1]);
    return 2;    
}

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .write = led_write,
    .read = led_read
};

/* init */
static int __init gpio_drv_init(void)
{
    int err;
    int dev_count = sizeof(gpios)/sizeof(gpios[0]);
    int ret;
    int i;

    /* 1. GPIO request */
    for (i = 0; i < dev_count; i++)
    {
        err = gpio_request(gpios[i].gpio_num, gpios[i].name);
        if(err < 0)
        {
            printk("can not request gpio %s %d\n", gpios[i].name, gpios[i].gpio_num);
            return -ENODEV;
        }

        /* 2. GPIO output/input */
        ret = gpio_direction_output(gpios[i].gpio_num, 1);
    }

    /* 3. register device */
    major = register_chrdev(0, "myled", &led_fops);
    if (major < 0) {
        printk(KERN_ERR "myled: failed to register char device\n");
        return major;
    }

    /* 4. create device node */
    led_class = class_create(THIS_MODULE, "myled_class");
    if (IS_ERR(led_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "myled");
		return PTR_ERR(led_class);
	}
    device_create(led_class, NULL, MKDEV(major, 0), NULL, "myled_dev");

    return 0;

}

static void __exit gpio_drv_exit(void)
{
    int i;
    int dev_count = sizeof(gpios)/sizeof(gpios[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(led_class, MKDEV(major, 0));
	class_destroy(led_class);
	unregister_chrdev(major, "myled");

	for (i = 0; i < dev_count; i++)
	{
		gpio_free(gpios[i].gpio_num);		
	}
}

/* 7. 其他完善：提供设备信息，自动创建设备节点*/

module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");