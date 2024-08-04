#include "constants.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ArduinoOTA.h>
#include <time.h>

#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "esp32/ulp.h"
#include "ulp_main.h"
#include "ulptool.h"

void SetupGPIOPins();
float GetVoltage();
void Restart();
void DeepSleep();
void SetupWifi();
void TurnOffOtaSwitch();
void SetupOta();
bool OtaSwitchState();
void RunOtaHandle(long timeout_ms);
void GetRealTime();
String CreatePayload(uint16_t ticks, long duration);
void SendPayload(String payload);
static void init_run_ulp(uint32_t usec);

void SetupGPIOPins()
{
    rtc_gpio_init(kGeigerCounterInterruptPin);
    rtc_gpio_set_direction(kGeigerCounterInterruptPin, RTC_GPIO_MODE_INPUT_ONLY);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(kSolarPanelAdcChannel, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(kSolarPanelBoostedAdcChannel, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(kBatteryAdcChannel, ADC_ATTEN_DB_11);
}

float GetVoltage(adc1_channel_t channel)
{
    int adc = 0;

    for (int i = 0; i < 1000; i++)
    {
        adc += adc1_get_raw(channel);
    }
    adc *= 0.001;

    float voltage = map(adc, 1330, 2065, 2420, 3680) / 1000.0 - 0.14; // 2.42V=1330; 3.68V=2065

    return voltage;
}

void Restart()
{
    Serial.println("Restart");
    ESP.restart();
}

void DeepSleep(unsigned long wakeup_time_ms)
{
    Serial.println("Entering Deep Sleep");
    esp_sleep_enable_timer_wakeup(1ull * wakeup_time_ms * 1000);
    esp_deep_sleep_start();
}

WiFiManager wifiManager;
void SetupWifi()
{
    // Set Static IP address
    IPAddress ip_address;
    ip_address.fromString(kIpAddress);

    // Set Gateway IP address
    IPAddress gateway(192, 168, 10, 1);
    IPAddress subnet(255, 255, 0, 0);
    IPAddress primaryDNS(8, 8, 8, 8);   // optional
    IPAddress secondaryDNS(8, 8, 4, 4); // optional

    // Configures static IP address
    if (!WiFi.config(ip_address, gateway, subnet, primaryDNS, secondaryDNS))
    {
        Serial.println("STA Failed to configure");
    }

    wifiManager.setConfigPortalTimeout(60);
    wifiManager.setConfigPortalTimeoutCallback(Restart);
    wifiManager.setConnectTimeout(5);
    if (wifiManager.getWiFiSSID() == "")
    {
        Serial.println("No wifi saved, enable config portal");
        wifiManager.setEnableConfigPortal(true);
    }
    else
    {
        Serial.print("Saved WiFi SSID: ");
        Serial.println(wifiManager.getWiFiSSID());
        Serial.println("Disable config portal");
        wifiManager.setEnableConfigPortal(false);
    }
    wifiManager.autoConnect("ESP32_GEIGER_COUNTER", "password");
    if (!WiFi.isConnected())
    {
        Serial.println("Wifi Connection Failed");
    }
}

void TurnOffOtaSwitch()
{
    HTTPClient http;
    String endpoint = "/changeotaswitch?state=False";
    String url = String(kServerName) + endpoint;

    http.begin(url);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0)
    {
        Serial.print("Turn Off OTA");
    }
    else
    {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpResponseCode);
    }

    http.end(); // Free resources
}

