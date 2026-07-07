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
static r, w;

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

static int read_key(int pos)
{
    int key = 0;
    if(!is_key_buf_empty())
    {
        key = gpio_keys[pos];
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
	key = get_key();
	err = copy_to_user(buf, &key, 4);
	
	return 4;
}

static unsigned int gpio_drv_poll (struct file *file, poll_table *wait)
{
    poll_wait(file, &gpio_wait_q, wait); // this wait, just to upload the wait queue to kernal
    return is_key_buf_empty ? 0 : POLLIN | POLLRDNORM; // tell app: data come in and readable
}