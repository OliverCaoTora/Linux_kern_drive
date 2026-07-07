#include <linux/module.h>
#include <linux/poll.h>

#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/mutex.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/tty.h>
#include <linux/kmod.h>
#include <linux/gfp.h>
#include <linux/gpio/consumer.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/timer.h>

struct gpio_desc{
    int gpio_num;
    int irq;
    char *name;
    int key;
    struct timer_list key_timer;
};

static struct gpio_desc gpios[2] = {
    {115, 0, "sr501"},
};

// major number
static int major = 0;
// device class
static struct class *gpio_class;

// ring buffer
#define BUF_LEN 128
static int gpio_keys[BUF_LEN];
static int r, w;

struct fasync_struct *button_fasync;

#define NEXT_POS(x) ((x + 1) % BUF_LEN)

static int is_key_buf_empty(void)
{
    return ( r == w );
}

static int is_key_buf_full(void)
{
    return ( r == NEXT_POS(w) );
}

static void put_key(int key_value)
{
    if ( !is_key_buf_full() ){
        gpio_keys[w] = key_value;
        w = NEXT_POS(w);
    }
}

static int read_key(void)
{
    int key = 0;
    if(!is_key_buf_empty())
    {
        key = gpio_keys[r];
        r = NEXT_POS(r);
    }
    return key;
}

static DECLARE_WAIT_QUEUE_HEAD(gpio_wait_q);
/* Equal to:
static wait_queue_head_t gpio_wait;

static int __init gpio_drv_init(void)
{
    init_waitqueue_head(&gpio_wait);
    return 0;
}
*/

/* read */
static ssize_t gpio_drv_read (struct file *file, char __user *buf, size_t size, loff_t *offset)
{
	//printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	int err;
	int key;

    // Non Block Flag
	if (is_key_buf_empty() && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;
	
    // Block
	wait_event_interruptible(gpio_wait_q, !is_key_buf_empty());
	key = read_key();
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

/* poll */
static unsigned int gpio_drv_poll (struct file *file, poll_table *wait)
{
    poll_wait(file, &gpio_wait_q, wait); // this wait, just to upload the wait queue to kernal
    return is_key_buf_empty() ? 0 : POLLIN | POLLRDNORM; // tell app: data come in and readable
}

/* fasync */
static int gpio_drv_fasync(int fd, struct file *file, int on)
{
	if (fasync_helper(fd, file, on, &button_fasync) >= 0)
		return 0;
	else
		return -EIO;
}

/* 定义自己的file_operations结构体 */
static struct file_operations gpio_key_drv = {
	.owner	 = THIS_MODULE,
	.read    = gpio_drv_read,
	.poll    = gpio_drv_poll,
	.fasync  = gpio_drv_fasync,
};

static irqreturn_t gpio_key_isr(int irq, void *device_id)
{
    struct gpio_desc *gpio_desc = device_id;
    int val;
    int key;
   
    printk("gpio_key_isr key %d irq happened\n", gpio_desc->gpio_num);

    val = gpio_get_value(gpio_desc->gpio_num);
    key = (gpio_desc->key) | (val<<8);
    put_key(key);
    wake_up_interruptible(&gpio_wait_q);
    kill_fasync(&button_fasync, SIGIO, POLL_IN);

    return IRQ_HANDLED;
}

/* entry init function */
static int __init gpio_drv_init(void)
{
    int err;
    int i;
    int count = sizeof(gpios)/sizeof(gpios[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
	
	for (i = 0; i < count; i++)
	{		
		gpios[i].irq  = gpio_to_irq(gpios[i].gpio_num);

		err = request_irq(gpios[i].irq, gpio_key_isr, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, gpios[i].name, &gpios[i]);
	}

	/* 注册file_operations 	*/
	major = register_chrdev(0, "sr501_major", &gpio_key_drv);  /* /dev/gpio_desc */

	gpio_class = class_create(THIS_MODULE, "sr501_class");
	if (IS_ERR(gpio_class)) {
		printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);
		unregister_chrdev(major, "sr501_major");
		return PTR_ERR(gpio_class);
	}

	device_create(gpio_class, NULL, MKDEV(major, 0), NULL, "sr501"); /* /dev/sr501 */
	
	return err;
}

/* 有入口函数就应该有出口函数：卸载驱动程序时，就会去调用这个出口函数
 */
static void __exit gpio_drv_exit(void)
{
    int i;
    int count = sizeof(gpios)/sizeof(gpios[0]);
    
	printk("%s %s line %d\n", __FILE__, __FUNCTION__, __LINE__);

	device_destroy(gpio_class, MKDEV(major, 0));
	class_destroy(gpio_class);
	unregister_chrdev(major, "100ask_gpio_key");

	for (i = 0; i < count; i++)
	{
		free_irq(gpios[i].irq, &gpios[i]);
	}
}


module_init(gpio_drv_init);
module_exit(gpio_drv_exit);

MODULE_LICENSE("GPL");