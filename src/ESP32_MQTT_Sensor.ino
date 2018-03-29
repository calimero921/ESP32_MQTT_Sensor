#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <FS.h>
#include <math.h>
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson.git

//#include <WiFiClient.h>
//#include <DNSServer.h>
//#include <ESP8266WebServer.h>
//#include <ESP8266mDNS.h>
//#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager.git

//Bibliothèque des capteurs DHT
#include <DHT_U.h>

//Bibliothèque MQTT
#include <PubSubClient.h>

//SH1106 I2C
#include <SH1106.h>               //https://github.com/squix78/esp8266-oled-ssd1306.git
#include <OLEDDisplay.h>
#include <OLEDDisplayFonts.h>
#include <OLEDDisplayUi.h>

// Include custom images
#include "images.h"

#define pin_sysled 13
#define pin_cmdled 12

//const char* configurationAPName = "AutoConnectAP";
const char* ssid     = "orangelabs";
const char* password = "softpessac";

// Display Settings
#define pin_I2CSDA 5
#define pin_I2CSCL 4
#define I2C_oled_adr 0x3c
SH1106 display(I2C_oled_adr, pin_I2CSDA, pin_I2CSDA);
OLEDDisplayUi ui (&display);
StaticJsonBuffer<1000> jsonBuffer;

unsigned long interval = 1000;             // refresh display interval
unsigned long prevMillis = 0;

unsigned long intervalPublish = 15000;     // MQTT publish interval
unsigned long prevMillisPublish = 0;

char mqtt_server[40] = "192.168.1.60";         //s'il a été configuré
char mqtt_port[5] = "1883";                    //s'il a été configuré
char mqtt_user[20] = "mqtt-test";              //s'il a été configuré
char mqtt_password[20] = "mqtt-test";          //s'il a été configuré

//flag for saving data
bool shouldSaveConfig = false;

#define temperature_topic "sensor/temperature"  //Topic température
#define humidity_topic "sensor/humidity"        //Topic humidité
#define led_topic "action/led"                  //Topic led

//Buffer qui permet de décoder les messages MQTT reçus
char message_buff[100];
long lastMsg = 0;             //Horodatage du dernier message publié sur MQTT
long lastRecu = 0;

//Dé-commentez la ligne qui correspond à votre capteur
#define DHTPIN 14             // Pin sur lequel est branché le DHT D5
#define DHTTYPE DHT22         // DHT 22  (AM2302)
DHT_Unified dht(DHTPIN, DHTTYPE);

//Création des objets
WiFiClient espClient;
PubSubClient mqttClient;

//*****************
//* initial setup *
//*****************
void setup() {
    Serial.begin(115200);   //Facultatif pour le debug

    pinMode(pin_sysled, OUTPUT);
    digitalWrite(pin_sysled, HIGH);
    delay(500);
    digitalWrite(pin_sysled, LOW);
    digitalWrite(pin_sysled, 0);

    pinMode(pin_cmdled, OUTPUT);
    digitalWrite(pin_cmdled, HIGH);
    delay(500);
    digitalWrite(pin_cmdled, LOW);
    digitalWrite(pin_cmdled, 0);

    //saut de ligne pour l'affichage dans la console.
    Serial.println();
    Serial.println();

    //loading configuration
   // if(!SPIFFS.begin()){
   //     Serial.println("SPIFFS Mount Failed");
   //     return;
   // }
   // loadSettings();

    //initialize dispaly
    display.init();
    display.clear();
    display.display();

    display.flipScreenVertically();
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.setContrast(255);
    display.drawString(64, 10, "Initialisation...");
    display.display();

    //WiFiManager
    //custom parameters
//    WiFiManagerParameter custom_mqtt_title("MQTT Configuration");
//    WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
//    WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
//    WiFiManagerParameter custom_mqtt_user("user", "mqtt user", mqtt_user, 20);
//    WiFiManagerParameter custom_mqtt_password("password", "mqtt password", mqtt_password, 20);

    //Local intialization. Once its business is done, there is no need to keep it around
//    WiFiManager wifiManager;

    //set config save notify callback
//    wifiManager.setSaveConfigCallback(saveConfigCallback);

    //add all your parameters here
//    wifiManager.addParameter(&custom_mqtt_title);
//    wifiManager.addParameter(&custom_mqtt_server);
//    wifiManager.addParameter(&custom_mqtt_port);
//    wifiManager.addParameter(&custom_mqtt_user);
//    wifiManager.addParameter(&custom_mqtt_password);

    //reset saved settings
//    wifiManager.resetSettings();
//    wifiManager.setAPCallback(configModeCallback);
//    wifiManager.setConfigPortalTimeout(180);

    //fetches ssid and pass from eeprom and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
//    if (!wifiManager.autoConnect(configurationAPName)) {
//        Serial.println("failed to connect and hit timeout");
//        delay(3000);
    //reset and try again, or maybe put it to deep sleep
//        ESP.reset();
//        delay(5000);
//    }

  dht.begin();
  sensor_t sensor;

  dht.temperature().getSensor(&sensor);
  dht.humidity().getSensor(&sensor);

    //connexion Wifi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

  //if you get here you have connected to the WiFi
    Serial.println("connected...");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
//    if (MDNS.begin("esp32")) {
//        Serial.println("MDNS responder started");
//    }

    //read updated parameters
//    strcpy(mqtt_server, custom_mqtt_server.getValue());
//    strcpy(mqtt_port, custom_mqtt_port.getValue());
//    strcpy(mqtt_user, custom_mqtt_user.getValue());
//    strcpy(mqtt_password, custom_mqtt_password.getValue());

    //save the custom parameters to FS
//    if (shouldSaveConfig) {
//        saveSettings();
//    }

   // mqttClient.setClient(espClient);
   // mqttClient.setServer(mqtt_server, atoi(mqtt_port));    //Configuration de la connexion au serveur MQTT
   // mqttClient.setCallback(mqttCallback);       //La fonction de callback qui est executée à chaque réception de message

    displayManagement();
}

