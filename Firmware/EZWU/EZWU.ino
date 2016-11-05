
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h> 
#include <SD.h>
#include <Servo.h>
#include <SPI.h>

#define MODE_SW 13//Interruptor de modo: 0 config, 1 operación
#define ALM_BT 12    //Botón de apagado de alarma (podría ser RST)
#define SRV_L 5     //Servo izquierdo
#define SRV_R 4     //Servo derecho
//#define TRIG 3      //Señal trigger para sensor ultrasónico
#define ECHO 3      //Señal echo de sensor ultrasónico
#define BUZZER 15   //Señal de control para el buzzer

#define SLEEP_TIME 60 //Tiempo en segundos para el estado de bajo consumo
#define DEBUG   //Establece si se imprimen mensajes de depuración en GPIO2

Servo servo_l;    //Objeto servo corespondiente a la rueda izquierda
Servo servo_r;    //Objeto servo corespondiente a la rueda derecha

WiFiClient client;   //Cliente Wi-Fi para las solicitudes 
ESP8266WebServer server(80);  //Instancia del servidor local

//Organiza la configuración con el fin de cargar/guardar más fácilmente
struct dev_mem
{
    char ssid[33];    //SSID de red para el dispositivo
    char pass[65];    //Contraseña de la red
    char ssid_ap[33];   //SSID de red para el dispositivo
    char pass_ap[65];   //Contraseña de la red
    uint16_t smoke_thresh; //Valor umbral para la concentración de humo
    uint16_t dist_thresh; //Valor umbral para la distancia a un obstáculo
    uint16_t alarm_time; //Hora de la alarma en minutos transcurridos desde 00:00 (60*h + m)
} dev_config;

unsigned long t0;     //Temporizador general del sistema
unsigned int rand_delay;   //Valor aleatorio para controlar el movimiento del dispositivo
bool los_blocked = false;   //Estado de LOS
bool rotate = false;        //Bandera para la rotación

void setup(){
  //default_config();
  
  dev_setup();   //Inicialización de hardware
  eeprom_load_config();
  
  //Entra a modo config si GPIO2 está en alto
  if(!digitalRead(MODE_SW)){
    //Modo config
    #ifdef DEBUG
      Serial1.println("Modo Config");
    #endif
    server_setup();
    server_loop();
    
  } else {
    yield();
    #ifdef DEBUG
      Serial1.println("Modo Operacion");
    #endif
    
    //Lectura del ADC (sensor de humo)
    uint16_t smoke_adc = analogRead(A0);
    
    #ifdef DEBUG
      Serial1.print("ADC: ");
      Serial1.println(smoke_adc);
    #endif
    
    //Si se rompe el umbral se sale de la función setup para continuar con loop 
    if(mq_read() > dev_config.smoke_thresh){
      #ifdef DEBUG
        Serial1.println("Alarma por humo");
      #endif
      timer_reset();
      return;
    }
    
    #ifdef DEBUG
      Serial1.println("Conectando a la red");
    #endif
      //Se conecta a la red WiFi
      connect_wifi();
      
      //Se realiza request HEAD a google.com para obtener la hora
      String time_resp = "";
      if(get_time(&time_resp)){
        
        //Se parsea la hora de la respuesta de la solicitud a formato 60*h + m
        int gmt_time = parse_time(time_resp);
        gmt_time -= 360;
        #ifdef DEBUG
          Serial1.print("Hora formateada: ");
          Serial1.println(gmt_time);
        #endif
  
        //Si es la hora programada se sale de la función setup para continuar con loop 
        if(gmt_time == dev_config.alarm_time){
          #ifdef DEBUG
            Serial1.println("Alarma por hora " + gmt_time);
          #endif
          timer_reset();
          //ESP.wdtEnable(1000);
          return;
        }
    }

    #ifdef DEBUG
      Serial1.println("Deep sleep");
      Serial1.println();
    #endif
    //Se entra en estado de bajo consumo durante el tiempo especificado
    ESP.deepSleep(SLEEP_TIME * 1000000);
    //ESP.restart();
  }
}

