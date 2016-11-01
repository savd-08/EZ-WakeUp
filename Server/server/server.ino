#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

/* SSID y contraseña de la red */
String AP_NameString = "EZWakeUp";
const char WiFiAPPSK[] = "bulldozer";

ESP8266WebServer server(80);  // Instancia del servidor

const int LED_PIN = 5; // LED integrado en el Thing

void setup() 
{
  delay(1000);
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.println();
  Serial.println("Configurando AP...");

  setupWiFi();

  /* Se asocia cada solicitud con la función que la maneja */
  server.on("/alarm", set_alarm);
  server.on("/network", set_network);
  server.on("/smoke", set_smoke);
  server.on("/ultrasonic", set_ultrasonic);

  /* Se inicia el servidor */
  server.begin();
}

void loop() 
{
  /*  Se encarga de manejar las solicitudes provenientes de los clientes.
   *  Para cada solicitud identifica la función que la maneja 
   */
  server.handleClient();
}

/* Configurar los parámetros necesarios de la red */
void setupWiFi()
{
  WiFi.mode(WIFI_AP);

  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  
  char AP_NameChar[AP_NameString.length() + 1];
  memset(AP_NameChar, 0, AP_NameString.length() + 1);

  for (int i = 0; i < AP_NameString.length(); i++)
  {
    AP_NameChar[i] = AP_NameString.charAt(i);    
  }
  
  WiFi.softAP(AP_NameChar, WiFiAPPSK);
}

/* Los parámetros se obtienen del JSON de la solicitud con el método server.arg() */

void set_alarm()
{
  int alarm_time = server.arg(0).toInt();
  Serial.print("Alarma: ");
  Serial.println(alarm_time, DEC);
  digitalWrite(LED_PIN, HIGH);
  server.send(200, "text/plain", "Cool!");
}

void set_network()
{
  String _ssid = server.arg(0);
  Serial.print("SSID: ");
  Serial.println(_ssid);
  String _password = server.arg(1);
  Serial.print("Password: ");
  Serial.println(_password);
  server.send(200, "text/plain", "Cool!");
}

void set_smoke()
{
  int thresh = server.arg(0).toInt();
  Serial.print("Threshold: ");
  Serial.println(thresh);
  digitalWrite(LED_PIN, LOW);
  server.send(200, "text/plain", "Cool!");
}

void set_ultrasonic()
{
  int thresh = server.arg(0).toInt();
  Serial.print("Threshold: ");
  Serial.println(thresh);
  server.send(200, "text/plain", "Cool!");
}
