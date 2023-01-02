#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <TelnetSpy.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ElegantOTA.h>
#include <FastLED.h>
#include <Wire.h>
#include <PubSubClient.h>

const char* ssid = "SSID";
const char* password = "password";

IPAddress staticIP(10, 11, 12, 57); //ESP static ip
IPAddress gateway(10, 11, 12, 1);   //IP Address of your WiFi Router (Gateway)
IPAddress subnet(255, 255, 255, 0);  //Subnet mask
IPAddress dns(10, 11, 12, 1);  //DNS

//  MQTT
const char* mqtt_server = "10.11.12.20";
String clientId = "KFlurUHi_";
char const* switchTopic1 = "/KFlurUHi/Reset";
char const* switchTopic2 = "/KFlurUHi/Bright";
WiFiClient espClient;
PubSubClient client(espClient);
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];
long int value = 0;
unsigned long lastMsg = (millis());
unsigned long currentMillis = (millis());
unsigned long timerReconnect;
unsigned long timerLED;
unsigned long timerRadar;

long rssi;

const long interval = 1000;           // interval at which to blink (milliseconds)
unsigned long previousMillis = 0;        // will store last time LED was updated
//ACHTUNG: BuiltinLed ist auf D4
#define LEDrt D5  // WLAN D8
#define LEDgr D3        // blink Achtung: GPIO0 geht nicht!
#define LEDbl D0     // MQTT
#define RadarP D2    // 10k pulldown VIN=5V!
bool Lgruen = false;
bool Lrot = false;
bool Lblau = false;
int lauf;
long rauf = 1L;

TelnetSpy SerialAndTelnet;

ESP8266WebServer server(80);


//#define SERIAL  Serial
#define SERIAL SerialAndTelnet

//Fastled
#define LED_PIN     D7     // D7
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1

int Bright = 100;
#define FRAMES_PER_SECOND 60

bool gReverseDirection = false;

CRGB leds[NUM_LEDS];
CRGBPalette16 gPal;

void callback(char* topic, byte* payload, unsigned int length) 
{
  String topicStr = topic;
  SERIAL.print("Message arrived [");
  SERIAL.print(topicStr);
  SERIAL.print("] ");

  String msgIN = "";
  for (int i=0 ; i<length ; i++)
  {
    msgIN += (char)payload[i];
  }
  String msgString = msgIN;
  SERIAL.print("msgString:  ");
  SERIAL.println(msgString);

  if (topicStr == switchTopic1)
  {
    if (msgString == "true")
    {
      client.publish("/KFlurUHi/Reset", "false");
      SERIAL.print("Bye...");
      delay(500);
      ESP.restart();
    } 
  }
  if (topicStr == switchTopic2)
  {
    Bright = msgString.toInt();
    FastLED.setBrightness( Bright );
  }
}

void reconnect() 
{
    SERIAL.print("Attempting MQTT connection...");   
    clientId = clientId + String(random(0xffff), HEX); // Create a random client ID
    
    if (client.connect(clientId.c_str())) 
    {   // Attempt to connect
        SERIAL.println("MQTT Connected");
        client.subscribe(switchTopic1);
        delay(50);
        client.subscribe(switchTopic2);
        delay(50);
        Lblau = true;
    }
    else 
    {        
        SERIAL.print("failed, rc=");
        SERIAL.print(client.state());
        Lblau = false;
        SERIAL.println(" try again in 5 seconds");
        
     }
  
}

void setupMQTT()
{
  Wire.begin();
  client.setServer(mqtt_server, 1883);
  client.setKeepAlive(120);
  client.setCallback(callback);
}

void setup_wifi() 
{
  delay(10);
     
  // We start by connecting to a WiFi network
  SERIAL.println();
  SERIAL.print("Connecting to ");
  SERIAL.println(ssid);
  WiFi.config(staticIP, gateway, subnet, dns);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
 
  while (WiFi.status() != WL_CONNECTED) 
  {
      digitalWrite(LED_BUILTIN, HIGH);
      delay(200);
      SERIAL.print(".");
      digitalWrite(LED_BUILTIN, LOW);
      delay(200);
      Lrot = false;
  }

  randomSeed(micros());

  SERIAL.println("");
  SERIAL.println("WiFi connected");
  SERIAL.println("IP address: ");
  SERIAL.println(WiFi.localIP());
  SERIAL.print("RSSI:  ");
  rssi = WiFi.RSSI();
  SERIAL.println(rssi);
  digitalWrite(LED_BUILTIN, HIGH);
  Lrot = true;
}

void telnetConnected() {
  SERIAL.println("Telnet connection established.");
  SERIAL.println("R -> reboot!");
}

void telnetDisconnected() {
  SERIAL.println("Telnet connection closed.");
}

void serialEcho()
  {
  // Serial Echo
  if (SERIAL.available() > 0) 
  {
    char c = SERIAL.read();
    switch (c) 
    {
      case '\r':
        SERIAL.println();
        break;
      case 'C':
        // nix
        break;
      case 'D':
        // nix
        break;
      case 'R':
        SERIAL.print("Bye...");
        delay(500);
        ESP.restart();
        break;
      default:
        SERIAL.print(c);
        break;
    }
  }
}

void setupTelnet()
{
  SerialAndTelnet.setWelcomeMsg("Hier ist der D1 KellerFlurUntenHinten");
  SerialAndTelnet.setCallbackOnConnect(telnetConnected);
  SerialAndTelnet.setCallbackOnDisconnect(telnetDisconnected);
  SERIAL.begin(74880);
  delay(100); // Wait for serial port
  SERIAL.setDebugOutput(false);
}

