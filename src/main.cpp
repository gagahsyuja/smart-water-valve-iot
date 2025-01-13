#include <Arduino.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <string.h>

const int trig_pin = 16;
const int echo_pin = 17;
const int relay_pin = 27;
const int one_wire_bus = 21;

WiFiManager wifiManager;

const String URL = "https://doscom.org/api/information";

// Sound speed in air
#define SOUND_SPEED 340
#define TRIG_PULSE_DURATION_US 10

long ultrason_duration;
float distance_cm;
bool manual_override = false;
float height = 100.0;
float threshold = 10.0;
bool relay = false;
JsonObject information;

void set_relay_status(bool);
float get_distance();
float get_percentage(float);
float get_temperature();
void post(int, int, String, bool, bool);
bool get_manual_status();
JsonObject get_information();

OneWire one_wire(one_wire_bus);
DallasTemperature sensors(&one_wire);

void setup()
{
    Serial.begin(9600);

    pinMode(trig_pin, OUTPUT); // We configure the trig as output
    pinMode(echo_pin, INPUT); // We configure the echo as input
    pinMode(relay_pin, OUTPUT);

    digitalWrite(relay_pin, HIGH);

    wifiManager.autoConnect("SMART-TANDON");

    sensors.begin();

    // manual_override = get_manual_status();

    if (WiFi.isConnected())
    {
        information = get_information();

        manual_override = information["state"]["manualMode"];
        
        height = information["config"]["height"];

        threshold = information["config"]["threshold"];
    }
}

void loop()
{
    delay(100);

    if (WiFi.isConnected())
    {
        information = get_information();
    }
    
    Serial.println("================================");
    
    float distance = get_distance();
    float temperature = get_temperature();
    float percentage = get_percentage(distance);

    if (!manual_override)
    {
        if (distance < threshold)
        {
            set_relay_status(true);
        }

        else
        {
            set_relay_status(false);
        }
    }

    if (WiFi.isConnected())
    {
        manual_override = information["state"]["manualMode"];
        post(int(distance), int(percentage), String(temperature), manual_override, relay);
    }

    delay(100);
}

void set_relay_status(bool is_close)
{
    if (is_close)
    {
        relay = true;

        digitalWrite(relay_pin, HIGH);

        Serial.println("Relay closed");
    }

    else
    {
        relay = false;

        digitalWrite(relay_pin, LOW);

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

    // Distance calculation
    distance_cm = ultrason_duration * SOUND_SPEED/2 * 0.0001;

    // print the distance on the serial port
    Serial.print("Distance (cm): ");
    Serial.println(distance_cm);

    return distance_cm;
}

float get_percentage(float distance_cm)
{
    float percentage = distance_cm / height * 100;

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
        
        return temp_celsius;
    }
    
    else
    {
        Serial.println("Could not get temps");

        return -1.0;
    }
}

void post(int distance, int level, String temperature, bool isManual, bool relay)
{
    HTTPClient http;
    JsonDocument root;
    String json;

    root["distance"] = distance;
    root["level"] = level;
    root["temperature"] = temperature;
    root["state"]["manualMode"] = isManual;
    root["state"]["relay"] = relay;
    
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

bool get_manual_status()
{
    HTTPClient http;
    JsonDocument doc;
    String response;

    http.begin(URL);
    http.GET();

    http.getString();
    deserializeJson(doc, response);
    JsonObject root = doc.as<JsonObject>();

    int result = root["state"]["manualMode"];

    return result;
}

JsonObject get_information()
{
    HTTPClient http;
    JsonDocument doc;
    String response;

    http.begin(URL);
    http.GET();

    http.getString();
    deserializeJson(doc, response);
    JsonObject root = doc.as<JsonObject>();

    return root;
}