void SetupOta()
{
    ArduinoOTA
        .onStart([]()
                 {
      TurnOffOtaSwitch();
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type); })
        .onEnd([]()
               { Serial.println("\nEnd"); })
        .onProgress([](unsigned int progress, unsigned int total)
                    { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); })
        .onError([](ota_error_t error)
                 {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed"); });

    ArduinoOTA.begin();
}

bool OtaSwitchState()
{
    HTTPClient http;
    String endpoint = "/otaswitchstate";
    String url = String(kServerName) + endpoint;
    bool otaSwitchState = false;

    http.begin(url);                   // Specify the URL
    int httpResponseCode = http.GET(); // Make the GET request

    if (httpResponseCode > 0)
    {
        // Check for a valid response
        String payload = http.getString();
        if (payload == "True" || payload == "true")
        {
            otaSwitchState = true;
        }
        else if (payload == "False" || payload == "false")
        {
            otaSwitchState = false;
        }
    }
    else
    {
        Serial.print("Error on HTTP request: ");
        Serial.println(httpResponseCode);
    }

    http.end(); // Free resources
    return otaSwitchState;
}

void RunOtaHandle(long timeout_ms = 30000)
{
    Serial.println("Run OTA Handle");
    long start_time = millis();
    while (millis() - start_time <= timeout_ms)
    {
        ArduinoOTA.handle();
    }
}

struct tm current_timeinfo;
void GetRealTime()
{
    // Initialize NTP
    configTzTime(tz_info, ntpServer);

    if (!getLocalTime(&current_timeinfo))
    {
        Serial.println("Failed to obtain time");
        return;
    }

    Serial.print("Current time: " + String(asctime(&current_timeinfo)));
}

String CreatePayload(uint16_t ticks, long duration)
{
    float solar_panel_voltage = GetVoltage(kSolarPanelAdcChannel);
    float solar_panel_boosted_voltage = GetVoltage(kSolarPanelBoostedAdcChannel);
    float battery_voltage = GetVoltage(kBatteryAdcChannel);

    // Create JSON payload
    String payload_status = "\"status\": {";
    payload_status += "\"solar_panel_voltage\": " + String(solar_panel_voltage) + ",";
    payload_status += "\"solar_panel_boosted_voltage\": " + String(solar_panel_boosted_voltage) + ",";
    payload_status += "\"battery_voltage\": " + String(battery_voltage);
    payload_status += "}";

    String payload_data = "\"data\": {";
    payload_data += "\"year\": " + String(current_timeinfo.tm_year + 1900) + ",";
    payload_data += "\"month\": " + String(current_timeinfo.tm_mon + 1) + ",";
    payload_data += "\"day\": " + String(current_timeinfo.tm_mday) + ",";
    payload_data += "\"hour\": " + String(current_timeinfo.tm_hour) + ",";
    payload_data += "\"minute\": " + String(current_timeinfo.tm_min) + ",";
    payload_data += "\"second\": " + String(current_timeinfo.tm_sec) + ",";
    payload_data += "\"ticks\": " + String(ticks) + ",";
    payload_data += "\"duration\": " + String(duration);
    payload_data += "}";

    String payload = "{" + payload_status + ", " + payload_data + "}";
    Serial.println(payload);

    return payload;
}

void SendPayload(String payload)
{
    // Configure HTTP client
    HTTPClient http;
    String endpoint = "/savedata";
    String url = String(kServerName) + endpoint;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-api-key", kApiKey);

    // Send POST request
    int httpResponseCode = http.POST(payload);

    // Print response
    if (httpResponseCode > 0)
    {
        String response = http.getString();
        Serial.println("Status Code: " + String(httpResponseCode));
        Serial.println("Response Text: " + response);
    }
    else
    {
        Serial.println("Error on sending POST: " + String(httpResponseCode));
    }

    http.end();
}

RTC_DATA_ATTR uint16_t previous_tick_count = 0;
RTC_DATA_ATTR bool first_run_complete = false;
RTC_DATA_ATTR bool ulp_initialized = false;

void setup()
{
    unsigned long start_time = millis();
    Serial.begin(115200);

    SetupGPIOPins();

    getLocalTime(&current_timeinfo);
    if (!ulp_initialized)
    {
        init_run_ulp(1); // parameter irrelevant, ULP runs infinite loop
        ulp_initialized = true;
    }

    // ulp variables are 32-bit but only the bottom 16-bits hold data
    uint16_t current_tick_count = ulp_tick_count & 0xFFFF;
    uint16_t tick_count_difference;
    if (current_tick_count >= previous_tick_count)
        tick_count_difference = current_tick_count - previous_tick_count;
    else
        tick_count_difference = (0xFFFF - previous_tick_count + 1) + current_tick_count;

    long duration = kUpdateFrequencySeconds * 1000 + millis();

    Serial.printf("Previous: %i Current: %i \n", previous_tick_count, current_tick_count);

    SetupWifi();

    GetRealTime();

    if (first_run_complete)
    {
        String payload = CreatePayload(tick_count_difference, duration);
        SendPayload(payload);
    }

    previous_tick_count = current_tick_count;

    if (OtaSwitchState())
    {
        SetupOta();
        RunOtaHandle();
    }

    wifiManager.disconnect();

    first_run_complete = true;

    unsigned long end_time = millis();

    DeepSleep(kUpdateFrequencySeconds * 1000 - (end_time - start_time));
}

void loop() {}

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[] asm("_binary_ulp_main_bin_end");

static void init_run_ulp(uint32_t usec)
{
    ulp_set_wakeup_period(0, usec);
    esp_err_t err = ulptool_load_binary(
        0, ulp_main_bin_start,
        (ulp_main_bin_end - ulp_main_bin_start) / sizeof(uint32_t));
    err = ulp_run((&ulp_entry - RTC_SLOW_MEM) / sizeof(uint32_t));

    if (err)
        Serial.println("Error Starting ULP Coprocessor");
}