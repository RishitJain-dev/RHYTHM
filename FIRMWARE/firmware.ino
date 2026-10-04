#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <FastLED.h>

#define DATA_PIN     0        
#define MATRIX_WIDTH  24       
#define MATRIX_HEIGHT 7        
#define NUM_LEDS     (MATRIX_WIDTH * MATRIX_HEIGHT) 
#define COLOR_ORDER  GRB      
#define CHIPSET      WS2812B
#define BRIGHTNESS   40       

CRGB leds[NUM_LEDS];

const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

const char* HACKATIME_API_KEY = "your-api-key-here";

const char* API_URL = "https://hackatime.hackclub.com/api/v1/users/current/all_time_since_today";

const unsigned long FETCH_INTERVAL_MS = 10 * 60 * 1000;
unsigned long lastFetchTime = 0;

String encodeBase64(String input) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    String encoded = "";
    int i = 0, j = 0;
    unsigned char array3[3], array4[4];
    int input_len = input.length();
    int k = 0;

    while (input_len--) {
        array3[i++] = input[k++];
        if (i == 3) {
            array4[0] = (array3[0] & 0xfc) >> 2;
            array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
            array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
            array4[3] = array3[2] & 0x3f;
            for(i = 0; (i < 4) ; i++) encoded += base64_chars[array4[i]];
            i = 0;
        }
    }
    if (i) {
        for(j = i; j < 3; j++) array3[j] = '\0';
        array4[0] = (array3[0] & 0xfc) >> 2;
        array4[1] = ((array3[0] & 0x03) << 4) + ((array3[1] & 0xf0) >> 4);
        array4[2] = ((array3[1] & 0x0f) << 2) + ((array3[2] & 0xc0) >> 6);
        for (j = 0; (j < i + 1); j++) encoded += base64_chars[array4[j]];
        while((i++ < 3)) encoded += '=';
    }
    return encoded;
}

int getPixelIndex(int x, int y) {
    if (x < 0 || x >= MATRIX_WIDTH || y < 0 || y >= MATRIX_HEIGHT) return -1;
    return (y * MATRIX_WIDTH) + x;
}

void setBarProgress(float ratio) {
    FastLED.clear();
    int activeLeds = round(ratio * NUM_LEDS);
    
    for (int i = 0; i < NUM_LEDS; i++) {
        if (i < activeLeds) {
            uint8_t hue = map(i, 0, NUM_LEDS - 1, 96, 0); 
            leds[i] = CHSV(hue, 255, 255);
        } else {
            leds[i] = CRGB::Black;
        }
    }
    FastLED.show();
}

void fetchHackatimeData() {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi Disconnected. Reconnecting...");
        return;
    }

    Serial.println("Connecting to Hackatime API...");
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, API_URL);

    String authHeader = "Basic " + encodeBase64(String(HACKATIME_API_KEY));
    http.addHeader("Authorization", authHeader);

    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        
        StaticJsonDocument<1024> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
            const char* digital_time = doc["data"]["digital"];
            float total_seconds = doc["data"]["total_seconds"];
            
            Serial.print("Total Hours Logged: ");
            Serial.println(digital_time);

            float max_target_seconds = 8.0 * 3600.0;
            float ratio = constrain(total_seconds / max_target_seconds, 0.0, 1.0);
            
            setBarProgress(ratio);
        } else {
            Serial.print("JSON Parse Error: ");
            Serial.println(error.c_str());
        }
    } else {
        Serial.print("HTTP Request failed with code: ");
        Serial.println(httpCode);
    }

    http.end();
}

void setup() {
    Serial.begin(115200);

    FastLED.addLeds<CHIPSET, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(BRIGHTNESS);
    FastLED.clear(true);

    for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Blue;
        FastLED.show();
        delay(5);
    }
    delay(200);
    FastLED.clear(true);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected!");

    fetchHackatimeData();
    lastFetchTime = millis();
}

void loop() {
    if (millis() - lastFetchTime >= FETCH_INTERVAL_MS) {
        fetchHackatimeData();
        lastFetchTime = millis();
    }
}