#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <mosquitto.h>

#define DEVICE_PATH "/dev/multi_led"
#define MQTT_HOST "192.168.7.1"
#define MQTT_PORT 1883
#define TOPIC "bbb/led"

void on_message(struct mosquitto *mosq, void *userdata, const struct mosquitto_message *message) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open device");
        return;
    }

    if (message->payloadlen >= 3) {
        char *payload = (char *)message->payload;
        if ((payload[0] == '0' || payload[0] == '1') &&
            payload[1] == ' ' &&
            (payload[2] == '0' || payload[2] == '1')) {
            write(fd, payload, 3);
            printf("MQTT: LED1 set to %c, LED2 set to %c\n", payload[0], payload[2]);
        }
    }

    close(fd);
}

int main() {
    struct mosquitto *mosq;

    mosquitto_lib_init();

    mosq = mosquitto_new(NULL, true, NULL);
    if (!mosq) {
        fprintf(stderr, "Failed to create Mosquitto instance\n");
        return EXIT_FAILURE;
    }

    // Connect to the MQTT broker in a loop until successful
    while (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Unable to connect to MQTT broker, retrying in 3 seconds...\n");
        sleep(3);  // Wait for 3 seconds before retrying
    }

    printf("Connected to MQTT broker at %s:%d\n", MQTT_HOST, MQTT_PORT);

    mosquitto_message_callback_set(mosq, on_message);

    if (mosquitto_subscribe(mosq, NULL, TOPIC, 0) != MOSQ_ERR_SUCCESS) {
        fprintf(stderr, "Failed to subscribe to topic %s\n", TOPIC);
        mosquitto_destroy(mosq);
        mosquitto_lib_cleanup();
        return EXIT_FAILURE;
    }

    printf("Subscribed to MQTT topic '%s'. Waiting for messages...\n", TOPIC);

    // Loop forever to handle MQTT communication
    while (1) {
        int rc = mosquitto_loop(mosq, -1, 1);  // Non-blocking loop to handle message and reconnect if needed
        if (rc != MOSQ_ERR_SUCCESS) {
            fprintf(stderr, "Connection lost, retrying in 3 seconds...\n");
            sleep(3);  // Wait before retrying the connection
            while (mosquitto_connect(mosq, MQTT_HOST, MQTT_PORT, 60) != MOSQ_ERR_SUCCESS) {
                fprintf(stderr, "Unable to reconnect to MQTT broker, retrying in 3 seconds...\n");
                sleep(3);  // Wait for 3 seconds before retrying
            }
            printf("Reconnected to MQTT broker\n");
            mosquitto_subscribe(mosq, NULL, TOPIC, 0);  // Re-subscribe to the topic after reconnection
        }
    }

    mosquitto_destroy(mosq);
    mosquitto_lib_cleanup();

    return EXIT_SUCCESS;
}

