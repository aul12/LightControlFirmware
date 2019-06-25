#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

extern "C" {
#include "rc_lib.h"
}

#define D0 16
#define D1 5
#define D2 4

#define MAX_CONNECTIONS 16

const char* ssid = "WLAN-8XJGHP";
const char* password = "7653308624866630";

WiFiServer wifiServer(1337);
void (*currentSpecialMode)(void) = 0;

int wakeupTicks = 0;
int fadeTicks = 0;

void setup() {
    pinMode(D0, OUTPUT);
    pinMode(D1, OUTPUT);
    pinMode(D2, OUTPUT);
    setColor(1023,0,0);

    Serial.begin(115200);

    WiFi.begin(ssid, password);
    IPAddress ip(192,168,2,200);   
    IPAddress gateway(192,168,2,1);   
    IPAddress subnet(255,255,255,0);   
    WiFi.config(ip, gateway, subnet);

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting..");
    }

    Serial.print("Connected to WiFi. IP:");
    Serial.println(WiFi.localIP());

    wifiServer.begin();
    setColor(0,1023,0);
}

void loop() {
    static rc_lib_package_t pkg;
    rc_lib_init_rx(&pkg);

    if (wifiServer.hasClient()) {
        WiFiClient client = wifiServer.available();

        while (client.connected()) {
            while (client.connected() && client.available() > 0) {
                Serial.println("Got data");
                if (rc_lib_decode(&pkg, client.read())) {
                    Serial.println("Got package");
                    handlePackage(&pkg);
                    client.stop();
                }
            }
        }
    }

    if (currentSpecialMode) {
        currentSpecialMode();
    }
}

void setColor(uint16_t r, uint16_t g, uint16_t b) {
    analogWrite(D0, b);
    analogWrite(D1, r);
    analogWrite(D2, g);
}

void handlePackage(const rc_lib_package_t *pkg) {
    if (pkg->channel_count != 4) {
        Serial.println("Package has wrong channel count");
        return;
    }

    if (pkg->channel_data[0] == 0) {
        Serial.println("Normal package");
        currentSpecialMode = 0;
        setColor(pkg->channel_data[1],pkg->channel_data[2],pkg->channel_data[3]);
    } else {
        Serial.println("Special package");
        specialPackage(pkg->channel_data[0]);
    }
}

void specialPackage(uint8_t cmd) {
    switch(cmd) {
        case 1:
            wakeupTicks = 0;
            currentSpecialMode = wakeup;
            break;
        case 2:
            fadeTicks = 0;
            currentSpecialMode = fade;
            break;
    }
}

void wakeup() {
    static unsigned int lastCall = millis();

    if (millis() - lastCall > 1000) {
        lastCall = millis();
        if (wakeupTicks <= 0) {
            setColor(0,0,0);
        } else if (wakeupTicks <= 256) {
            setColor(wakeupTicks, 0, 0);
        } else if (wakeupTicks <= 256+64) {
            int tick = wakeupTicks-256;
            setColor(256, tick*4, tick);
        } else if (wakeupTicks <= 256+64+(255-65)) {
            int tick = wakeupTicks-(256+64);
            setColor(tick*4, tick*4, tick);
        } else if (wakeupTicks <= 256+64+(255-65)+(255-65)) {
            int tick = wakeupTicks-(256+64+(255-65));
            setColor(1023, 1023, 4*tick);
        } else {
            setColor(1023, 1023, 1023);
            --wakeupTicks;
        }

        wakeupTicks++;
    }
}

void fade() {
    static unsigned int lastCall = millis();

    if (millis() - lastCall > 2) {
        lastCall = millis();
        uint16_t r=0,g=0,b=0;

        int normalizedTick = fadeTicks % 1024 - 512;
        uint16_t primary = 1024 - abs(normalizedTick);
        uint16_t secondary = abs(normalizedTick);

        if (fadeTicks <= 1024-1) {
            r = primary;
            if (fadeTicks < 512) {
                b = secondary;
            } else {
                g = secondary;
            }
        } else if (fadeTicks <= 1024*2-1) {
            g = primary;
            if (fadeTicks < 1024+512) {
                r = secondary;
            } else {
                b = secondary;
            }
        } else if (fadeTicks <= 1024*3-1) {
            b = primary;
            if (fadeTicks < 1024*2 + 512) {
                g = secondary;
            } else {
                r = secondary;
            }
        }
        setColor(r,g,b);
        
        fadeTicks = (fadeTicks+1) % (1024*3);
    }
}

