/*
 * Copyright (c) 2020, CATIE
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mbed.h"
#include "bme280.h"
#include <nsapi_dns.h>
#include <MQTTClientMbedOs.h>
using namespace std::chrono;
using namespace sixtron;
namespace {
#define GROUP_NUMBER            "group15"
#define MQTT_TOPIC_PUBLISH      "mohask_/feeds/Pressure"
#define MQTT_TOPIC_PUBLISH_TEMP      "mohask_/feeds/temp-and-hum"
#define MQTT_TOPIC_PUBLISH_HUMIDITY      "mohask_/feeds/humdity"
#define MQTT_TOPIC_SUBSCRIBE    "mohask_/feeds/led"
#define SYNC_INTERVAL           1
#define MQTT_CLIENT_ID          "6LoWPAN_Node_"GROUP_NUMBERs
#define ADAFRUIT_IO_USERNAME = "mohask_"
#define ADAFRUIT_IO_KEY = "aio_ifeH37DNBXC3Z8ox2PWruLA0rLOB"
#define PERIOD_MS 1000ms
#define BME280_adresse 0x76 << 1
 
I2C bus(I2C1_SDA,I2C1_SCL);
DigitalOut led(LED1);
InterruptIn Bouton(BUTTON1);
Thread thread;
Timer timer;
bool appuie = false;
bool ledBool = false;
Mutex mutex_temp;
Mutex mutex_hum;
Mutex mutex_press;
Mutex mutex_print;
BME280 CapteurBME280(&bus, BME280::I2CAddress::Address1);
}
 
 
// Network
NetworkInterface *network;
MQTTClient *client;
 
// MQTT
// const char* hostname = "fd9f:590a:b158::1";
const char* hostname = "io.adafruit.com";
int port = 1883;
 
// Error code
nsapi_size_or_error_t rc = 0;
 
// Event queue
static int id_yield;
static EventQueue main_queue(32 * EVENTS_EVENT_SIZE);
 
/*!
 *  \brief Called when a message is received
 *
 *  Print messages received on mqtt topic
 */
void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
 
    // Get the payload string
    char* char_payload = (char*)malloc((message.payloadlen+1)*sizeof(char)); // allocate the necessary size for our buffer
    char_payload = (char *) message.payload; // get the arrived payload in our buffer
    char_payload[message.payloadlen] = '\0'; // String must be null terminated
 
    // Compare our payload with known command strings
    if (strcmp(char_payload, "ON") == 0) {
        led = 1;
    }
    else if (strcmp(char_payload, "OFF") == 0) {
        led = 0;
    }
    else if (strcmp(char_payload, "RESET") == 0) {
        printf("RESETTING ...\n");
        system_reset();
    }
}
 
void obtenirPression(){
   
    appuie = true;
}
 static int8_t publish1(float var) {
 
   
    char mqttPayload[12];
    sprintf(mqttPayload, "%.2f", var);
 
    MQTT::Message message;
    message.qos = MQTT::QOS1;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)mqttPayload;
    message.payloadlen = strlen(mqttPayload);
 
    printf("Send: %s to MQTT Broker: %s\n", mqttPayload, hostname);
    rc = client->publish(MQTT_TOPIC_PUBLISH_TEMP, message);
    if (rc != 0) {
        printf("Failed to publish: %d\n", rc);
        return rc;
    }
    return 0;
}

static int8_t publish2(float var) {
 
   
    char mqttPayload[12];
    sprintf(mqttPayload, "%.2f", var);
 
    MQTT::Message message;
    message.qos = MQTT::QOS1;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)mqttPayload;
    message.payloadlen = strlen(mqttPayload);
 
    printf("Send: %s to MQTT Broker: %s\n", mqttPayload, hostname);
    rc = client->publish(MQTT_TOPIC_PUBLISH_HUMIDITY, message);
    if (rc != 0) {
        printf("Failed to publish: %d\n", rc);
        return rc;
    }
    return 0;
}
void threadTemp(){
    
 
    while(true){
 
        timer.start();
       
        if(timer.elapsed_time().count() >= 15000000){
            mutex_temp.lock();
            float temperature = CapteurBME280.temperature();
            mutex_temp.unlock();
            mutex_temp.lock();
            float humidite = CapteurBME280.humidity();
            mutex_temp.unlock();
            publish1(temperature);
            publish2(humidite);
            timer.reset();
        }
    }
}
 
