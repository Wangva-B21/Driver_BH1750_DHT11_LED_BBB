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
#include <linux/time.h>

#define DEVICE_NAME "bh1750"
#define CLASS_NAME  "bh1750_class"

#define GPIO1_BASE  0x4804C000
#define GPIO_SIZE   0x1000
#define GPIO_OE     0x134
#define GPIO_DATAIN 0x138
#define GPIO_DATAOUT 0x13C

#define SDA_GPIO 60 // GPIO1_28 = P9.12
#define SCL_GPIO 49 // GPIO1_17 = P9.23

#define BH1750_ADDR 0x23
#define BH1750_CMD  0x10  // Continuously H-Resolution Mode

static void __iomem *gpio1_base;
static struct class *bh1750_class;
static struct cdev bh1750_cdev;
static dev_t dev_num;

// Dữ liệu cảm biến được lưu trữ và trả về khi đọc
static uint16_t bh1750_data = 0;
static DEFINE_MUTEX(bh1750_data_mutex);  // Mutex bảo vệ dữ liệu cảm biến

// Thread nền để đọc cảm biến mỗi 3 giây
static struct task_struct *bh1750_thread;

// ===== GPIO Bit-Banding =====
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
    if (value)
        writel(readl(gpio1_base + GPIO_DATAOUT) | (1 << (gpio % 32)), gpio1_base + GPIO_DATAOUT);
    else
        writel(readl(gpio1_base + GPIO_DATAOUT) & ~(1 << (gpio % 32)), gpio1_base + GPIO_DATAOUT);
}

static inline int gpio_read(int gpio)
{
    return (readl(gpio1_base + GPIO_DATAIN) >> (gpio % 32)) & 1;
}

// ===== Bit-Banging I2C =====
static void i2c_delay(void) { udelay(5); }

static void i2c_start(void)
{
    gpio_set_output(SDA_GPIO);
    gpio_write(SDA_GPIO, 1);
    gpio_write(SCL_GPIO, 1);
    i2c_delay();
    gpio_write(SDA_GPIO, 0);
    i2c_delay();
    gpio_write(SCL_GPIO, 0);
}

static void i2c_stop(void)
{
    gpio_set_output(SDA_GPIO);
    gpio_write(SDA_GPIO, 0);
    gpio_write(SCL_GPIO, 1);
    i2c_delay();
    gpio_write(SDA_GPIO, 1);
    i2c_delay();
}

static int i2c_write_byte(uint8_t data)
{
    int i;
    int ack;  // Moved declaration to the top of the function

    for (i = 0; i < 8; i++) {
        gpio_write(SDA_GPIO, (data & 0x80) ? 1 : 0);
        i2c_delay();
        gpio_write(SCL_GPIO, 1);
        i2c_delay();
        gpio_write(SCL_GPIO, 0);
        data <<= 1;
    }

    gpio_set_input(SDA_GPIO);
    gpio_write(SCL_GPIO, 1);
    i2c_delay();
    ack = gpio_read(SDA_GPIO);  // Now 'ack' is declared
    gpio_write(SCL_GPIO, 0);
    gpio_set_output(SDA_GPIO);
    return (ack == 0);
}

static uint8_t i2c_read_byte(int ack)
{
    int i;
    uint8_t data = 0;
    gpio_set_input(SDA_GPIO);

    for (i = 0; i < 8; i++) {
        gpio_write(SCL_GPIO, 1);
        i2c_delay();
        data = (data << 1) | gpio_read(SDA_GPIO);
        gpio_write(SCL_GPIO, 0);
        i2c_delay();
    }

    gpio_set_output(SDA_GPIO);
    gpio_write(SDA_GPIO, ack ? 0 : 1);
    gpio_write(SCL_GPIO, 1);
    i2c_delay();
    gpio_write(SCL_GPIO, 0);
    gpio_write(SDA_GPIO, 1);

    return data;
}

// ===== Đọc dữ liệu từ cảm biến và lưu vào biến toàn cục =====
static void bh1750_read_data(void)
{
    uint8_t msb, lsb;
    uint16_t value;

    i2c_start();
    if (!i2c_write_byte(BH1750_ADDR << 1)) return;
    if (!i2c_write_byte(BH1750_CMD)) return;
    i2c_stop();

    msleep(180);  // Thời gian chờ để cảm biến hoàn tất đo

    i2c_start();
    if (!i2c_write_byte((BH1750_ADDR << 1) | 1)) return;
    msb = i2c_read_byte(1);
    lsb = i2c_read_byte(0);
    i2c_stop();

    value = (msb << 8) | lsb;
    
    // Lưu giá trị đo được vào biến toàn cục bh1750_data
    mutex_lock(&bh1750_data_mutex);
    bh1750_data = value * 5 / 6;  // Chuyển đổi sang đơn vị lux (nếu cần)
    mutex_unlock(&bh1750_data_mutex);
}

// ===== Thread nền để đọc dữ liệu cảm biến mỗi 3 giây =====
static int bh1750_thread_fn(void *data)
{
    while (!kthread_should_stop()) {
        bh1750_read_data();
        msleep(3000);  // Đọc lại mỗi 3 giây
    }
    return 0;
}

// ===== Driver: đọc từ /dev/bh1750 =====
static ssize_t bh1750_read(struct file *f, char __user *buf, size_t count, loff_t *offset)
{
    char str[16];
    int len;

    // Đọc dữ liệu cảm biến từ biến bh1750_data
    mutex_lock(&bh1750_data_mutex);
    len = snprintf(str, sizeof(str), "%u\n", bh1750_data);
    mutex_unlock(&bh1750_data_mutex);

    return simple_read_from_buffer(buf, count, offset, str, len);
}

static const struct file_operations bh1750_fops = {
    .owner = THIS_MODULE,
    .read  = bh1750_read,
};

// ===== Module init/exit =====
int __init bh1750_init(void)
{
    int ret;

    gpio1_base = ioremap(GPIO1_BASE, GPIO_SIZE);
    if (!gpio1_base) return -ENOMEM;

    if (gpio_request(SDA_GPIO, "SDA") || gpio_request(SCL_GPIO, "SCL"))
        return -EBUSY;

    gpio_set_output(SDA_GPIO);
    gpio_set_output(SCL_GPIO);
    gpio_write(SDA_GPIO, 1);
    gpio_write(SCL_GPIO, 1);

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret) return ret;

    cdev_init(&bh1750_cdev, &bh1750_fops);
    cdev_add(&bh1750_cdev, dev_num, 1);

    bh1750_class = class_create(THIS_MODULE, CLASS_NAME);
    device_create(bh1750_class, NULL, dev_num, NULL, DEVICE_NAME);

    // Tạo và bắt đầu thread đọc dữ liệu cảm biến
    bh1750_thread = kthread_run(bh1750_thread_fn, NULL, "bh1750_thread");
    if (IS_ERR(bh1750_thread)) {
        pr_err("Failed to create bh1750 thread\n");
        return PTR_ERR(bh1750_thread);
    }

    pr_info("BH1750 driver with bit-banging I2C and ioremap loaded\n");
    return 0;
}

void __exit bh1750_exit(void)
{
    kthread_stop(bh1750_thread);

    device_destroy(bh1750_class, dev_num);
    class_destroy(bh1750_class);
    cdev_del(&bh1750_cdev);
    unregister_chrdev_region(dev_num, 1);

    gpio_free(SDA_GPIO);
    gpio_free(SCL_GPIO);
    iounmap(gpio1_base);

    pr_info("BH1750 driver removed\n");
}

module_init(bh1750_init);
module_exit(bh1750_exit);
MODULE_LICENSE("GPL");

