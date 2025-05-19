#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/mutex.h>

#define DEVICE_NAME "dht11"
#define CLASS_NAME  "dht11_class"

#define GPIO1_BASE     0x4804C000
#define GPIO_SIZE      0x1000
#define GPIO_OE        0x134
#define GPIO_DATAIN    0x138
#define GPIO_DATAOUT   0x13C

#define DHT11_GPIO     48// GPIO1_28 = P9.15

static void __iomem *gpio1_base;
static dev_t dev_num;
static struct class *dht11_class;
static struct cdev dht11_cdev;
static struct task_struct *dht11_thread;

static DEFINE_MUTEX(dht11_mutex);
static int humidity = 0, temperature = 0;

// Debug flag
static int debug = 0;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Enable debug output (0=off, 1=on)");

#define DHT11_DEBUG(fmt, ...) \
    do { if (debug) printk(KERN_INFO "DHT11: " fmt, ##__VA_ARGS__); } while (0)

// ===== GPIO control =====
static inline void gpio_set_output(int gpio)
{
    u32 oe = readl(gpio1_base + GPIO_OE);
    oe &= ~(1 << (gpio % 32));
    writel(oe, gpio1_base + GPIO_OE);
}

static inline void gpio_set_input(int gpio)
{
    u32 oe = readl(gpio1_base + GPIO_OE);
    oe |= (1 << (gpio % 32));
    writel(oe, gpio1_base + GPIO_OE);
}

static inline void gpio_write(int gpio, int value)
{
    u32 val = readl(gpio1_base + GPIO_DATAOUT);
    if (value)
        val |= (1 << (gpio % 32));
    else
        val &= ~(1 << (gpio % 32));
    writel(val, gpio1_base + GPIO_DATAOUT);
}

static inline int gpio_read(int gpio)
{
    return (readl(gpio1_base + GPIO_DATAIN) >> (gpio % 32)) & 1;
}

// ===== Đọc DHT11 =====
static int dht11_read_raw(int *humidity, int *temperature)
{
    uint8_t data[5] = {0};
    int i, j;

    DHT11_DEBUG("Starting read\n");
    gpio_set_output(DHT11_GPIO);
    gpio_write(DHT11_GPIO, 0);
    mdelay(20);
    gpio_write(DHT11_GPIO, 1);
    udelay(50);
    gpio_set_input(DHT11_GPIO);

    // Chờ DHT11 phản hồi
    for (i = 0; i < 100; i++) {
        if (!gpio_read(DHT11_GPIO)) break;
        udelay(1);
    }
    if (i == 100) {
        printk(KERN_ERR "DHT11: No response (low signal)\n");
        return -1;
    }

    for (i = 0; i < 100; i++) {
        if (gpio_read(DHT11_GPIO)) break;
        udelay(1);
    }
    if (i == 100) {
        printk(KERN_ERR "DHT11: No response (high signal)\n");
        return -1;
    }

    for (i = 0; i < 100; i++) {
        if (!gpio_read(DHT11_GPIO)) break;
        udelay(1);
    }
    if (i == 100) {
        printk(KERN_ERR "DHT11: No response (second low signal)\n");
        return -1;
    }

    // Đọc 40 bit dữ liệu
    for (j = 0; j < 40; j++) {
        while (!gpio_read(DHT11_GPIO));
        udelay(50);
        if (gpio_read(DHT11_GPIO))
            data[j / 8] |= (1 << (7 - (j % 8)));
        while (gpio_read(DHT11_GPIO));
    }

    DHT11_DEBUG("Raw: %d %d %d %d %d\n", data[0], data[1], data[2], data[3], data[4]);

    if (((data[0] + data[1] + data[2] + data[3]) & 0xFF) != data[4]) {
        printk(KERN_ERR "DHT11: Checksum error\n");
        return -2;
    }

    *humidity = data[0];
    *temperature = data[2];
    return 0;
}

static void dht11_read_loop(void)
{
    int h, t;
    DHT11_DEBUG("Entering read loop\n");
    int ret = dht11_read_raw(&h, &t);
    if (ret == 0) {
        mutex_lock(&dht11_mutex);
        humidity = h;
        temperature = t;
        mutex_unlock(&dht11_mutex);
        DHT11_DEBUG("Humidity=%d%%, Temperature=%d*C\n", h, t);
    } else {
        printk(KERN_ERR "DHT11: Read failed with error %d\n", ret);
    }
}

static int dht11_thread_fn(void *data)
{
    DHT11_DEBUG("Thread started\n");
    while (!kthread_should_stop()) {
        dht11_read_loop();
        msleep(5000); // Tăng lên 5 giây
    }
    return 0;
}

// ===== Giao tiếp với người dùng =====
static ssize_t dht11_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
    char buffer[32];
    int len;

    mutex_lock(&dht11_mutex);
    len = snprintf(buffer, sizeof(buffer), "Humidity: %d%%, Temp: %d*C\n", humidity, temperature);
    mutex_unlock(&dht11_mutex);

    return simple_read_from_buffer(buf, count, ppos, buffer, len);
}

static const struct file_operations dht11_fops = {
    .owner = THIS_MODULE,
    .read  = dht11_read,
};

// ===== Khởi tạo và hủy =====
static int __init dht11_init(void)
{
    int ret;

    gpio1_base = ioremap(GPIO1_BASE, GPIO_SIZE);
    if (!gpio1_base) {
        printk(KERN_ERR "DHT11: Failed to map GPIO1\n");
        return -ENOMEM;
    }
    DHT11_DEBUG("GPIO1 mapped at %p\n", gpio1_base);

    ret = gpio_request(DHT11_GPIO, "DHT11");
    if (ret < 0) {
        printk(KERN_ERR "DHT11: Failed to request GPIO %d: %d\n", DHT11_GPIO, ret);
        iounmap(gpio1_base);
        return ret;
    }
    DHT11_DEBUG("GPIO %d requested\n", DHT11_GPIO);

    gpio_set_output(DHT11_GPIO);
    gpio_write(DHT11_GPIO, 1);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0) {
        printk(KERN_ERR "DHT11: Failed to allocate chrdev: %d\n", ret);
        gpio_free(DHT11_GPIO);
        iounmap(gpio1_base);
        return ret;
    }

    cdev_init(&dht11_cdev, &dht11_fops);
    ret = cdev_add(&dht11_cdev, dev_num, 1);
    if (ret < 0) {
        printk(KERN_ERR "DHT11: Failed to add cdev: %d\n", ret);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(DHT11_GPIO);
        iounmap(gpio1_base);
        return ret;
    }

    dht11_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(dht11_class)) {
        ret = PTR_ERR(dht11_class);
        printk(KERN_ERR "DHT11: Failed to create class: %d\n", ret);
        cdev_del(&dht11_cdev);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(DHT11_GPIO);
        iounmap(gpio1_base);
        return ret;
    }

    device_create(dht11_class, NULL, dev_num, NULL, DEVICE_NAME);

    DHT11_DEBUG("Starting thread\n");
    dht11_thread = kthread_run(dht11_thread_fn, NULL, "dht11_thread");
    if (IS_ERR(dht11_thread)) {
        ret = PTR_ERR(dht11_thread);
        printk(KERN_ERR "DHT11: Failed to start thread: %d\n", ret);
        device_destroy(dht11_class, dev_num);
        class_destroy(dht11_class);
        cdev_del(&dht11_cdev);
        unregister_chrdev_region(dev_num, 1);
        gpio_free(DHT11_GPIO);
        iounmap(gpio1_base);
        return ret;
    }
    DHT11_DEBUG("Thread started successfully\n");

    pr_info("DHT11 driver loaded\n");
    return 0;
}

static void __exit dht11_exit(void)
{
    kthread_stop(dht11_thread);
    device_destroy(dht11_class, dev_num);
    class_destroy(dht11_class);
    cdev_del(&dht11_cdev);
    unregister_chrdev_region(dev_num, 1);

    gpio_free(DHT11_GPIO);
    iounmap(gpio1_base);

    pr_info("DHT11 driver removed\n");
}

module_init(dht11_init);
module_exit(dht11_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("DHT11 Kernel Driver using bit-banging on GPIO_60");
