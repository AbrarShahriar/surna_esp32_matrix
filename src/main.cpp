#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "SPIFFS.h"
#include <MD_MAX72xx.h>
#include <HTTPClient.h>
#include <ArduinoJSON.h>

const char *rootCACertificate = R"literal(-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)literal";

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 1
#define CS_PIN 5

#define SERVER_NAME "https://fair-plum-lemur-boot.cyclic.app/"

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

AsyncWebServer server(80);

TaskHandle_t Task2;
TaskHandle_t Task1;

const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";
const char *PARAM_INPUT_3 = "ip";
const char *PARAM_INPUT_4 = "gateway";

String ssid;
String pass;
String ip;
String gateway;

bool booted = false;
bool loading = false;

const char *ssidPath = "/ssid.txt";
const char *passPath = "/pass.txt";
const char *ipPath = "/ip.txt";
const char *gatewayPath = "/gateway.txt";

IPAddress localIP;
IPAddress localGateway;
IPAddress subnet(255, 255, 0, 0);

unsigned long previousMillis = 0;
const long interval = 60000;

unsigned long lastTime = 0;
unsigned long timerDelay = 10000;

void drawShape(JsonArray data)
{
  mx.clear();
  for (int i = 0; i <= 7; i++)
  {
    mx.setRow(0, 0, i, data[i]);
  }
}

void drawBootAnim()
{
  Serial.println("drawbootanim");
  byte stickStates[4][8] = {{0, 0, 32, 16, 8, 4, 0, 0}, {0, 0, 16, 16, 8, 8, 0, 0}, {0, 0, 8, 8, 16, 16, 0, 0}, {0, 0, 4, 8, 16, 32, 0, 0}};

  for (byte j = 0; j < 4; j++)
  {
    if (booted == false)
    {
      for (byte i = 0; i < 7; i++)
      {
        mx.setRow(0, 0, i, stickStates[j][i]);
      }
      delay(500);
      mx.clear();
    }
  }
}

void drawWifiMode(byte mode)
{
  if (mode == 1)
  {
    byte AP[8] = {0, 0, 38, 86, 116, 84, 0, 0};
    for (byte i = 0; i < 7; i++)
    {
      mx.setRow(0, 0, i, AP[i]);
    }
  }
  else if (mode == 2)
  {
    byte ST[8] = {0, 0, 110, 68, 100, 36, 100, 0};
    for (byte i = 0; i < 7; i++)
    {
      mx.setRow(0, 0, i, ST[i]);
    }
  }
}

// Initialize SPIFFS
void initSPIFFS()
{
  if (!SPIFFS.begin(true))
  {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char *path)
{
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if (!file || file.isDirectory())
  {
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while (file.available())
  {
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char *path, const char *message)
{
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if (!file)
  {
    Serial.println("- failed to open file for writing");
    return;
  }
  if (file.print(message))
  {
    Serial.println("- file written");
  }
  else
  {
    Serial.println("- frite failed");
  }
}

bool initWiFi()
{
  if (ssid == "" || ip == "")
  {
    Serial.println("Undefined SSID or IP address.");
    return false;
  }

  WiFi.mode(WIFI_STA);
  localIP.fromString(ip.c_str());
  localGateway.fromString(gateway.c_str());

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Connecting to WiFi...");

  // if (!WiFi.config(localIP, localGateway, subnet, IPAddress(8, 8, 8, 8)))
  // {
  //   Serial.println("STA Failed to configure");
  //   return false;
  // }

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      Serial.println("Failed to connect.");
      return false;
    }
  }

  Serial.print("Go to: ");
  Serial.println(WiFi.localIP());
  return true;
}

void initMatrix()
{
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 1);
  mx.clear();
}

void Task2Code(void *parameter)
{
  for (;;)
  {
    if (booted == false)
    {
      drawBootAnim();
    }
  }
}

void drawIP()
{
  mx.clear();
  uint16_t c[10] = {48, 49, 50, 51, 51, 53, 54, 55, 56, 57};
  String a = WiFi.localIP().toString();
  Serial.println(a);
  char buf[a.length() + 1];
  a.toCharArray(buf, a.length() + 1);
  for (int i = 0; i < a.length(); i++)
  {
    Serial.println(buf[i]);

    String x = String(buf[i]);
    if (x == "0")
    {
      mx.setChar(5, c[0]);
    }
    else if (x == "1")
    {
      mx.setChar(5, c[1]);
    }
    else if (x == "2")
    {
      mx.setChar(5, c[2]);
    }
    else if (x == "3")
    {
      mx.setChar(5, c[3]);
    }
    else if (x == "4")
    {
      mx.setChar(5, c[4]);
    }
    else if (x == "5")
    {
      mx.setChar(5, c[5]);
    }
    else if (x == "6")
    {
      mx.setChar(5, c[6]);
    }
    else if (x == "7")
    {
      mx.setChar(5, c[7]);
    }
    else if (x == "8")
    {
      mx.setChar(5, c[8]);
    }
    else if (x == "9")
    {
      mx.setChar(5, c[9]);
    }
    else if (x == ".")
    {
      mx.setChar(5, 46);
    }
    delay(1000);
    if (i != a.length() - 1)
    {
      mx.clear();
    }
  }
}

