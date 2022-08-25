/*******************************************************************
    Copyright (C) 2017 Triad Semiconductor

    ts4231_config_example.ino - Example application for configuring the Triad
              Semiconductor TS4231 Light to Digital converter.
    Created by: John Seibel
*******************************************************************/
#include <dummy.h>
#include "ts4231.h"

#define light_timeout 500  
typedef unsigned long ulong;
int sensor_cout;
int sensor_pins[32][2];
ulong sensor_last_rise[32];
ulong sensor_last_len[32];
ulong last_sync_time[32];
bool last_sync_axis[32];

uint8_t configSensor(int SID) {
    int device1_D_pin = sensor_pins[SID][0];
    int device1_E_pin = sensor_pins[SID][1];
    TS4231  device1(device1_E_pin, device1_D_pin);
    Serial.printf("Configuration %d\r\n", SID);
    if (device1.waitForLight(light_timeout)) {

        //Execute this code when light is detected
        Serial.println("Light DETECTED");

        uint8_t config_result = device1.configDevice();
        unsigned long rise_time_E = 0;
        int ePin = HIGH;
        //user can determine how to handle each return value for the configuration function
        switch (config_result) {
        case CONFIG_PASS:
            Serial.println("Configuration SUCCESS");
            break;
        case BUS_FAIL:  //unable to resolve state of TS4231 (3 samples of the bus signals resulted in 3 different states)
            Serial.println("Configuration Unsuccessful - BUS_FAIL");
            break;
        case VERIFY_FAIL:  //configuration read value did not match configuration write value, run configuration again
            Serial.println("Configuration Unsuccessful - VERIFY_FAIL");
            break;
        case WATCH_FAIL:  //verify succeeded but entry into WATCH mode failed, run configuration again
            Serial.println("Configuration Unsuccessful - WATCH_FAIL");
            break;
        default:  //value returned was unknown
            Serial.println("Program Execution ERROR");
            break;
        }
        return config_result;
    }
    else {
        //insert code here for no light detection
        Serial.println("Light TIMEOUT");
    }
    Serial.println("");
}

void setup() {
    Serial.begin(500000);
    while (!Serial);  //wait for serial port to connect

    Serial.println("Serial Port Connected");
    Serial.println();
    Serial.println();
    
    sensor_cout = 2;
    sensor_pins[0][0] = 18;
    sensor_pins[0][1] = 19;
    sensor_pins[1][0] = 22;
    sensor_pins[1][1] = 23;

    for (int i = 0; i < sensor_cout; ++i) {
        configSensor(i);
        attachInterruptArg(sensor_pins[i][1], changed, (void*)(i), CHANGE);
    }
    
    xTaskCreatePinnedToCore(CommunicateThread, "communicate", 4096, NULL, 0, NULL, 1);

}

void changed(void *pin_ptr) {
    auto sid = (int)pin_ptr;
    auto pin = sensor_pins[sid][1];
    if (digitalRead(pin) == LOW) {
        sensor_last_rise[sid] = micros();
    }
    else {
        sensor_last_len[sid] = micros() - sensor_last_rise[sid];
    }
}

void CommunicateThread(void *parameter) {
    for (;;) {
        for (int i = 0; i < sensor_cout; ++i) {
            if (sensor_last_len[i] != 0) {
                bool isSyncFlash = true;
                bool skip, data, axis;
                if (sensor_last_len[i] < 60)
                    isSyncFlash = false;
                else if (sensor_last_len[i] < 70) { //j0
                    skip = false; data = false; axis = false;
                }
                else if (sensor_last_len[i] < 80) { //k0
                    skip = false; data = false; axis = true;
                }
                else if (sensor_last_len[i] < 90) { //j1
                    skip = false; data = true; axis = false;
                }
                else if (sensor_last_len[i] < 100) { //k1
                    skip = false; data = true; axis = true;
                }
                else if (sensor_last_len[i] < 110) { //j2
                    skip = true; data = false; axis = false;
                }
                else if (sensor_last_len[i] < 120) { //k2
                    skip = true; data = false; axis = true;
                }
                else if (sensor_last_len[i] < 130) { //j3
                    skip = true; data = true; axis = false;
                }
                else if (sensor_last_len[i] < 140) { //k3
                    skip = true; data = true; axis = true;
                }
                if (isSyncFlash) {
                    last_sync_time[i] = sensor_last_rise[i];
                    last_sync_axis[i] = axis;
                    //Serial.printf("sync %u %d %d %s\r\n", sensor_last_rise[i], skip, data, axis ? "Y" : "X");
                }
                else {

                    Serial.printf("%d %d %d\r\n", i, sensor_last_rise[i] - last_sync_time[i], last_sync_axis[i]);
                }
                sensor_last_len[i] = 0;
            }
        }
    }
}



void loop() {
    vTaskDelete(NULL);
}