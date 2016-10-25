#include <Servo.h>
#include <ESP8266WiFi.h>

#define MODE_SW 2   //Interruptor de modo: 0 config, 1 operación
#define ALM_BT 0    //Botón de apagado de alarma (podría ser RST)
#define SRV_L 5     //Servo izquierdo
#define SRV_R 4     //Servo derecho
#define TRIG 1      //Señal trigger para sensor ultrasónico
#define ECHO 3      //Señal echo de sensor ultrasónico
#define BUZZER 15   //Señal de control para el buzzer

#define DEBUG

Servo servo_l;
Servo servo_r;
WiFiClient client;


char ssid[33] = "OP";   //SSID de red para el dispositivo
char pass[33] = "pass"; //Contraseña de la red

uint8_t sleep_time = 60;  //Tiempo en segundos para el estado de bajo consumo

uint16_t smoke_thresh = 900;  //Valor umbral para la concentración de humo

uint16_t dist_thresh = 30;  //Valor umbral para la concentración de humo

uint16_t alarm_time = 450; //Hora de la alarma en minutos transcurridos desde 00:00 (60*h + m)

unsigned long t0;
unsigned int rand_delay;

void setup(){
  //ESP.wdtEnable(2000);
  //yield();
  dev_setup();   //Inicialización de hardware
  
  #ifdef DEBUG
      Serial1.println("Check 3");
   #endif
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
    if(smoke_adc > smoke_thresh){
      #ifdef DEBUG
        Serial1.println("Alarma por humo");
      #endif
      timer_reset();
      return;
    } else {

      //Se conecta a la red WiFi
      connect_wifi();
      
      //Se realiza request HEAD a google.com para obtener la hora
      String time_resp = "";
      if(get_time(&time_resp)){
        
        #ifdef DEBUG
          Serial1.print("HEAD: ");
          Serial1.println(time_resp);
        #endif
        
        //Se parsea la hora de la respuesta de la solicitud a formato 60*h + m
        int gmt_time = parse_time(time_resp);
        
        #ifdef DEBUG
          Serial1.print("Hora formateada: ");
          Serial1.println(gmt_time);
        #endif
  
        //Si es la hora programada se sale de la función setup para continuar con loop 
        if(gmt_time == alarm_time){
          #ifdef DEBUG
            Serial1.println("Alarma por hora");
          #endif
          timer_reset();
          //ESP.wdtEnable(1000);
          return;
        }
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
}

void timer_reset(){
  t0 = millis();
  randomSeed(analogRead(A0));
  rand_delay = random(1000,4000);
}

void loop(){
  unsigned long t = (millis() - t0);
  
  bool los_blocked = false;
  bool buzzer_on = false;
  bool rotate = false;
  
  los_blocked = (read_ultrasonic() < dist_thresh) ? false : true;
  buzzer_on = (t%450 < 225) ? true : false;
  digitalWrite(BUZZER, buzzer_on);

  if(t < rand_delay && !rotate){
    if(!los_blocked){
      servo_l.writeMicroseconds(1700);
      servo_r.writeMicroseconds(1300);
    } else {
      servo_l.writeMicroseconds(1400);
      servo_r.writeMicroseconds(1400);  
    }
  } else {
    if(!rotate){
      timer_reset();
      rotate = true;
      return;
    }
    if(t < rand_delay){
      servo_l.writeMicroseconds(1700);
      servo_r.writeMicroseconds(1700);
    } else {
      rotate = false;  
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

  #ifdef DEBUG
    Serial1.println("Check 1");
  #endif
  
  //pinMode(MODE_SW, INPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(ALM_BT, INPUT);

  #ifdef DEBUG
    Serial1.println("Check 2");
  #endif
  servo_l.attach(SRV_L);
  servo_r.attach(SRV_R);
}

void connect_wifi()
{
  //Se coloca WiFi en modo STA or AP_STA
  WiFi.mode(WIFI_STA);

  //Se intenta iniciar la conexión con la red
  WiFi.begin(ssid, pass);

  //Se permite un máximo de 10 intentos
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 10){
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
    return false;
  }

  //Solicitud HEAD para obtener la hora
  client.println(F("HEAD / HTTP/1.1"));
  client.println(F("Host: www.google.com"));
  client.println(F("Connection: close"));
  client.println();
  delay(10);

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
  uint8_t minute = time_str.substring(time_index + 2, time_index + 4).toInt();
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

