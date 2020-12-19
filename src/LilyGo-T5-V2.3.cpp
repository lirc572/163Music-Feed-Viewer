#define HTTP_HOST "163music.avosapps.us"
#define HTTP_PATH "/1.1/functions/event"
#define HTTP_ADDR "http://" HTTP_HOST HTTP_PATH
#define HTTP_DATA_F "{\"uid\":\""
#define HTTP_DATA_B "\"}"
#define HTTP_PORT 80

#include "definitions.h" //defines WIFI_SSID, WIFI_PASSWORD and NEUID :P

/**
 * It is necessary to include these two headers to access LeanCloud Cloud function through REST API.
 **/
#define LC_ID "171P7IxHeNStt1LHc4dT4f90-MdYXbMMI" //LeanCloud Application ID
#define LC_KEY "uDYWXqmSCYAPOoe9wHf1X9bL"         //LeanCloud Application Key

// include library, include base class, make path known
#include <Arduino.h>
#include <GxEPD.h>
#include "SD.h"
#include "SPI.h"
#include "esp_adc_cal.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <cstdio>
#include <cstdlib>

#define uS_TO_S_FACTOR 1000000ull

#include <GxGDEH0213B73/GxGDEH0213B73.h> // 2.13" b/w newer panel

// FreeFonts from Adafruit_GFX
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/Org_01.h>
#include <Fonts/TomThumb.h>

#include <GxIO/GxIO_SPI/GxIO_SPI.h>
#include <GxIO/GxIO.h>

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18

#define ELINK_SS 5
#define ELINK_BUSY 4
#define ELINK_RESET 16
#define ELINK_DC 17

#define SDCARD_SS 13
#define SDCARD_CLK 14
#define SDCARD_MOSI 15
#define SDCARD_MISO 2

#define BUTTON_PIN 39

#define BATT_ADC_PIN 35

GxIO_Class io(SPI, /*CS=5*/ ELINK_SS, /*DC=*/ELINK_DC, /*RST=*/ELINK_RESET);
GxEPD_Class display(io, /*RST=*/ELINK_RESET, /*BUSY=*/ELINK_BUSY);

SPIClass sdSPI(VSPI);

WiFiMulti wifiMulti;
HTTPClient http;

const char *skuNum = "SKU:H239";
bool sdOK = false;
int startX = 40, startY = 10;

int vref = 1100;

void setupADC()
{
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize((adc_unit_t)ADC_UNIT_1, (adc_atten_t)ADC1_CHANNEL_6, (adc_bits_width_t)ADC_WIDTH_BIT_12, 1100, &adc_chars);
    //Check type of calibration value used to characterize ADC
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF)
    {
        //Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
        vref = adc_chars.vref;
    }
    else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
    {
        //Serial.printf("Two Point --> coeff_a:%umV coeff_b:%umV\n", adc_chars.coeff_a, adc_chars.coeff_b);
    }
    else
    {
        //Serial.println("Default Vref: 1100mV");
    }
}

String getVoltage()
{
    uint16_t v = analogRead(BATT_ADC_PIN);
    float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
    return String(battery_voltage) + "V";
}