void setupOTA()
{
  // During updates "over the air" the telnet session will be closed.
  // So the operations of ArduinoOTA cannot be seen via telnet.
  // So we only use the standard "Serial" for logging.
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
    FastLED.setBrightness( 200 );
    leds[0] = CRGB::Purple;
    FastLED.show();
    digitalWrite(LEDrt, LOW);
    digitalWrite(LEDgr, LOW);
    digitalWrite(LEDbl, LOW);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) SERIAL.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) SERIAL.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) SERIAL.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) SERIAL.println("Receive Failed");
    else if (error == OTA_END_ERROR) SERIAL.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setupElegantOTA()
{
  server.on("/", []() {
    server.send(200, "text/plain", "Hi! I am D1KellerFlurUntenHinten.");
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  server.begin();
  SERIAL.println("HTTP server started");
}

void LEDaction()
{
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) 
  {
    previousMillis = currentMillis;
    lauf = lauf + 1;

    if (Lrot == false && lauf == 1)
    {
      lauf = 2;
    }
    if (Lrot == true && lauf == 1)
    {
      leds[0] = CRGB::DarkRed;
      digitalWrite(LEDrt, HIGH);
      digitalWrite(LEDgr, LOW);
      digitalWrite(LEDbl, LOW);
    }
    else 
    {
      digitalWrite(LEDrt, LOW);
    }
    if (Lgruen == false && lauf == 2)
    {
      lauf = 3;
    }
    if (Lgruen == true && lauf == 2)
    {
      leds[0] = CRGB::DarkGreen;
      digitalWrite(LEDgr, HIGH);
      digitalWrite(LEDbl, LOW);
      digitalWrite(LEDrt, LOW);
    }
    else 
    {
      digitalWrite(LEDgr, LOW);
    }

    if (Lblau == false && lauf ==3)
    {
      lauf = 4;
    }
    if (Lblau == true && lauf == 3)
    {
      leds[0] = CRGB::Blue;
      digitalWrite(LEDbl, HIGH);
      digitalWrite(LEDrt, LOW);
      digitalWrite(LEDgr, LOW);
    }
    else
    {
      digitalWrite(LEDbl, LOW);
    }

    if (Lrot == false && lauf == 4)
    {
      lauf = 0;
    }
    if (Lrot == true && lauf == 4)
    {
      digitalWrite(LEDrt, HIGH);
      digitalWrite(LEDgr, LOW);
      digitalWrite(LEDbl, LOW);
      lauf = 1;
    }
    
    if (lauf > 2)
    {
      lauf = 0;
    }
    digitalWrite(LED_BUILTIN, LOW);
  }

  //leds[0] = CRGB::DarkGreen;
  //leds[1] = CRGB::Green;
  //leds[2] = CRGB::Yellow;
  //leds[3] = CRGB::Orange;
  //leds[4] = CRGB::OrangeRed;
  //leds[5] = CRGB::Red;
  delay(20);
  FastLED.show();

}

void setup() 
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RadarP, INPUT);
  pinMode(LEDbl, OUTPUT);
  pinMode(LEDrt, OUTPUT);
  pinMode(LEDgr, OUTPUT);
  digitalWrite(LEDbl, LOW);
  digitalWrite(LEDrt, LOW);
  digitalWrite(LEDgr, LOW);

  //Fastled
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( Bright );
  gPal = HeatColors_p;

  setupMQTT();
  reconnect();

  setupTelnet();
  setup_wifi();
  setupOTA();
  setupElegantOTA();
      
  SERIAL.println("Chars will be echoed. Play around...\n");
}

void loop() {
  SerialAndTelnet.handle();
  ArduinoOTA.handle();
  server.handleClient();   // ElegantOTA

  // WLAN neu verbinden
  if (WiFi.status() != WL_CONNECTED) 
  {
      delay(500);
      //digitalWrite(LED_BUILTIN, HIGH);
      Lrot = false;
      setup_wifi();
  }
  

  // MQTT wieder verbinden  
  if (!client.connected())  
  {
    if ((millis() - timerReconnect) > 5000)
    {
      timerReconnect = millis();
      clientId = "KFlurUHi_";
      setupMQTT();
      reconnect();
    }
  }
  
  client.loop();

  
  unsigned long now = millis();  // Statusmeldungen
  if (now - lastMsg > 60000) 
  {
    lastMsg = now;
    ++value;
    snprintf (msg, MSG_BUFFER_SIZE, "true #%ld", value);
    SERIAL.print("Publish message: ");
    SERIAL.println(msg);
    client.publish("/KFlurUHi/available", msg);
    rssi = WiFi.RSSI();
    snprintf (msg, MSG_BUFFER_SIZE, "%ld", rssi);
    client.publish("/KFlurUHi/rssi", msg);
  }
  
  if (digitalRead(RadarP) == HIGH && Lgruen == false)
  {
    SERIAL.println("Radar aktiv");
    timerRadar = millis();
    Lgruen = true;
    client.publish("/KFlurUHi/Radar", "true");
  }
  if (digitalRead(RadarP) == LOW && Lgruen == true && timerRadar < (millis() - 6000))  //Zeit, bis er abfällt, damit er auch sicher wieder scharf is
  {
    SERIAL.println("Radar frei");
    Lgruen = false;
    client.publish("/KFlurUHi/Radar", "false");
  }
  
  serialEcho();

  LEDaction();
  
}
