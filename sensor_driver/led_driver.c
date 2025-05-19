#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kthread.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Character Device Driver with GPIO LED control (BBB) for two LEDs");
MODULE_VERSION("1.2");

#define DEVICE_NAME "multi_led"
#define CLASS_NAME "multi_led_class"
#define BUFFER_SIZE 1024

#define GPIO1_BASE 0x4804C000
#define GPIO_SIZE  0x1000

#define GPIO_OE       0x134
#define GPIO_DATAIN   0x138
#define GPIO_DATAOUT  0x13C

#define GPIO_LED1     47  // LED1 on GPIO_47 (P8.15)
#define GPIO_BIT1     (GPIO_LED1 % 32)  // Bit 15 for GPIO_47
#define GPIO_LED2     45  // LED2 on GPIO_45 (P8.11)
#define GPIO_BIT2     (GPIO_LED2 % 32)  // Bit 13 for GPIO_45

static int major_number;
static struct class *dev_class = NULL;
static struct device *dev_device = NULL;
static char kernel_buffer[BUFFER_SIZE];
static size_t buffer_size = 0;

static void __iomem *gpio_base = NULL;

static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t, loff_t *);

// Sysfs attributes for LEDs
static ssize_t led1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t led1_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static ssize_t led2_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf);
static ssize_t led2_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count);
static struct kobj_attribute dev_attr_led1 = __ATTR(led1, 0664, led1_show, led1_store);
static struct kobj_attribute dev_attr_led2 = __ATTR(led2, 0664, led2_show, led2_store);

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .read = device_read,
    .write = device_write,
};

// Blinking thread for LED2
static struct task_struct *blink_thread = NULL;
static bool led2_blinking = false;
static bool stop_blink = false;

static int blink_led2(void *data)
{
    int val;
    while (!kthread_should_stop()) {
        if (led2_blinking) {
            val = ioread32(gpio_base + GPIO_DATAOUT);
            val ^= (1 << GPIO_BIT2); // Toggle LED2
            iowrite32(val, gpio_base + GPIO_DATAOUT);
            msleep(500); // Blink every 500ms
        } else {
            msleep(100); // Sleep when not blinking
        }
    }
    stop_blink = true;
    return 0;
}

static int __init device_init(void)
{
    int val;

    printk(KERN_INFO "Initializing character driver with GPIO (LED1: GPIO %d, bit %d; LED2: GPIO %d, bit %d)\n",
           GPIO_LED1, GPIO_BIT1, GPIO_LED2, GPIO_BIT2);

    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ALERT "Failed to register major number\n");
        return major_number;
    }

    dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dev_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(dev_class);
    }

    dev_device = device_create(dev_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev_device)) {
        class_destroy(dev_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(dev_device);
    }

    memset(kernel_buffer, 0, BUFFER_SIZE);

    // Map GPIO1 memory
    gpio_base = ioremap(GPIO1_BASE, GPIO_SIZE);
    if (!gpio_base) {
        printk(KERN_ALERT "Failed to ioremap GPIO\n");
        return -ENOMEM;
    }

    // Configure GPIO pins as output
    val = ioread32(gpio_base + GPIO_OE);
    val &= ~(1 << GPIO_BIT1); // Set GPIO_47 as output
    val &= ~(1 << GPIO_BIT2); // Set GPIO_45 as output
    iowrite32(val, gpio_base + GPIO_OE);

    // Initialize LEDs to OFF
    val = ioread32(gpio_base + GPIO_DATAOUT);
    val &= ~(1 << GPIO_BIT1); // LED1 OFF
    val &= ~(1 << GPIO_BIT2); // LED2 OFF
    iowrite32(val, gpio_base + GPIO_DATAOUT);

    printk(KERN_INFO "Driver loaded. GPIO %d and %d configured as output.\n", GPIO_LED1, GPIO_LED2);

    // Create sysfs files
    if (sysfs_create_file(&dev_device->kobj, &dev_attr_led1.attr))
        printk(KERN_ALERT "Failed to create sysfs file for LED1\n");
    if (sysfs_create_file(&dev_device->kobj, &dev_attr_led2.attr))
        printk(KERN_ALERT "Failed to create sysfs file for LED2\n");

    // Request GPIOs
    if (gpio_request(GPIO_LED1, "LED1") < 0) {
        printk(KERN_ALERT "Failed to request GPIO %d\n", GPIO_LED1);
    } else {
        gpio_direction_output(GPIO_LED1, 0);
    }
    if (gpio_request(GPIO_LED2, "LED2") < 0) {
        printk(KERN_ALERT "Failed to request GPIO %d\n", GPIO_LED2);
    } else {
        gpio_direction_output(GPIO_LED2, 0);
    }

    // Start blinking thread for LED2
    blink_thread = kthread_run(blink_led2, NULL, "led2_blink_thread");
    if (IS_ERR(blink_thread)) {
        printk(KERN_ALERT "Failed to start blink thread\n");
        return PTR_ERR(blink_thread);
    }

    return 0;
}