void setupWifi()
{
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
    while (wifiMulti.run() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println();
    Serial.println("Wifi connected!");
}

String readFile(fs::FS &fs, const char *path)
{
    Serial.printf("Reading file: %s\n", path);

    File file = fs.open(path);
    if (!file)
    {
        Serial.println("Failed to open file for reading");
        return String();
    }
    String str("");
    Serial.print("Read from file: ");
    while (file.available())
    {
        //Serial.write(file.read());
        //str.append(std::string(1, file.read()));
        str.concat(char(file.read()));
    }
    //Serial.println();
    file.close();
    return str;
}

const int capacity = JSON_ARRAY_SIZE(20) + JSON_OBJECT_SIZE(1) + JSON_OBJECT_SIZE(2) + 10 * JSON_OBJECT_SIZE(5) + 1600;

const long gmtOffset_sec = 28800;
const char *ntpServer = "asia.pool.ntp.org";

void manageJson(String json)
{
    DynamicJsonDocument doc(capacity);
    DeserializationError err = deserializeJson(doc, json.c_str());
    if (err)
    {
        display.setCursor(20, display.height() / 2 - 10);
        display.setFont(&FreeSerif9pt7b);
        display.println("Json deserialization failed:");
        display.println(err.c_str());
    }
    else
    {
        display.setCursor(0, 6);
        //display.setFont(&FreeSerif9pt7b);
        for (std::uint8_t i = 0; i < doc["result"]["size"]; i++)
        {
            const char *song_name = doc["result"]["songs"][i]["name"];
            //const char* artist    = doc["result"]["songs"][i]["artist"];
            //const char* msg       = doc["result"]["songs"][i]["msg"];
            const char *datetime = doc["result"]["songs"][i]["datetime"];
            display.setFont(&FreeSerif9pt7b);
            display.print(song_name);
            display.setFont(&TomThumb);
            //display.printf(" -%s", artist);
            display.printf(" %s", datetime);
            //display.print(" ");
            //display.println(msg);
            display.setFont(&FreeSerif9pt7b);
            display.println();
        }
    }
}

void setup()
{
    Serial.begin(115200);
    Serial.println();
    Serial.println("setup");
    setupWifi();
    http.begin(HTTP_ADDR); //HTTP
    http.addHeader("Content-Type", "application/json");
    http.addHeader("X-LC-Id", LC_ID);
    http.addHeader("X-LC-Key", LC_KEY);
    //Serial.printf("Posting %s to %s\n", String(HTTP_DATA_F)+NEUID+HTTP_DATA_B, HTTP_ADDR);
    Serial.print("HTTP POST Response Status Code: ");
    int status_code = http.POST(String(HTTP_DATA_F) + NEUID + HTTP_DATA_B);
    Serial.println(status_code);
    if (status_code == -11)
    {
        Serial.println("Retrying...");
        status_code = http.POST(String(HTTP_DATA_F) + NEUID + HTTP_DATA_B);
        Serial.printf("HTTP POST Response Status Code: %d\n", status_code);
    }
    String payload = http.getString();

    setupADC();
    SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
    display.init(); // enable diagnostic output on Serial

    display.setRotation(1);
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);

    if (status_code > 399 && status_code < 600)
    {
        display.setCursor(20, display.height() / 2 - 10);
        display.setFont(&FreeSerif9pt7b);
        display.print("API call returns ");
        display.println(status_code);
    }
    else
    {
        //Serial.println(payload);
        manageJson(payload);
    }

    /*
    sdSPI.begin(SDCARD_CLK, SDCARD_MISO, SDCARD_MOSI, SDCARD_SS);

    if (!SD.begin(SDCARD_SS, sdSPI)) {
        sdOK = false;
    } else {
        sdOK = true;
    }

    if (sdOK) {
        //std::string ha = readFile(SD, "/event.json");
        //Serial.println(ha.c_str());
        //Serial.println(readFile(SD, "/event.json"));
        //Serial.println("hahaha");
        String json;
        json = readFile(SD, "/event.json");
        manageJson(json);
    }
    */

    /*
    display.setCursor(display.width() / 2, display.height() - 35);

    display.println(skuNum);

    display.setTextColor(GxEPD_BLACK);

    display.setCursor(display.width() / 2 - 40, display.height() - 10);

    if (sdOK) {
        uint32_t cardSize = SD.cardSize() / (1024 * 1024);
        display.println("SDCard:" + String(cardSize) + "MB");
    } else {
        display.println("SDCard  None");
    }

    display.setFont(&TomThumb);
    display.println("Sometimes I feel like the world is against me\nThe sound of your voice baby that's what saves me.");
    // Cnt be uglier
    */

    display.setCursor(display.width() - 21, display.height() - 4);
    display.setFont(&TomThumb);
    display.println(getVoltage());

    // Update local time
    configTime(gmtOffset_sec, 0, ntpServer);
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("Failed to obtain time");
    }
    else
    {
        Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");

        display.setCursor(display.width() - 52, display.height() / 2 + 10);
        display.printf("Updated: %02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
        char timeHour[3];
        strftime(timeHour,3, "%H", &timeinfo);
        int th = atoi(timeHour);
        if (th >= 0 && th <7) {
            esp_sleep_enable_timer_wakeup(7ull * 60ull * 60ull * uS_TO_S_FACTOR);
            esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);
            esp_deep_sleep_start();
        }
    }

    display.update();

    // goto sleep
    esp_sleep_enable_timer_wakeup(10 * 60 * uS_TO_S_FACTOR);
    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, LOW);

    esp_deep_sleep_start();
}

void loop()
{
}