void loop(){
  unsigned long t = (millis() - t0);

  los_blocked = (read_ultrasonic() < dev_config.dist_thresh) ? true : false;
  digitalWrite(BUZZER, HIGH);
  if(!digitalRead(ALM_BT)){
    ESP.restart();
  }
  if(t < rand_delay && !rotate){
    if(!los_blocked){
      servo_r.writeMicroseconds(1800);
      servo_l.writeMicroseconds(1200);
    } else {
      servo_r.writeMicroseconds(1200);
      servo_l.writeMicroseconds(1200);  
    }
  } else {
    if(!rotate){
      rotate = true;
      timer_reset();
      return;
    }
    if(t < rand_delay){
      servo_r.writeMicroseconds(1800);
      servo_l.writeMicroseconds(1800);
    } else {
      rotate = false;
      timer_reset();
    }
  }
}

//-------------------------Misc------------------------------------------


void dev_setup(){
  //Línea de comunicación de depuración
  #ifdef DEBUG
    Serial1.begin(115200);
    Serial1.println("Init");
  #endif
  
  //Se colocan varios pines del sistema en el estado que se requiere
  //pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT_PULLUP);

  pinMode(MODE_SW, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(ALM_BT, INPUT_PULLUP);

  servo_l.attach(SRV_L);
  servo_r.attach(SRV_R);

  digitalWrite(BUZZER, LOW);
  servo_r.writeMicroseconds(1500);
  servo_l.writeMicroseconds(1500);
  //SD.begin(SS);
}

void timer_reset(){
  t0 = millis();
  randomSeed(millis()/2);
  rand_delay = random(1000,4000);
  #ifdef DEBUG
    Serial1.println("Timer reset");
    Serial1.print("Rand delay: ");
    Serial1.println(rand_delay);
  #endif
}

//Escribir algún mensaje en un archivo en la tarjeta SD
/*boolean write_log(String msg) {
  File file_out;
  file_out = SD.open("log.txt", FILE_WRITE);
  //Escribir solo si el archivo se abrió adecuadamente
  if (file_out) {
    #ifdef DEBUG
      Serial1.println("Escribiendo SD");
    #endif
    file_out.println(msg);
    file_out.close(); //Se cierra el archivo
    return true;
  } else {
    #ifdef DEBUG
      Serial1.println("Error SD");
    #endif
    return false;
  }
}
*/

//-------------------------Server------------------------------------------

/* Configurar los parámetros necesarios de la red */
void start_ap(){
  WiFi.mode(WIFI_AP);
  uint8_t mac[WL_MAC_ADDR_LENGTH];
  WiFi.softAPmacAddress(mac);
  WiFi.softAP(dev_config.ssid_ap, dev_config.pass_ap);
}

void server_setup() 
{
  delay(1000);
  start_ap();

  /* Se asocia cada solicitud con la función que la maneja */
  server.on("/alarm", set_alarm);
  server.on("/ssid", set_ssid);
  server.on("/password", set_password);
  server.on("/smoke", set_smoke);
  server.on("/ultrasonic", set_ultrasonic);
  server.on("/ap_name", set_ap_name);
  server.on("/ap_pass", set_ap_pass);
  /* Se inicia el servidor */
  server.begin();
}

void server_loop() {
  /*  Se encarga de manejar las solicitudes provenientes de los clientes.
   *  Para cada solicitud identifica la función que la maneja 
   */
  while(!digitalRead(MODE_SW)){
    server.handleClient();
    delay(50);
  }
  ESP.restart();
}

/* Los parámetros se obtienen del JSON de la solicitud con el método server.arg() */

void set_alarm()
{
  #ifdef DEBUG
      Serial1.println(server.arg(0));
    #endif
  dev_config.alarm_time = server.arg(0).toInt();
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_ssid()
{
  strcpy(dev_config.ssid, server.arg(0).c_str());
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_password()
{
  strcpy(dev_config.pass, server.arg(0).c_str());
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_ap_name()
{
  strcpy(dev_config.ssid_ap, server.arg(0).c_str());
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_ap_pass()
{
  strcpy(dev_config.pass_ap, server.arg(0).c_str());
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_smoke()
{
  dev_config.smoke_thresh = server.arg(0).toInt();
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

void set_ultrasonic()
{
  dev_config.dist_thresh = server.arg(0).toInt();
  server.send(200, "text/plain", "Cool!");
  eeprom_save_config();
}

//-------------------------EEPROM------------------------------------------

void eeprom_save_config(){
  EEPROM.begin(245);
  delay(10);
  EEPROM.put(0, dev_config);
  yield();
  EEPROM.end();
}

void eeprom_load_config(){
  EEPROM.begin(245);
  delay(10);
  EEPROM.get(0, dev_config);
  yield();
  EEPROM.end();

  #ifdef DEBUG
    Serial1.println("dev config");
    Serial1.println(dev_config.ssid);
    Serial1.println(dev_config.pass);
    Serial1.println(dev_config.ssid_ap);
    Serial1.println(dev_config.pass_ap);
    Serial1.println(dev_config.smoke_thresh);
    Serial1.println(dev_config.dist_thresh);
    Serial1.println(dev_config.alarm_time);
  #endif
}

void default_config(){
  strcpy(dev_config.ssid, "ACM1PT");
  strcpy(dev_config.pass, "pwSwYdoG6");
  strcpy(dev_config.ssid_ap, "EZWakeUp");
  strcpy(dev_config.pass_ap, "bulldozer");
  dev_config.smoke_thresh = 69;
  dev_config.dist_thresh = 30;
  dev_config.alarm_time = 450;
  
  eeprom_save_config();
}

//-------------------------Tiempo------------------------------------------

void connect_wifi()
{
  //Se coloca WiFi en modo STA o AP_STA
  WiFi.mode(WIFI_STA);

  //Se intenta iniciar la conexión con la red
  WiFi.begin(dev_config.ssid, dev_config.pass);

  //Se permite un máximo de 10 intentos
  int i = 0;
  while (WiFi.status() != WL_CONNECTED){
    i++;
    delay(100);
  }
  
  #ifdef DEBUG
    if(WiFi.status() == WL_CONNECTED){
      Serial1.println("Wifi conectado");
    }
  #endif
}

bool get_time(String *response) {

  //Se intenta conectar con el servidor y puerto dados
  if ( !client.connect("www.google.com", 80) ) {
    #ifdef DEBUG
      Serial1.println("No se puede conectar");
    #endif
    return false;
  }
  
  //Solicitud HEAD para obtener la hora
  client.println("HEAD / HTTP/1.1");
  client.println("Host: www.google.com");
  client.println();
  delay(200);

  //Se lee la respuesta del servidor
  uint8_t i = 0;
  while(client.available() && i < 64) {
    char c = client.read();
    response->concat(c);
    i++;
    delay(10);
  }
  return true;
}

int parse_time(String time_str){
  uint8_t time_index = time_str.indexOf("GMT")-9;
  uint8_t hour = time_str.substring(time_index, time_index + 2).toInt();
  uint8_t minute = time_str.substring(time_index + 3, time_index + 5).toInt();
  return 60*hour + minute;
}

//-------------------------Sensores------------------------------------------

int mq_read(){
  float smoke_curve[3] = {2.3,0.53,-0.44};
  float mq_r = 0; 
  
  for (int i = 0; i < 25; i++) {
    uint16_t raw_adc = analogRead(A0);
    float rs_tmp = 10*(1023-raw_adc);
    rs_tmp = rs_tmp/raw_adc;
    mq_r += rs_tmp;
    delay(10);
  }

  mq_r = mq_r/25;
  mq_r = mq_r/0.6;

  int smoke_ppm = pow(10,(((log(mq_r)-smoke_curve[1])/smoke_curve[2]) + smoke_curve[0]));
  
  #ifdef DEBUG
        Serial1.print("Smoke ppm: ");
        Serial1.println(smoke_ppm);
  #endif

  //String log_str = "Se ha detectado " + String(smoke_ppm) + " ppm de humo";
  //write_log(log_str);
  
  return smoke_ppm;  
}

uint16_t read_ultrasonic(){
  unsigned long tof = pulseIn(ECHO, HIGH);
  uint16_t dist_cm = tof/58;
  #ifdef DEBUG
        Serial1.print("Dist: ");
        Serial1.println(dist_cm);
  #endif
  return dist_cm;
}

