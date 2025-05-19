SENSOR_DRIVER_SITE = $(TOPDIR)/package/sensor_driver
SENSOR_DRIVER_SITE_METHOD = local

CROSS_COMPILE = $(HOST_DIR)/bin/arm-buildroot-linux-gnueabihf-

LINUX_DIR = $(TOPDIR)/output/build/linux-custom
KERNEL_VERSION = 6.1.46

define SENSOR_DRIVER_BUILD_CMDS
	$(MAKE) -C $(LINUX_DIR) M=$(SENSOR_DRIVER_SITE) \
		ARCH=arm \
		CROSS_COMPILE=$(CROSS_COMPILE) modules
endef

define SENSOR_DRIVER_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/lib/modules/$(KERNEL_VERSION)/kernel/drivers/sensor/
	install -m 0644 $(SENSOR_DRIVER_SITE)/bh1750_driver.ko $(TARGET_DIR)/lib/modules/$(KERNEL_VERSION)/kernel/drivers/sensor/
	install -m 0644 $(SENSOR_DRIVER_SITE)/dht11_driver.ko $(TARGET_DIR)/lib/modules/$(KERNEL_VERSION)/kernel/drivers/sensor/
	install -m 0644 $(SENSOR_DRIVER_SITE)/led_driver.ko $(TARGET_DIR)/lib/modules/$(KERNEL_VERSION)/kernel/drivers/sensor/
endef

$(eval $(generic-package))