static void __exit device_exit(void)
{
    if (blink_thread) {
        kthread_stop(blink_thread);
        while (!stop_blink) {
            msleep(10); // Wait for thread to stop
        }
    }

    if (gpio_base)
        iounmap(gpio_base);

    sysfs_remove_file(&dev_device->kobj, &dev_attr_led1.attr);
    sysfs_remove_file(&dev_device->kobj, &dev_attr_led2.attr);

    device_destroy(dev_class, MKDEV(major_number, 0));
    class_destroy(dev_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    gpio_free(GPIO_LED1);
    gpio_free(GPIO_LED2);

    printk(KERN_INFO "Driver unloaded\n");
}

static int device_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "Device opened\n");
    return 0;
}

static int device_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "Device closed\n");
    return 0;
}

static ssize_t device_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    size_t to_copy = buffer_size - *offset;
    if (to_copy > len)
        to_copy = len;

    if (to_copy == 0)
        return 0;

    if (copy_to_user(buffer, kernel_buffer + *offset, to_copy))
        return -EFAULT;

    *offset += to_copy;
    return to_copy;
}

static ssize_t device_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    char user_input[4];
    int val;
    char led1_state, led2_state;

    if (len < 3 || len > 4) // Expecting "1 1" or "0 0" (with optional newline)
        return -EINVAL;

    if (copy_from_user(user_input, buffer, len))
        return -EFAULT;

    // Parse input (expecting "X Y" where X and Y are '0' or '1')
    if (user_input[1] != ' ' || (user_input[0] != '0' && user_input[0] != '1') ||
        (user_input[2] != '0' && user_input[2] != '1'))
        return -EINVAL;

    led1_state = user_input[0];
    led2_state = user_input[2];

    val = ioread32(gpio_base + GPIO_DATAOUT);

    // Control LED1
    if (led1_state == '1') {
        val |= (1 << GPIO_BIT1); // Turn LED1 ON
        printk(KERN_INFO "LED1 ON\n");
    } else {
        val &= ~(1 << GPIO_BIT1); // Turn LED1 OFF
        printk(KERN_INFO "LED1 OFF\n");
    }

    // Control LED2
    if (led2_state == '1') {
        led2_blinking = true; // Start blinking
        printk(KERN_INFO "LED2 BLINKING\n");
    } else {
        led2_blinking = false;
        val &= ~(1 << GPIO_BIT2); // Turn LED2 OFF
        printk(KERN_INFO "LED2 OFF\n");
    }

    iowrite32(val, gpio_base + GPIO_DATAOUT);

    // Update kernel_buffer
    snprintf(kernel_buffer, BUFFER_SIZE, "LED1: %s, LED2: %s",
             (led1_state == '1') ? "ON" : "OFF",
             (led2_state == '1') ? "BLINKING" : "OFF");
    buffer_size = strlen(kernel_buffer);

    return len;
}

static ssize_t led1_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    int val = ioread32(gpio_base + GPIO_DATAOUT);
    return sprintf(buf, "LED1 is %s\n", (val & (1 << GPIO_BIT1)) ? "ON" : "OFF");
}

static ssize_t led1_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int val;

    if (strncmp(buf, "1", 1) == 0) {
        val = ioread32(gpio_base + GPIO_DATAOUT);
        val |= (1 << GPIO_BIT1);
        iowrite32(val, gpio_base + GPIO_DATAOUT);
    } else if (strncmp(buf, "0", 1) == 0) {
        val = ioread32(gpio_base + GPIO_DATAOUT);
        val &= ~(1 << GPIO_BIT1);
        iowrite32(val, gpio_base + GPIO_DATAOUT);
    }

    return count;
}

static ssize_t led2_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
    return sprintf(buf, "LED2 is %s\n", led2_blinking ? "BLINKING" : "OFF");
}

static ssize_t led2_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
    int val;

    if (strncmp(buf, "1", 1) == 0) {
        led2_blinking = true;
    } else if (strncmp(buf, "0", 1) == 0) {
        led2_blinking = false;
        val = ioread32(gpio_base + GPIO_DATAOUT);
        val &= ~(1 << GPIO_BIT2);
        iowrite32(val, gpio_base + GPIO_DATAOUT);
    }

    return count;
}

module_init(device_init);
module_exit(device_exit);
