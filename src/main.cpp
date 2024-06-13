#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "SPIFFS.h"
#include <Preferences.h>

const int ledPin = 2;
AsyncWebServer webserver(80);

Preferences prefs;

TaskHandle_t blinkTask_h;

static void blinkTask(void *parameter)
{
  while (1)
  {
    digitalWrite(ledPin, HIGH);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    digitalWrite(ledPin, LOW);
    vTaskDelay(450 /portTICK_PERIOD_MS);
    Serial.print(millis());
    Serial.println("   Blinktask");
  }
}

static void web(void *parameter)
{
  while (1)
  {
    yield();
  }
}

bool initWiFi(String ssid, String pass)
{
  unsigned long previousMillis = 0;
  const long interval = 10000;

  if (ssid == "")
  {
    Serial.println("SSID ist leer");
    return false;
  }
  if (pass.length() < 8)
  {
    Serial.println("WLAN-Passwort ist < 8 Zeichen");
    return false;
  }

  WiFi.mode(WIFI_STA);

  WiFi.begin(ssid.c_str(), pass.c_str());
  Serial.println("Verbinde mit WLAN...");

  unsigned long currentMillis = millis();
  previousMillis = currentMillis;

  while (WiFi.status() != WL_CONNECTED)
  {
    currentMillis = millis();
    if (currentMillis - previousMillis >= interval)
    {
      Serial.println("Fehler bei der Verbindung.");
      return false;
    }
  }
  Serial.println("IP-Adresse: ");
  Serial.println(WiFi.localIP());
  return true;
}

// Funktion um Werte in der HTML-Datei auszutauschen
String processor(const String &var)
{
  String ledState;
  if (var == "STATE")
  {
    if (digitalRead(ledPin))
    {
      ledState = "AN";
    }
    else
    {
      ledState = "AUS";
    }
    return ledState;
  }

  if (var == "MAC")
  {
    return WiFi.macAddress();
  }

  if (var == "SSID")
  {
    return prefs.getString("ssid");
  }
  if (var == "PASS")
  {
    return prefs.getString("pass");
  }

  return String();
}

void setup()
{
  Serial.begin(115200);
  SPIFFS.begin();

  // Namespace für die Netzwerkeinstellungen erzeugen
  prefs.begin("network", false);

  // LED-PIN als Ausgang definieren und ausschalten
  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  // Wenn sich das WLAN initialisieren laesst, dann den Webserver zusammenbauen
  if (initWiFi(prefs.getString("ssid"), prefs.getString("pass")))
  {

    webserver.serveStatic("/", SPIFFS, "/");

    webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                 { request->send(SPIFFS, "/index.html", "text/html", false, processor); });

    webserver.on("/on", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
      digitalWrite(ledPin, HIGH);
      request->send(SPIFFS, "/index.html", "text/html", false, processor); });

    webserver.on("/off", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
      digitalWrite(ledPin, LOW);
      request->send(SPIFFS, "/index.html", "text/html", false, processor); });

    webserver.on("/api", HTTP_GET, [](AsyncWebServerRequest *request)
                 {
      request->send(SPIFFS, "/io.json", "application/json", false, processor); });

    webserver.begin();
  }

  // Wenn das nicht klappt, dann WLAN als Access Point starten
  else
  {
    Serial.println("Starte Access Point)");
    WiFi.softAP("RC-Controller", NULL);

    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Web Server Root URL
    webserver.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                 { request->send(SPIFFS, "/settings.html", "text/html", false, processor); });

    webserver.serveStatic("/", SPIFFS, "/");

    webserver.on("/", HTTP_POST, [](AsyncWebServerRequest *request)
                 {
      int params = request->params();
      for(int i=0;i<params;i++){
        AsyncWebParameter* p = request->getParam(i);
        if(p->isPost()){
          if (p->name() == "ssid") {
            prefs.putString("ssid",p->value().c_str());
          }
          if (p->name() == "pass") {
            prefs.putString("pass",p->value().c_str());
          }
        }
      }
      request->send(200, "text/plain", "Erledigt. ESP startet neu");
      delay(3000);
      ESP.restart(); });
    webserver.begin();
  }

}

void loop()
{
}