################################################################################
# sensor_autostart package for Buildroot
################################################################################

SENSOR_AUTOSTART_VERSION = 1.0
SENSOR_AUTOSTART_SITE = $(TOPDIR)/package/sensor_autostart
SENSOR_AUTOSTART_SITE_METHOD = local

define SENSOR_AUTOSTART_INSTALL_TARGET_CMDS
	# Tạo thư mục init.d nếu chưa có
	mkdir -p $(TARGET_DIR)/etc/init.d/
	# Copy script vào thư mục init.d
	install -m 0755 $(SENSOR_AUTOSTART_SITE)/S99sensor $(TARGET_DIR)/etc/init.d/S99sensor
endef

$(eval $(generic-package))
