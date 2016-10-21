#include <ESP8266WiFi.h>

#define MODE_SW 2   //Interruptor de modo: 1 config, 0 operación
#define ALM_BT 0    //Botón de apagado de alarma (podría ser RST)
#define SRV_L 5     //Servo izquierdo
#define SRV_R 4     //Servo derecho
#define TRIG 7      //Señal trigger para sensor ultrasónico
#define ECHO 8      //Señal echo de sensor ultrasónico
#define BUZZER 15   //Señal de control para el buzzer

#define DEBUG

WiFiClient client;

char ssid[33] = "OP";   //SSID de red para el dispositivo
char pass[33] = "pass"; //Contraseña de la red

uint8_t sleep_time = 60;  //Tiempo en segundos para el estado de bajo consumo

uint16_t smoke_thresh = 900;  //Valor umbral para la concentración de humo

uint16_t alarm_time = 450; //Hora de la alarma en minutos transcurridos desde 00:00 (60*h + m)

void setup(){
  init();   //Inicialización de hardware

  //Entra a modo config si GPIO2 está en alto
  if(digitalRead(MODE_SW)){
    //Modo config
  } else {
    
    //Lectura del ADC (sensor de humo)
    uint16_t smoke_adc = analogRead(A0);
    
    //Si se rompe el umbral se sale de la función setup para continuar con loop 
    if(smoke_adc > smoke_thresh){
      return;
    }

    //Se conecta a la red WiFi
    connect_wifi();
    
    //Se realiza request HEAD a google.com para obtener la hora
    String time_resp = "";
    if(get_time(&time_resp)){
      
      //Se parsea la hora de la respuesta de la solicitud a formato 60*h + m
      int gmt_time = parse_time(time_resp);

      //Si es la hora programada se sale de la función setup para continuar con loop 
      if(gmt_time == alarm_time){
        return
      }
    }

    //Se entra en estado de bajo consumo durante el tiempo especificado
    ESP.deepSleep(sleep_time * 1000000);
  }
}

void loop(){
  
}

void init(){
  //Se colocan varios pines del sistema en el estado que se requiere
  pinMode(MODE_SW, INPUT);
  pinMode(ALM_BT, INPUT);
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);
  pinMode(BUZZER, OUTPUT);

  //Línea de comunicación de depuración
  #ifdef DEBUG
    Serial1.begin(9600);
  #endif
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
  }
  
  return true;
}

int parse_time(String time_str){
  uint8_t time_index = time_str.indexOf("GMT")-9;
  uint8_t hour = time_str.substring(time_index, time_index + 2).toInt();
  uint8_t minute = time_str.substring(time_index + 2, time_index + 4).toInt();
  return 60*hour + minute;
}

