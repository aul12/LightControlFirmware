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
#define D3 0
#define D4 2
#define D5 14
#define D6 12

#define PIN_G D3
#define PIN_R D4
#define PIN_W D5
#define PIN_B D6

#include "private.h"

#ifndef ssid
    #error "private.h should define a macro called ssid"
#endif

#ifndef password
    #error "private.h should define a macro called password"
#endif

WiFiServer wifiServer(1337);
void (*currentSpecialMode)(void) = 0;

int wakeupTicks = 0;
int fadeTicks = 0;

rc_lib_package_t pkg;

void setup() {
    pinMode(D0, OUTPUT);
    pinMode(D1, OUTPUT);
    pinMode(D2, OUTPUT);
    pinMode(PIN_R, INPUT_PULLUP);
    pinMode(PIN_G, INPUT_PULLUP);
    pinMode(PIN_B, INPUT_PULLUP);
    pinMode(PIN_W, INPUT_PULLUP);
    setColor(1023,0,0);

    Serial.begin(115200);

    WiFi.begin(ssid, password);
    IPAddress ip(192,168,2,200);   
    IPAddress gateway(192,168,2,1);   
    IPAddress subnet(255,255,255,0);   
    WiFi.config(ip, gateway, subnet);

    Serial.println();

    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting..");
    }

    Serial.print("Connected to WiFi. IP:");
    Serial.println(WiFi.localIP());

    wifiServer.begin();
    setColor(0,1023,0);
    rc_lib_init_rx(&pkg);
}

void loop() {
    if (WiFi.status() == WL_CONNECTED) {
        if (wifiServer.hasClient()) {
            WiFiClient client = wifiServer.available();

            Serial.print("Available: ");
            Serial.println(client.available());

            while (client.connected() || client.available() > 0) {
                if (client.available() > 0) {
                  Serial.println("Got data");
                  if (rc_lib_decode(&pkg, client.read())) {
                      Serial.println("Got package");
                      handlePackage(&pkg);
                      client.stop();
                  }
                }
            }
        }
    } else {
      Serial.println("Not connected!");
    }

    if (currentSpecialMode) {
        currentSpecialMode();
    }

    handleButtons();
}

bool isEdge(int pin, bool *lastState, long *lastEdge) {
    bool state = digitalRead(pin);
    long t = millis();
    if (state != *lastState) {
        bool isRealEdge = (*lastEdge < t - 10);
        *lastState = state;
        *lastEdge = t;
        return isRealEdge;
    } else {
        return false;
    }
}

void handleButtons() {
    static long lastEdgeR = 0, lastEdgeG = 0, lastEdgeB = 0, lastEdgeW = 0;
    static bool lastStateR = digitalRead(PIN_R);
    static bool lastStateG = digitalRead(PIN_G);
    static bool lastStateB = digitalRead(PIN_B);
    static bool lastStateW = digitalRead(PIN_W);

    if (isEdge(PIN_R, &lastStateR, &lastEdgeR) && lastStateR) {
        Serial.println("Set R");
        currentSpecialMode = fade;
    }

    if (isEdge(PIN_G, &lastStateG, &lastEdgeG) && lastStateG) {
        setColor(0, 1023, 0);
        Serial.println("Set G");
        currentSpecialMode = 0;
    }

    if (isEdge(PIN_B, &lastStateB, &lastEdgeB) && lastStateB) {
        setColor(0, 0, 1023);
        Serial.println("Set B");
        currentSpecialMode = 0;
    }

    if (isEdge(PIN_W, &lastStateW, &lastEdgeW) && lastStateW) {
        setColor(0, 0, 0);
        Serial.println("Clear");
        currentSpecialMode = 0;
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
        default: break;
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