//*************
//* main loop *
//*************
void loop() {
    unsigned long currMillis = millis();
    // gestion de l'interval entre les actions d'affichage
    if (currMillis > (prevMillis + interval)) {
        prevMillis = currMillis;
        displayManagement();
    }

    // gestion de l'interval entre les actions d'affichage
    if (currMillis > (prevMillisPublish + intervalPublish)) {
        prevMillisPublish = currMillis;
//        if (!mqttClient.connected()) {
//            reconnect();
//        }
//        mqttClient.loop();
        long now = millis();
        Serial.print("Temperature : ");
        Serial.print(printTemperature());
        Serial.print(" | Humidite : ");
        Serial.println(printHumidity());
//        mqttClient.publish(temperature_topic, String(t).c_str(), true);   //Publie la température sur le topic temperature_topic
//        mqttClient.publish(humidity_topic, String(h).c_str(), true);      //Et l'humidité

//        if (now - lastRecu > 100 ) {
//            lastRecu = now;
//            mqttClient.subscribe(led_topic);
//        }
    }
}

//Reconnexion
// void reconnect() {
//     //Boucle jusqu'à obtenur une reconnexion
//     while (!mqttClient.connected()) {
//         Serial.print("Connexion au serveur MQTT...");
//         if (mqttClient.connect("ESPClient", mqtt_user, mqtt_password)) {
//             Serial.println("OK");
//         } else {
//             Serial.print("KO, erreur : ");
//             Serial.print(mqttClient.state());
//             Serial.println(" On attend 5 secondes avant de recommencer");
//             delay(5000);
//         }
//     }
// }

//**********************
//* display management *
//**********************
void displayManagement() {
    display.clear();

    display.setFont(ArialMT_Plain_10);
//    display.setTextAlignment(TEXT_ALIGN_LEFT);
//    display.drawString(0, 0, printDate(now));
//    display.setTextAlignment(TEXT_ALIGN_RIGHT);
//    display.drawString(128, 0, printTime(now));

    //wifi signal indicator
    if (WiFi.status() == WL_CONNECTED) {
        const long signalWiFi = WiFi.RSSI();
        if (signalWiFi > -100) {
            int strength = 0;
            if (signalWiFi > (-45)) {
                strength = 5;
            } else if (signalWiFi > (-55)) {
                strength = 4;
            } else if (signalWiFi > (-65)) {
                strength = 3;
            } else if (signalWiFi > (-75)) {
                strength = 2;
            } else if (signalWiFi > (-85)) {
                strength = 1;
            }
            display.drawXbm(60, 2, 8, 8, wifiSymbol);
            for (int i = 0; i < strength; i++) {
                display.drawLine(68 + (i * 2), 10, 68 + (i * 2), 10 - (i * 2));
            }
            display.setFont(ArialMT_Plain_10);
            display.setTextAlignment(TEXT_ALIGN_CENTER);
            String signalWifiInfo = String(signalWiFi);
            signalWifiInfo += "dBm";
            display.drawString(96, 54, signalWifiInfo);
        }
    }

    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(32, 11, printTemperature());

    display.setFont(ArialMT_Plain_16);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(96, 11, printHumidity());

    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_CENTER);
    display.drawString(32, 54, WiFi.localIP().toString());

    display.display();
}