/*!
 *  \brief Yield to the MQTT client
 *
 *  On error, stop publishing and yielding
 */
static void yield(){
    // printf("Yield\n");
   
    rc = client->yield(100);
 
    if (rc != 0){
        printf("Yield error: %d\n", rc);
        main_queue.cancel(id_yield);
        main_queue.break_dispatch();
        system_reset();
    }
}
 
/*!
 *  \brief Publish data over the corresponding adafruit MQTT topic
 *
 */
static int8_t publish(float var) {
 
   
    char mqttPayload[12];
    sprintf(mqttPayload, "%.2f", var);
 
    MQTT::Message message;
    message.qos = MQTT::QOS1;
    message.retained = false;
    message.dup = false;
    message.payload = (void*)mqttPayload;
    message.payloadlen = strlen(mqttPayload);
 
    printf("Send: %s to MQTT Broker: %s\n", mqttPayload, hostname);
    rc = client->publish(MQTT_TOPIC_PUBLISH, message);
    if (rc != 0) {
        printf("Failed to publish: %d\n", rc);
        return rc;
    }
    return 0;
}

 
// main() runs in its own thread in the OS
// (note the calls to ThisThread::sleep_for below for delays)
 
int main()
{
 
    CapteurBME280.initialize();
    CapteurBME280.set_sampling();
 
    printf("Connecting to border router...\n");
 
    /* Get Network configuration */
    network = NetworkInterface::get_default_instance();
 
    if (!network) {
        printf("Error! No network interface found.\n");
        return 0;
    }
 
    /* Add DNS */
    nsapi_addr_t new_dns = {
        NSAPI_IPv6,
        { 0xfd, 0x9f, 0x59, 0x0a, 0xb1, 0x58, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x01 }
    };
    nsapi_dns_add_server(new_dns, "LOWPAN");
 
    /* Border Router connection */
    rc = network->connect();
    if (rc != 0) {
        printf("Error! net->connect() returned: %d\n", rc);
        return rc;
    }
 
    /* Print IP address */
    SocketAddress a;
    network->get_ip_address(&a);
    printf("IP address: %s\n", a.get_ip_address() ? a.get_ip_address() : "None");
 
    /* Open TCP Socket */
    TCPSocket socket;
    SocketAddress address;
    network->gethostbyname(hostname, &address);
    address.set_port(port);
 
    /* MQTT Connection */
    client = new MQTTClient(&socket);
    socket.open(network);
    rc = socket.connect(address);
    if(rc != 0){
        printf("Connection to MQTT broker Failed\n");
        return rc;
    }
 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 4;
    data.keepAliveInterval = 25;
    data.clientID.cstring = "moha";
    data.username.cstring = (char*) "mohask_" ;
    data.password.cstring = (char*) "aio_ifeH37DNBXC3Z8ox2PWruLA0rLOB";
    if (client->connect(data) != 0){
        printf("Connection to MQTT Broker Failed\n");
    }
 
    printf("Connected to MQTT broker\n");
 
    /* MQTT Subscribe */
    if ((rc = client->subscribe(MQTT_TOPIC_SUBSCRIBE, MQTT::QOS0, messageArrived)) != 0){
        printf("rc from MQTT subscribe is %d\r\n", rc);
    }
    printf("Subscribed to Topic: %s\n", MQTT_TOPIC_SUBSCRIBE);
 
 
    thread.start(threadTemp);
 
    yield();
 
    // Yield every 1 second
    id_yield = main_queue.call_every(SYNC_INTERVAL * 1000, yield);
 
    // Publish
    //Bouton.fall(main_queue.event(publish));
 
    //main_queue.dispatch_forever();
 
   
 
    while (true) {
       
        Bouton.rise(&obtenirPression);
       
        if(appuie){
            mutex_press.lock();
            float pression = CapteurBME280.pressure();
            mutex_press.unlock();
            publish(pression);
            appuie = false;
        }
 
        ThisThread::sleep_for(PERIOD_MS);
    }
}