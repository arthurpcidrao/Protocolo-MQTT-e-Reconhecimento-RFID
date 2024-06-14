// um andar tem uma altura de 3m, aproximadamente. No estacionamento é maior.
// vamos considerar um teto de 3,5m e com isso o sensor ficará a 3 m do chão.

#include <ESP32Servo.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <PubSubClient.h>

#define I2C_ADDR          0x27
#define LCD_COLUMNS         20
#define LCD_LINES            4

LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_LINES);

#define LIGHT_SENSOR_L      34 // ok
#define LIGHT_SENSOR_R      35 // ok
#define LEDG_RIGHT          27 // ok
#define LEDR_RIGHT          26 // ok
#define LEDG_LEFT           14 // ok
#define LEDR_LEFT           25 // ok
#define IN_SERVO            18 // ok
#define OUT_SERVO           15 // ok
#define IN_BTN              34
#define OUT_BTN             35
#define MSG_BUFFER_SIZE     50
#define WAIT_TIME          500
#define TIME_TO_SEND      2000


bool state_in_btn = 0, previous_in_btn = 0;
bool state_out_btn = 0, previous_out_btn = 0;
bool servoInDown = true;
bool servoOutDown = true;

unsigned long startTime = 0;
unsigned long endTime = 0;
unsigned long auxTimer = 0;

Servo servo_in;
Servo servo_out;

int count = 0, cars_in = 0, count_out = 0;
int count_emergency = 0;


//BROKER MQTT
const char* mqtt_server = "test.mosquitto.org"; //servidor mqtt
WiFiClient espClient;
PubSubClient client(espClient);    
char msg[MSG_BUFFER_SIZE];

// wifi e senha da rede local
const char* ssid = "Connectify-arthur"; //Connectify-arthur
const char* password = "12345678a"; //12345678a



void conectarBroker() {         
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  Serial.print("Conectando ao servidor MQTT...");
  if (!client.connected()) {
    Serial.print(".");
    String clientId = "";
    clientId += String(random(0xffff), HEX);
    if (client.connect(clientId.c_str())) {
      Serial.println("Conectado");
      client.subscribe("estacionamento/btn_entra");
      client.subscribe("estacionamento/btn_saida");
      client.subscribe("estacionamento/btn_emergencia");
    }
  }
}



void reconectarBroker() {
  if (!client.connected()) {
    conectarBroker();
  }
  client.loop();
}



void callback(char* topic, byte* payload, unsigned int lenght) {

  if ((char)payload[0] == 'e') {
    //entrou um carro
    
    cars_in++;
    servo_in.write(0);
    delay(500);
    servo_in.write(90);
    

    sprintf(msg, "%i", cars_in);
    client.publish("/estacionamento/dentro", msg);
  }

  if ((char)payload[0] == 's') {
    //saiu um carro
    if (cars_in > 0){
      cars_in--;
      count_out++;
      servo_out.write(0);
      delay(500);
      servo_out.write(90);
    }

    sprintf(msg, "%i", cars_in);
    client.publish("/estacionamento/dentro", msg);
    

    sprintf(msg, "%.2f",count_out*10.0);
    client.publish("/estacionamento/dinheiro", msg);
  }
  
}



void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();

  pinMode(LIGHT_SENSOR_L, INPUT);
  pinMode(LIGHT_SENSOR_R, INPUT);

  pinMode(LEDG_RIGHT, OUTPUT);
  pinMode(LEDR_RIGHT, OUTPUT);
  pinMode(LEDG_LEFT, OUTPUT);
  pinMode(LEDR_LEFT, OUTPUT);

  servo_in.attach(IN_SERVO);
  servo_out.attach(OUT_SERVO);

  servo_in.write(90);
  servo_out.write(90);

  Serial.print("Conectando-se ao Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");

  conectarBroker();

}



void loop() {
  reconectarBroker();

  count = sensor(LIGHT_SENSOR_L, LEDR_LEFT, LEDG_LEFT) + 
  sensor(LIGHT_SENSOR_R, LEDR_RIGHT, LEDG_RIGHT);

  if (millis() > auxTimer + TIME_TO_SEND){
    auxTimer = millis();
    sprintf(msg, "%i", count);   //monta string
    client.publish("/estacionamento/vagas", msg);   //publica a mensagem
  }

  updateLCD();
  
}



int sensor(int light_sensor, int red, int green){
  //Serial.println(analogRead(light_sensor));
  if (analogRead(light_sensor) > 600){
    digitalWrite(red,HIGH);
    digitalWrite(green, LOW);
    return 0;
  }
  else{
    digitalWrite(red,LOW);
    digitalWrite(green, HIGH);
    return 1;
  }
  

}



inline void updateLCD() {
    lcd.setCursor(0, 0);
    lcd.print("Dentro: ");
    lcd.setCursor(9, 0);
    lcd.print(" ");
    lcd.setCursor(8, 0);
    lcd.print(cars_in);

    lcd.setCursor(0, 1);
    lcd.print("Vagas disp.: ");
    lcd.print(count);

    lcd.setCursor(0, 2);
    lcd.print("Valor: R$ ");
    lcd.print(count_out * 10);
}