//**********************
//*    MQTT Callback   *
//**********************
// Déclenche les actions à la réception d'un message
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    int i = 0;
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length, DEC));
    //create character buffer with ending null terminator (string)
    for (i = 0; i < length; i++) {
        message_buff[i] = payload[i];
    }
    message_buff[i] = '\0';

    String msgString = String(message_buff);
    Serial.println("Payload: " + msgString);

   if (msgString == "ON" ) {
       digitalWrite(pin_cmdled, HIGH);
   } else {
       digitalWrite(pin_cmdled, LOW);
   }
}

//*********************************
//* WifiManager Settings Callback *
//*********************************
//callback notifying us of the need to save config
//void saveConfigCallback() {
//    Serial.println("Should save config");
//    shouldSaveConfig = true;
//}

//**********************
//* configuration mode *
//**********************
//void configModeCallback (WiFiManager *myWiFiManager) {
//    display.clear();
//    display.setFont(ArialMT_Plain_10);
//    display.setTextAlignment(TEXT_ALIGN_CENTER);
//    display.drawString(64, 10, "Configuaration Mode");
//    display.drawString(64, 28, "please connect to");
//    display.setFont(ArialMT_Plain_16);
//    display.drawString(64, 46, String(configurationAPName));
//    display.display();
//
//    Serial.println("Entered config mode");
//    Serial.println(WiFi.softAPIP());
//
//    Serial.println(myWiFiManager->getConfigPortalSSID());
//}

//*************
//* utilities *
//*************
#define countof(a) (sizeof(a) / sizeof(a[0]))

String formatJSON(const JsonObject& obj) {
    char buffer[1000];
    obj.printTo(buffer, sizeof(buffer));
    String strOut = buffer;
    return strOut;
}

String printTemperature() {
    String value = "---";
    String unit = "°C";

    // Lecture de l'humidité en %
    sensors_event_t event;
    dht.temperature().getEvent(&event);
    float t = event.temperature;

    if (!isnan(t)) {
        value = String(t).c_str();
        value += " ";
        value += unit;
    }
    return value;
}

String printHumidity() {
    String value = "---";
    String unit = "%";

    // Lecture de l'humidité en %
    sensors_event_t event;
    dht.humidity().getEvent(&event);
    float h = event.relative_humidity;
    if (!isnan(h)) {
        value = String(h).c_str();
        value += " ";
        value += unit;
    }
    return value;
}

void saveSettings() {
    Serial.println("saving config");
    jsonBuffer.clear();
    JsonObject& json = jsonBuffer.createObject();
    //Serial.println("encodage JSON");
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;
    json["mqtt_user"] = mqtt_user;
    json["mqtt_password"] = mqtt_password;
    //json.prettyPrintTo(Serial);
    //Serial.println();

    File configFile = SPIFFS.open("/settings.json", "w");
    if (!configFile) {
        Serial.println("file open failed");
    } else {
        //Serial.println("ecriture fichier");
        json.printTo(configFile);
    }
    //Serial.println("fermeture dufichier");
    configFile.close();
    Serial.println("saving done");
}

void loadSettings() {
    Serial.println("loading configuration");
    //creation du fichier avec les valeurs par default si il n'existe pas
    if (!SPIFFS.exists("/settings.json")) {
        Serial.println("missing file -> creation");
        saveSettings();
    }

    //lecture des données du fichier
    File configFile = SPIFFS.open("/settings.json", "r");
    if (!configFile) {
        Serial.println("failed opening file");
    } else {
        Serial.println("reading file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);

        Serial.println("decoding JSON");
        jsonBuffer.clear();
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        //json.prettyPrintTo(Serial);
        //Serial.println();
        if (json.success()) {
            //Serial.println("parsing dans les variables");
            strcpy(mqtt_server, json["mqtt_server"]);
            strcpy(mqtt_port, json["mqtt_port"]);
            strcpy(mqtt_user, json["mqtt_user"]);
            strcpy(mqtt_password, json["mqtt_password"]);
        }

        //Serial.println("closing file");
        configFile.close();
        Serial.println("loading configuration done");
    }
}
