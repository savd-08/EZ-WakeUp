#include <Servo.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <SPI.h>
#include <SD.h>

#define MODE_SW 2   //Interruptor de modo: 0 config, 1 operación
#define ALM_BT 0    //Botón de apagado de alarma (podría ser RST)
#define SRV_L 5     //Servo izquierdo
#define SRV_R 4     //Servo derecho
#define TRIG 1      //Señal trigger para sensor ultrasónico
#define ECHO 3      //Señal echo de sensor ultrasónico
#define BUZZER 15   //Señal de control para el buzzer

#define DEBUG

const int chipSelect = 4;
Servo servo_l;
Servo servo_r;
WiFiClient client;

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

void save_config(){
  strcpy(dev_config.ssid, "CuartosMamaCoyita");
  strcpy(dev_config.pass, "CMC12345");
  strcpy(dev_config.ssid_ap, "Mefisto");
  strcpy(dev_config.pass_ap, "bulldozer");
  dev_config.smoke_thresh = 512;
  dev_config.dist_thresh = 30;
  dev_config.alarm_time = 450;
  
  EEPROM.begin(245);
  delay(10);
  EEPROM.put(0, dev_config);
  yield();
  EEPROM.end();
}

void load_config(){
  EEPROM.begin(245);
  delay(10);
  EEPROM.get(0, dev_config);
  yield();
  EEPROM.end();
}

uint8_t sleep_time = 60;  //Tiempo en segundos para el estado de bajo consumo
unsigned long t0;
unsigned int rand_delay;
bool los_blocked = false;
bool rotate = false;

void setup(){
  //save_config();
  load_config();
  dev_setup();   //Inicialización de hardware
  
  //Entra a modo config si GPIO2 está en alto
  if(false){
    //Modo config
  } else {
    yield();
    #ifdef DEBUG
      Serial1.println("Modo Operación");
    #endif
    
    //Lectura del ADC (sensor de humo)
    uint16_t smoke_adc = analogRead(A0);
    
    #ifdef DEBUG
      Serial1.print("ADC: ");
      Serial1.println(smoke_adc);
    #endif
    
    //Si se rompe el umbral se sale de la función setup para continuar con loop 
    if(smoke_adc > dev_config.smoke_thresh){
      #ifdef DEBUG
        Serial1.println("Alarma por humo");
      #endif
      escrituraSD();
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
        
        #ifdef DEBUG
          Serial1.print("Hora formateada: ");
          Serial1.println(gmt_time);
        #endif
  
        //Si es la hora programada se sale de la función setup para continuar con loop 
        if(gmt_time == dev_config.alarm_time){
          #ifdef DEBUG
            Serial1.println("Alarma por hora");
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
    //ESP.deepSleep(sleep_time * 1000000);
    ESP.restart();
  }

  // Abrimos la comunicación serial:
  Serial.begin(9600);
  while (!Serial) {
    ; // esperamos a que el puerto abra
  }
}

void timer_reset(){
  t0 = millis();
  randomSeed(analogRead(A0));
  rand_delay = random(1000,4000);
  #ifdef DEBUG
    Serial1.println("Timer reset");
    Serial1.print("Rand delay: ");
    Serial1.println(rand_delay);
  #endif
}

void loop(){
  unsigned long t = (millis() - t0);

  los_blocked = (read_ultrasonic() < dev_config.dist_thresh) ? true : false;
  digitalWrite(BUZZER, HIGH);

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

void dev_setup(){
  //Línea de comunicación de depuración
  #ifdef DEBUG
    Serial1.begin(115200);
    Serial1.println("Init");
  #endif
  
  //Se colocan varios pines del sistema en el estado que se requiere
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  //pinMode(MODE_SW, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(ALM_BT, INPUT);

  servo_l.attach(SRV_L);
  servo_r.attach(SRV_R);

  digitalWrite(BUZZER, LOW);
  servo_r.writeMicroseconds(1500);
  servo_l.writeMicroseconds(1500);
}

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

uint16_t read_ultrasonic(){
  unsigned long tof = pulseIn(ECHO, HIGH);
  uint16_t dist_cm = tof/58;
  #ifdef DEBUG
        Serial1.print("Dist: ");
        Serial1.println(dist_cm);
  #endif
  return dist_cm;
}

void escrituraSD() {
  // verificamos que la SD esté conectada e inicializamos:
  SD.begin(chipSelect);
  
  // String para guardar lo que se escribe en la SD:
  String dataString = "";
  
  // lectura del sensor:
  uint16_t sensor = analogRead(A0);
  dataString += String(sensor);

  // Abrir el archivo, solamente se puede tener un archivo abierto a la vez
  // por que lo que hay que cerrarlo cada vez que se quiere abrir de nuevo.
  File dataFile = SD.open("datalog.txt", FILE_WRITE);

  // si está disponible, sobreescribimos:
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    // print al puerto serial:
    //Serial.println(dataString);
  }
  // si no se abre el archivo podemos generar un pop up:
  else {
    Serial.println("error opening datalog.txt");
  }
}
