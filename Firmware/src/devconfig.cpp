#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "devconfig.h"
#include "mqtt.h"
#include "otcontrol.h"
#include "sensors.h"
#include "auxInput.h"
#include <HADiscovery.h>

const char CFG_FILENAME[] PROGMEM = "/config.json";

const char CFGKEY_HOSTNAME[] PROGMEM = "hostname";
const char CFGKEY_HAPREFIX[] PROGMEM = "haPrefix";
const char CFGKEY_MQTT[] PROGMEM = "mqtt";
const char CFGKEY_OUTSIDETEMP[] PROGMEM = "outsideTemp";
const char CFGKEY_HEATING[] PROGMEM = "heating";
const char *CFGKEY_AUX PROGMEM = "aux";

DevConfig devconfig;
extern bool configMode;

DevConfig::DevConfig():
        writeBufFlag(false),
        fsOk(false) {
}

void DevConfig::begin() {
    fsOk = LittleFS.begin(false);
#ifdef NODO
    if (!fsOk) {
#else
    if (!fsOk || configMode) {
#endif
        if (configMode)
            Serial.println("Config mode: formatting LittleFS and reloading config.");
        else
            Serial.println("LittleFS mount failed; formatting...");

        LittleFS.format();
        fsOk = LittleFS.begin(false);
    }

    if (fsOk)
        Serial.printf("LittleFS mounted: total=%u used=%u\n", (unsigned) LittleFS.totalBytes(), (unsigned) LittleFS.usedBytes());
    else
        Serial.println("LittleFS mount failed; running without persisted config.");
    update();
}

void DevConfig::update() {
    if (!fsOk)
        return;
    File f = getFile();
    if (f) {
        JsonDocument doc;
        deserializeJson(doc, f);

        if (doc[FPSTR(CFGKEY_HOSTNAME)].is<String>())
            hostname = doc[FPSTR(CFGKEY_HOSTNAME)].as<String>();

        if (doc[FPSTR(CFGKEY_HAPREFIX)].is<String>())
            HADiscovery::setHAPrefix(doc[FPSTR(CFGKEY_HAPREFIX)].as<String>());

        timezone = doc[F("timezone")] | 3600;

        if (hostname.isEmpty())
            hostname = F(HOSTNAME);

        if (WiFi.isConnected()) {
            WiFi.setHostname(hostname.c_str());
            MDNS.begin(hostname);
        }

        if (doc[FPSTR(CFGKEY_MQTT)].is<JsonObject>()) {
            MqttConfig mc;
            const JsonObject &jobj = doc[FPSTR(CFGKEY_MQTT)].as<JsonObject>();
            mc.host = jobj[F("host")].as<String>();
            mc.port = jobj[F("port")].as<uint16_t>();
            mc.tls = jobj[F("tls")].as<bool>();
            mc.user = jobj[F("user")].as<String>();
            mc.pass = jobj[F("pass")].as<String>();
            mc.keepAlive = jobj[F("keepAlive")] | 15;
            mqtt.setConfig(mc);
        }

        OneWireNode::clear();
        for (int i=0; i<sizeof(auxInput) / sizeof(auxInput[0]); i++) {
            JsonObject obj = doc[FPSTR(CFGKEY_AUX)][i];
            auxInput[i].setConfig(obj);
        }   

        if (doc[FPSTR(CFGKEY_OUTSIDETEMP)].is<JsonObject>()) {
            JsonObject obj = doc[FPSTR(CFGKEY_OUTSIDETEMP)];
            outsideTemp.setConfig(obj);
        }

        for (int i=0; i<2; i++) {
            JsonObject hcfg = doc[FPSTR(CFGKEY_HEATING)][i]; 
            
            JsonObject obj = hcfg[F("roomtemp")];
            roomTemp[i].setConfig(obj);

            obj = hcfg[F("roomsetpoint")];
            roomSetPoint[i].setConfig(obj);

            obj = hcfg[F("returnLimit")];
            returnTemp[i].setConfig(obj);
        }

        JsonObject cfg = doc.as<JsonObject>();
        otcontrol.setConfig(cfg);

        f.close();
    }
}

File DevConfig::getFile() {
    if (!fsOk)
        return File();
    return LittleFS.open(FPSTR(CFG_FILENAME), "r");
}

void DevConfig::write(String &str) {
    File f = LittleFS.open(FPSTR(CFG_FILENAME), "w");
    f.write((uint8_t *) str.c_str(), str.length());
    f.close();
    writeBufFlag = true;
}

void DevConfig::remove() {
    if (fsOk)
        LittleFS.remove(FPSTR(CFG_FILENAME));
}

void DevConfig::loop() {
    if (fsOk && writeBufFlag) {
        writeBufFlag = false;
        update();
    }
}

String DevConfig::getHostname() const {
    return hostname;
}

int DevConfig::getTimezone() const {
    return timezone;
}