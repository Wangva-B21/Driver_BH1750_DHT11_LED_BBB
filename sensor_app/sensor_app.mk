################################################################################
# Định nghĩa tên gói
SENSOR_APP_VERSION = 1.0

# Đường dẫn chứa file nguồn
SENSOR_APP_SITE = $(TOPDIR)/package/sensor_app
SENSOR_APP_SITE_METHOD = local

# Lệnh biên dịch - Tạo 3 file thực thi
define SENSOR_APP_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(TARGET_DIR)/usr/bin/app_bh1750 $(SENSOR_APP_SITE)/app_bh1750.c -lm -lmosquitto -lssl -lcrypto
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(TARGET_DIR)/usr/bin/app_dht11 $(SENSOR_APP_SITE)/app_dht11.c -lm -lmosquitto -lssl -lcrypto
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(TARGET_DIR)/usr/bin/app_led $(SENSOR_APP_SITE)/app_led.c -lm -lmosquitto -lssl -lcrypto
endef

# Lệnh cài đặt - Đảm bảo quyền thực thi
define SENSOR_APP_INSTALL_TARGET_CMDS
	chmod +x $(TARGET_DIR)/usr/bin/app_bh1750
	chmod +x $(TARGET_DIR)/usr/bin/app_dht11
	chmod +x $(TARGET_DIR)/usr/bin/app_led
endef

# Định nghĩa gói
$(eval $(generic-package))
