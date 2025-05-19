#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <mosquitto.h>

#define DEVICE_FILE "/dev/dht11"
#define MQTT_HOST "192.168.7.1"   // Đổi thành IP của máy PC bạn
#define MQTT_PORT 1883
#define MQTT_TOPIC "dht11/data"

void read_dht11_and_publish(struct mosquitto *mosq) {
    int fd;
    char buffer[64];
    ssize_t bytes_read;

    fd = open(DEVICE_FILE, O_RDONLY);
    if (fd == -1) {
        perror("Error opening device file");
        return;
    }

    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
        perror("Error reading from device");
        close(fd);
        return;
    }

    buffer[bytes_read] = '\0';
    buffer[strcspn(buffer, "\n")] = '\0'; // Xóa ký tự xuống dòng nếu có

    printf("DHT11 Data: %s\n", buffer);

    int ret = mosquitto_publish(mosq, NULL, MQTT_TOPIC, strlen(buffer), buffer, 0, false);
    if (ret != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to publish to MQTT: %s\n", mosquitto_strerror(ret));
    }

    close(fd);
}

int main(int argc, char *argv[]) {
    struct mosquitto *mosq;
    int ret;
    
    mosquitto_lib_init();

    // Tạo một client MQTT mới
    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create Mosquitto client\n");
        return 1;
    }

    // Tiếp tục kết nối lại nếu không thể kết nối đến broker
    while ((ret = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60)) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Unable to connect to MQTT broker, retrying in 5 seconds...\n");
        sleep(5); // Đợi 5 giây trước khi thử lại
    }
    printf("Connected to MQTT broker at %s:%d\n", MQTT_HOST, MQTT_PORT);

    if (argc == 2 && strcmp(argv[1], "start") == 0) {
        printf("Starting continuous DHT11 reading with MQTT publishing...\n");
        while (1) {
            read_dht11_and_publish(mosq);
            sleep(3);
        }
    } else if (argc == 2 && strcmp(argv[1], "read") == 0) {
        read_dht11_and_publish(mosq);
    } else {
        printf("Usage: %s <start|read>\n", argv[0]);
    }

    // Kiểm tra kết nối và tự động kết nối lại nếu mất kết nối
    while (1) {
        ret = mosquitto_loop(mosq, -1, 1);  // Loop này giúp duy trì kết nối và tái kết nối khi mất kết nối
        if (ret != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Connection lost, retrying...\n");
            sleep(5); // Đợi 5 giây trước khi thử lại
            while ((ret = mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60)) != MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "Unable to reconnect to MQTT broker, retrying in 5 seconds...\n");
                sleep(5);
            }
            printf("Reconnected to MQTT broker\n");
        }
    }

    mosquitto_disconnect(mosq);
    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();
    return 0;
}

