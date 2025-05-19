#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <mosquitto.h>

#define DEVICE "/dev/bh1750"
#define MQTT_HOST "192.168.7.1"  // IP của PC - THAY ĐỔI CHO PHÙ HỢP
#define MQTT_PORT 1883
#define MQTT_TOPIC "light/data"

void read_light_data_and_publish(struct mosquitto *mosq) {
    int fd;
    char buffer[16];
    ssize_t bytes_read;

    fd = open(DEVICE, O_RDONLY);
    if (fd == -1) {
        perror("Failed to open the device");
        return;
    }

    bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read == -1) {
        perror("Failed to read data from the device");
        close(fd);
        return;
    }

    buffer[bytes_read] = '\0';
    buffer[strcspn(buffer, "\n")] = '\0';

    printf("Current Light Data from BH1750: %s lux\n", buffer);

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
        printf("Starting continuous light reading with MQTT publishing...\n");
        while (1) {
            read_light_data_and_publish(mosq);
            sleep(3);
        }
    } else if (argc == 2 && strcmp(argv[1], "read") == 0) {
        read_light_data_and_publish(mosq);
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

