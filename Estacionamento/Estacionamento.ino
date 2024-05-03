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

#define PIN_ECHO_R          12
#define PIN_TRIG_R          13
#define LEDG_RIGHT          27
#define LEDR_RIGHT          26
#define PIN_ECHO_L          32
#define PIN_TRIG_L          33
#define LEDG_LEFT           14
#define LEDR_LEFT           25
#define IN_SERVO            18
#define OUT_SERVO           15
#define IN_BTN              34
#define OUT_BTN             35
#define BUZZER               4
#define EMERGENCY            5
#define MSG_BUFFER_SIZE     50
#define WAIT_TIME          500
#define TIME_TO_SEND      2000

bool emergency_btn = 0;

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
const char* ssid = "Wokwi-GUEST";
const char* password = "";



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
    
    if (servoInDown){
      cars_in++;
      servo_in.write(0);
      delay(500);
      servo_in.write(90);
    }

    sprintf(msg, "%i", cars_in);
    client.publish("/estacionamento/dentro", msg);
  }

  if ((char)payload[0] == 's') {
    //saiu um carro
    if (servoOutDown && cars_in > 0){
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

  if ((char)payload[0] == 'a') {
    //botão de emergência
    count_emergency++;
    if (count_emergency%2 == 0){
      lcd.setCursor(0, 3);
      lcd.print("         ");
      client.publish("/estacionamento/emergencia", "");
      noTone(BUZZER);
      servo_in.write(0);
      servo_out.write(0);
      count_emergency = 0;
    }
    else{
      tone(BUZZER, 220);
      lcd.setCursor(0, 3);
      lcd.print("EMERGENCY");
      client.publish("/estacionamento/emergencia", "EMERGÊNCIA");
      servo_in.write(0);
      servo_out.write(0);
    }
  }
  
}



void setup() {
  Serial.begin(115200);

  Serial.print("Conectando-se ao Wi-Fi");
  WiFi.begin(ssid, password, 6);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");

  lcd.init();
  lcd.backlight();

  pinMode(BUZZER, OUTPUT);

  pinMode(PIN_TRIG_R, OUTPUT);
  pinMode(PIN_ECHO_R, INPUT);
  pinMode(PIN_TRIG_L, OUTPUT);
  pinMode(PIN_ECHO_L, INPUT);

  pinMode(LEDG_RIGHT, OUTPUT);
  pinMode(LEDR_RIGHT, OUTPUT);
  pinMode(LEDG_LEFT, OUTPUT);
  pinMode(LEDR_LEFT, OUTPUT);

  servo_in.attach(IN_SERVO);
  servo_out.attach(OUT_SERVO);

  servo_in.write(90);
  servo_out.write(90);

  conectarBroker();

}



void loop() {
  reconectarBroker();

  if (millis() - startTime >= WAIT_TIME){
    servo_in.write(90);
    servoInDown = true;
    previous_in_btn = state_in_btn;
  }

  emergency_btn = digitalRead(EMERGENCY);

  if (emergency_btn){
    tone(BUZZER, 220);
    lcd.setCursor(0, 3);
    lcd.print("EMERGENCY");
    delay(10);
    noTone(BUZZER);
    client.publish("/estacionamento/emergencia", "EMERGÊNCIA");
    servo_in.write(0);
    servo_out.write(0);
  }

  else{
    client.publish("/estacionamento/emergencia", "");
    lcd.setCursor(0, 3);
    lcd.print("         ");
    
    state_in_btn = digitalRead(IN_BTN);
    state_out_btn = digitalRead(OUT_BTN);

    if (state_in_btn != previous_in_btn){
      getInCar();

      sprintf(msg, "%i", cars_in);
      client.publish("/estacionamento/dentro", msg);
    }

    if (state_out_btn != previous_out_btn){
      getOutCar();

      sprintf(msg, "%i", cars_in);
      client.publish("/estacionamento/dentro", msg);

      sprintf(msg, "%.2f",count_out*10.0);
      client.publish("/estacionamento/dinheiro", msg);
    }

    count = ultrasonic(PIN_TRIG_R, PIN_ECHO_R, LEDR_RIGHT, LEDG_RIGHT) + 
    ultrasonic(PIN_TRIG_L, PIN_ECHO_L, LEDR_LEFT, LEDG_LEFT);

    if (millis() > auxTimer + TIME_TO_SEND){
      auxTimer = millis();
      sprintf(msg, "%i", count);   //monta string
      client.publish("/estacionamento/vagas", msg);   //publica a mensagem
    }

    updateLCD();
  }
}



inline int ultrasonic(int trig, int echo, int red, int green){
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  
  int distance = (pulseIn(echo,HIGH))/58.8;

  if (distance < 250){
    digitalWrite(red,HIGH);
    digitalWrite(green, LOW);
    return 0;
  }
  else {
    digitalWrite(red,LOW);
    digitalWrite(green, HIGH);
    return 1;
  }

}

inline void getInCar(){
  if (servoInDown){
    cars_in++;
    startTime = millis();
    servoInDown = false;
    servo_in.write(0);
  }

  if (millis() - startTime >= WAIT_TIME){
    servo_in.write(90);
    servoInDown = true;
    previous_in_btn = state_in_btn;
  }
}


inline void getOutCar(){
  if (servoOutDown && cars_in > 0){
    cars_in--;
    count_out++;
    endTime = millis();
    servoOutDown = false;
    servo_out.write(0);
  }
    
  if (millis() - endTime >= WAIT_TIME){
    servo_out.write(90);
    servoOutDown = true;
    previous_out_btn = state_out_btn;
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
