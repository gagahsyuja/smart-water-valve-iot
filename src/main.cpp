#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <BlynkSimpleEsp32.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <string.h>
#include <WiFiCredentials.h>
#include <BlynkCredentials.h>

const int trig_pin = 25;
const int echo_pin = 26;
const int relay_pin = 33;
const int one_wire_bus = 14;

const String URL = "http://192.168.217.101:3000/api/information";

// Sound speed in air
#define SOUND_SPEED 340
#define TRIG_PULSE_DURATION_US 10

long ultrason_duration;
float distance_cm;
bool manual_override = false;

void relay(bool);
float get_distance();
float get_percentage(float);
float get_temperature();
void post(int, int, String);

OneWire one_wire(one_wire_bus);
DallasTemperature sensors(&one_wire);

void setup()
{
    Serial.begin(9600);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    pinMode(trig_pin, OUTPUT); // We configure the trig as output
    pinMode(echo_pin, INPUT); // We configure the echo as input
    pinMode(relay_pin, OUTPUT);

    digitalWrite(relay_pin, HIGH);

    if (WiFi.isConnected())
        Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASSWORD, "blynk.cloud", 80);

    sensors.begin();
    sensors.setResolution(12);
}

BLYNK_CONNECTED()
{
    Blynk.syncVirtual(V0);
    Blynk.syncVirtual(V4);
}

BLYNK_WRITE(V0)
{
    if (manual_override)
    {
        if (param.asInt() == 0)
        {
            digitalWrite(relay_pin, HIGH);
        }

        else
        {
            digitalWrite(relay_pin, LOW);
        }
    }
}

BLYNK_WRITE(V4)
{
    manual_override = param.asInt() == 1;
}

void loop()
{
    delay(250);
    
    Serial.println("================================");
    // post(69, 69, "POSTED FROM ARDUINO COY");
    
    if (WiFi.isConnected())
        Blynk.run();
    else
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    float distance = get_distance();
    float temperature = get_temperature();
    float percentage = get_percentage(distance);

    if (distance < 10.0)
    {
        if (!manual_override)
            relay(true);
    }

    else
    {
        if (!manual_override)
            relay(false);
    }

    if (WiFi.isConnected())
        post(int(distance), int(percentage), String(temperature));

    delay(250);
}

void relay(bool is_close)
{
    if (is_close)
    {
        digitalWrite(relay_pin, HIGH);

        if (WiFi.isConnected())
            Blynk.virtualWrite(V0, 0);
        // Blynk.virtualWrite(V3, 0);

        Serial.println("Relay closed");
    }

    else
    {
        digitalWrite(relay_pin, LOW);

        if (WiFi.isConnected())
            Blynk.virtualWrite(V0, 1);
        // Blynk.virtualWrite(V3, 1);

        Serial.println("Relay opened");
    }
}

float get_distance()
{
    // Set up the signal
    digitalWrite(trig_pin, LOW);
    delay(2);
    // Create a 10 µs impulse
    digitalWrite(trig_pin, HIGH);
    delayMicroseconds(TRIG_PULSE_DURATION_US);
    digitalWrite(trig_pin, LOW);

    // Return the wave propagation time (in µs)
    ultrason_duration = pulseIn(echo_pin, HIGH);

    //distance calculation
    distance_cm = ultrason_duration * SOUND_SPEED/2 * 0.0001;

    // We print the distance on the serial port
    Serial.print("Distance (cm): ");
    Serial.println(distance_cm);

    if (WiFi.isConnected())
    {
        Blynk.virtualWrite(V1, distance_cm);
        Blynk.virtualWrite(V3, get_percentage(distance_cm));    
    }
    
    return distance_cm;
}

float get_percentage(float distance_cm)
{
    float percentage = (distance_cm - 10.0) / 90.0 * 100;
    percentage = (100 - percentage) < 0
        ? 0
        : (100 - percentage) > 100
            ? 100
            : 100 - percentage;

    Serial.print("Level is ");
    Serial.print(percentage);
    Serial.println("%");

    return percentage;
}

float get_temperature()
{
    sensors.requestTemperatures();

    float temp_celsius = sensors.getTempCByIndex(0);

    if (temp_celsius != DEVICE_DISCONNECTED_C)
    {
        Serial.print("Temps is ");
        Serial.print(temp_celsius);
        Serial.println(" celsius");    
        Blynk.virtualWrite(V2, temp_celsius);
        
        return temp_celsius;
    }
    
    else
    {
        Serial.println("Could not get temps");

        return -1.0;
    }
}

void post(int distance, int level, String temperature)
{
    HTTPClient http;
    StaticJsonDocument<256> root;
    String json;

    root["distance"] = distance;
    root["level"] = level;
    root["temperature"] = temperature;
    
    serializeJson(root, json);

    http.begin(URL);
    http.addHeader("Content-Type", "application/json");

    int status = http.POST(json);

    if (status == 200)
    {
        Serial.println("Post works!");
    }
    else
    {
        Serial.println("Nope!");
    }
}