void drawLoading()
{
  mx.clear();
  byte loadingFrames[4][8] = {{0, 0, 0, 0, 32, 16, 0, 0}, {0, 0, 16, 32, 0, 0, 0, 0}, {0, 0, 8, 4, 0, 0, 0, 0}, {0, 0, 0, 0, 4, 8, 0, 0}};

  for (byte j = 0; j < 4; j++)
  {
    {
      if (loading == true)
      {
        for (byte i = 0; i < 7; i++)
        {
          mx.setRow(0, 0, i, loadingFrames[j][i]);
        }
        delay(100);
        mx.clear();
      }
    }
  }
}

void Task1Code(void *parameter)
{
  for (;;)
  {
    if (loading == true)
    {
      drawLoading();
    }
  }
}

void setup()
{
  Serial.begin(115200);
  initMatrix();

  xTaskCreatePinnedToCore(
      Task2Code, /* Function to implement the task */
      "Task2",   /* Name of the task */
      1000,      /* Stack size in words */
      NULL,      /* Task input parameter */
      1,         /* Priority of the task */
      &Task2,    /* Task handle. */
      1);        /* Core where the task should run */

  initSPIFFS();

  // Load values saved in SPIFFS
  ssid = readFile(SPIFFS, ssidPath);
  pass = readFile(SPIFFS, passPath);
  ip = readFile(SPIFFS, ipPath);
  gateway = readFile(SPIFFS, gatewayPath);
  Serial.println(ssid);
  Serial.println(pass);
  Serial.println(ip);
  Serial.println(gateway);

  if (initWiFi())
  {
    booted = true;
    delay(500);
    drawIP();

    // Route for root / web page
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html", false); });
    server.serveStatic("/", SPIFFS, "/");

    // Route to set GPIO state to HIGH
    server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html", false); });

    // Route to set GPIO state to LOW
    server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/index.html", "text/html", false); });
    server.begin();
  }
  else
  {
    // Connect to Wi-Fi network with SSID and password
    Serial.println("Setting AP (Access Point)");
    // NULL sets an open Access Point
    WiFi.softAP("SURNA-WIFI-MANAGER", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    booted = true;
    delay(500);
    drawWifiMode(1);

    // Web Server Root URL
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(SPIFFS, "/wifimanager.html", "text/html"); });

    server.serveStatic("/", SPIFFS, "/");

    server.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
              {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          // HTTP POST ssid value
          if (p->name() == PARAM_INPUT_1) {
            ssid = p->value().c_str();
            Serial.print("SSID set to: ");
            Serial.println(ssid);
            // Write file to save value
            writeFile(SPIFFS, ssidPath, ssid.c_str());
          }
          // HTTP POST pass value
          if (p->name() == PARAM_INPUT_2) {
            pass = p->value().c_str();
            Serial.print("Password set to: ");
            Serial.println(pass);
            // Write file to save value
            writeFile(SPIFFS, passPath, pass.c_str());
          }
          // HTTP POST ip value
          if (p->name() == PARAM_INPUT_3) {
            ip = p->value().c_str();
            Serial.print("IP Address set to: ");
            Serial.println(ip);
            // Write file to save value
            writeFile(SPIFFS, ipPath, ip.c_str());
          }
          // HTTP POST gateway value
          if (p->name() == PARAM_INPUT_4) {
            gateway = p->value().c_str();
            Serial.print("Gateway set to: ");
            Serial.println(gateway);
            // Write file to save value
            writeFile(SPIFFS, gatewayPath, gateway.c_str());
          }
        }
      }
      request->send(200, "text/plain", "Done. ESP will restart, connect to your router");
      delay(3000);
      ESP.restart(); });

    server.begin();
  }

  xTaskCreatePinnedToCore(
      Task1Code, /* Function to implement the task */
      "Task1",   /* Name of the task */
      1000,      /* Stack size in words */
      NULL,      /* Task input parameter */
      1,         /* Priority of the task */
      &Task1,    /* Task handle. */
      1);        /* Core where the task should run */
}

void loop()
{
  if ((millis() - lastTime) > timerDelay)
  {
    // Check WiFi connection status
    if (WiFi.status() == WL_CONNECTED)
    {
      HTTPClient https;

      String serverPath = SERVER_NAME;
      https.begin(serverPath.c_str(), rootCACertificate);

      // Send HTTP GET request
      loading = true;
      int httpResponseCode = https.GET();
      // int httpResponseCode = -1;

      if (httpResponseCode > 0)
      {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = https.getString();

        DynamicJsonDocument doc(2048);
        deserializeJson(doc, payload);

        JsonArray data = doc.as<JsonArray>();
        Serial.print("loop() running on core ");
        Serial.println(xPortGetCoreID());
        Serial.println(payload);

        loading = false;
        delay(200);
        drawShape(data);
      }
      else
      {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      https.end();
    }
    else
    {
      Serial.println("WiFi Disconnected");
    }
    lastTime = millis();
  }
}