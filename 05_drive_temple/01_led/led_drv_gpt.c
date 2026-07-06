#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>

#define DEVICE_NAME "myled"
#define CLASS_NAME  "myled_class"

/*
 * 这里换成你板子上实际连接 LED 的 GPIO 编号
 * 注意：这是 Linux GPIO number，不一定等于芯片手册里的 GPIO 引脚号
 */
#define LED_GPIO 17

static int major;
static struct class *led_class;
static struct device *led_device;

static ssize_t led_write(struct file *file,
                         const char __user *buf,
                         size_t count,
                         loff_t *ppos)
{
    char val;

    if (count < 1)
        return -EINVAL;

    if (copy_from_user(&val, buf, 1))
        return -EFAULT;

    if (val == '1') {
        gpio_set_value(LED_GPIO, 1);
        printk(KERN_INFO "myled: LED ON\n");
    } else if (val == '0') {
        gpio_set_value(LED_GPIO, 0);
        printk(KERN_INFO "myled: LED OFF\n");
    } else {
        printk(KERN_WARNING "myled: invalid value, use 0 or 1\n");
        return -EINVAL;
    }

    return count;
}

static const struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .write = led_write,
};

static int __init led_init(void)
{
    int ret;

    printk(KERN_INFO "myled: init\n");

    /*
     * 1. 注册字符设备
     */
    major = register_chrdev(0, DEVICE_NAME, &led_fops);
    if (major < 0) {
        printk(KERN_ERR "myled: failed to register char device\n");
        return major;
    }

    /*
     * 2. 创建设备类
     */
    led_class = class_create(CLASS_NAME);
    if (IS_ERR(led_class)) {
        ret = PTR_ERR(led_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "myled: failed to create class\n");
        return ret;
    }

    /*
     * 3. 创建设备节点 /dev/myled
     */
    led_device = device_create(led_class, NULL,
                               MKDEV(major, 0),
                               NULL,
                               DEVICE_NAME);
    if (IS_ERR(led_device)) {
        ret = PTR_ERR(led_device);
        class_destroy(led_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "myled: failed to create device\n");
        return ret;
    }

    /*
     * 4. 申请 GPIO
     */
    ret = gpio_request(LED_GPIO, "myled_gpio");
    if (ret) {
        device_destroy(led_class, MKDEV(major, 0));
        class_destroy(led_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "myled: failed to request GPIO %d\n", LED_GPIO);
        return ret;
    }

    /*
     * 5. 设置 GPIO 为输出，初始值为 0
     */
    ret = gpio_direction_output(LED_GPIO, 0);
    if (ret) {
        gpio_free(LED_GPIO);
        device_destroy(led_class, MKDEV(major, 0));
        class_destroy(led_class);
        unregister_chrdev(major, DEVICE_NAME);
        printk(KERN_ERR "myled: failed to set GPIO direction\n");
        return ret;
    }

    printk(KERN_INFO "myled: loaded, major = %d\n", major);

    return 0;
}

static void __exit led_exit(void)
{
    printk(KERN_INFO "myled: exit\n");

    /*
     * 卸载驱动前先关灯
     */
    gpio_set_value(LED_GPIO, 0);

    /*
     * 释放 GPIO
     */
    gpio_free(LED_GPIO);

    /*
     * 删除 /dev/myled
     */
    device_destroy(led_class, MKDEV(major, 0));

    /*
     * 删除 class
     */
    class_destroy(led_class);

    /*
     * 注销字符设备
     */
    unregister_chrdev(major, DEVICE_NAME);
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Oliver");
MODULE_DESCRIPTION("Simple GPIO LED driver using legacy GPIO API");