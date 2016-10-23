#include <ESP8266WiFi.h>
#include <WiFiClient.h> 
#include <ESP8266WebServer.h>

/* SSID y contraseña de la red */
const char *ssid = "EZWakeUp";
const char *password = "bulldozer";

ESP8266WebServer server(80);  // Instancia del servidor

void setup() 
{
  delay(1000);
  Serial.begin(115200);
  Serial.println();
  Serial.print("Configurando AP...");

  /* Se configura el nombre y la contraseña del punto de acceso */
  WiFi.softAP(ssid, password);

  /* Se configura el IP */
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);

  /* Se asocia cada solicitud con la función que la maneja */
  server.on("/alarm", set_alarm);
  server.on("/network", set_network);
  server.on("/smoke", set_smoke);
  server.on("/ultrasonic", set_ultrasonic);

  /* Se inicia el servidor */
  server.begin();
}

/* Los parámetros se obtienen del JSON de la solicitud con el método server.arg() */

void set_alarm()
{
  String alarm_time = server.arg("time");
  Serial.println(alarm_time);
}

void set_network()
{
  String _ssid = server.arg("ssid");
  Serial.println(_ssid);
  String _password = server.arg("password");
  Serial.println(_password);
}

void set_smoke()
{
  String smoke_thresh = server.arg("threshold");
  Serial.println(smoke_thresh);
}

void set_ultrasonic()
{
  String ultrasonic_thresh = server.arg("threshold");
  Serial.println(ultrasonic_thresh);
}

void loop() 
{
  /*  Se encarga de manejar las solicitudes provenientes de los clientes.
   *  Para cada solicitud identifica la función que la maneja 
   */
  server.handleClient();
}
