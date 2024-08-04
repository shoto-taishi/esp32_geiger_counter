#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <Arduino.h>
#include <driver/adc.h>

// GPIO Pins
const gpio_num_t kGeigerCounterInterruptPin = GPIO_NUM_34;
const adc1_channel_t kSolarPanelAdcChannel = ADC1_CHANNEL_4;        // GPIO32
const adc1_channel_t kSolarPanelBoostedAdcChannel = ADC1_CHANNEL_5; // GPIO33
const adc1_channel_t kBatteryAdcChannel = ADC1_CHANNEL_7;           // GPIO35

// Server details
const char *kServerName = "http://192.168.10.130:8000"; // Server IP and endpoint
const char *kApiKey = "your_api_key_here";              // API key
const String kIpAddress = "192.168.10.155";             // IP Address of ESP32

// Update frequency
const int kUpdateFrequencySeconds = 60 * 1;

// NTP Server and Timezone settings
const char *ntpServer = "pool.ntp.org";
const char *tz_info = "JST-9"; // Asia/Tokyo

#endif // CONSTANTS_H
