#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WiFi.h> 
#include <PubSubClient.h>

#define LedVerde 32
#define LedVermelho 25
#define tranca 33
#define buzzer 4
#define SS_PIN 5
#define RST_PIN 15
#define MSG_BUFFER_SIZE     50

Preferences preferences;
#define MAX_STRING 10
int contadorId = 0;
int cars_in = 0;
int count_out = 0;

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);
String identifier[MAX_STRING] = {"", "", "", "", "", "", "", "", "", ""};

const char* ssid = "Connectify-arthur";
const char* password = "12345678a";
const char* mqtt_server = "test.mosquitto.org";

WiFiClient espClient;
PubSubClient client(espClient);
char msg[MSG_BUFFER_SIZE];

void setup() {
  setup_wifi();
  setupMQTT();
  SPI.begin();
  lcd.init();
  lcd.backlight();
  mfrc522.PCD_Init();
  Serial.begin(115200);
  Serial.println("RFID + ESP32");
  Serial.println("Passe alguma tag RFID para verificar o id da mesma.");
  loadString();
  pinMode(LedVerde, OUTPUT);
  pinMode(LedVermelho, OUTPUT);
  pinMode(tranca, OUTPUT);
  pinMode(buzzer, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  client.subscribe("rfid/brauleadicionarId");
  client.subscribe("rfid/braulelimpaIDs");
  client.subscribe("estacionamento/btn_entra");
  client.subscribe("estacionamento/btn_saida");

  lcd.home();
  lcd.print("Aguardando");
  lcd.setCursor(0,1);
  lcd.print("Leitura RFID");

  if (Serial.available() > 0) {
    String string = Serial.readString();
    string.trim();
    if(string == "limpar") {
      clearStringArray();
    } else {
      while(true) {
        if(identifier[contadorId].length() > 0) {
          contadorId++;
        } else {
          identifier[contadorId] = string;
          Serial.println("id cadastrado: " + string);
          contadorId++;
          saveString();
          break;
        }
      }
    }
  }

  if (!mfrc522.PICC_IsNewCardPresent()) {
     return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String conteudo = "";
  Serial.print("id da tag :");
  for (byte i = 0; i < mfrc522.uid.size; i++) {
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  conteudo.toUpperCase();
  client.publish("rfid/lastID", conteudo.c_str());

  int count = 0;
  for(int i = 0; i < 10; i++) {
    if (conteudo.substring(1) == identifier[i]) {
        digitalWrite(LedVerde, HIGH);
        lcd.clear();
        lcd.print("Acesso Liberado");
        client.publish("rfid/acessopermitido", "Acesso Permitido");
        digitalWrite(buzzer, HIGH);
        delay(1000);
        digitalWrite(buzzer, LOW);
        digitalWrite(tranca, HIGH);
        for(byte s = 5; s > 0; s--) {
          lcd.setCursor(8,1);
          lcd.print(s);
          delay(1000);
        }
        digitalWrite(tranca, LOW);
        digitalWrite(LedVerde, LOW);
        lcd.clear();
        count = 0;
        cars_in++;
        sprintf(msg, "%i", cars_in);
        client.publish("/estacionamento/dentro", msg);
        break;
    } else {
      count++;
    }
  }

  if (count > 0) {
    digitalWrite(LedVermelho, HIGH);
    for(byte s = 5; s > 0; s--) {
      lcd.clear();
      lcd.home();
      lcd.print("Acesso negado");
      lcd.setCursor(8,1);
      lcd.print(s);
      client.publish("rfid/acessonegado", "Acesso Negado");
      delay(800);
      digitalWrite(buzzer, HIGH);
      delay(200);
      digitalWrite(buzzer, LOW);
    }
    digitalWrite(LedVermelho, LOW);
    lcd.clear();
  }
}

void setup_wifi() {
  delay(10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void setupMQTT() {
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  client.subscribe("rfid/brauleadicionarId");
  client.subscribe("rfid/braulelimpaIDs");
}

void callback(char* topic, byte* payload, unsigned int length) {
  String newID = "";
  for (int i = 0; i < length; i++) {
    newID += (char)payload[i];
  }
  if (String(topic) == "rfid/brauleadicionarId") {
    if (newID.length() > 0) {
      if (contadorId < MAX_STRING) {
        identifier[contadorId] = newID;
        Serial.println("Novo ID cadastrado: " + newID);
        contadorId++;
        saveString();
      } else {
        Serial.println("Erro: Limite máximo de IDs atingido.");
      }
    } else {
      Serial.println("Erro: ID inválido.");
    }
  } 
  
  if (String(topic) == "rfid/braulelimpaIDs") {
    clearStringArray();
    Serial.println("Todos os IDs foram removidos.");
  }

  if ((char)payload[0] == 'e') {
    //entrou um carro
    
    cars_in++;

    sprintf(msg, "%i", cars_in);
    client.publish("/estacionamento/dentro", msg);
  }

  if ((char)payload[0] == 's') {
    //saiu um carro
    
    cars_in--;
    count_out++;

    sprintf(msg, "%i", cars_in);
    client.publish("/estacionamento/dentro", msg);

    sprintf(msg, "%.2f",count_out*10.0);
    client.publish("/estacionamento/dinheiro", msg);
  }
}

void reconnect() {
 while (!client.connected()) {
   String clientId = "ESP32Client-";
   clientId += String(random(0xffff), HEX);
   if (client.connect(clientId.c_str())) {
     client.publish("/ThinkIOT/Publish", "Welcome");
     client.subscribe("/ThinkIOT/Subscribe");
   } else {
     delay(2000);
   }
  }
}

void saveString() {
  preferences.begin("rfid", false);
  for(int i = 0; i < MAX_STRING; i++) {
    String key = "str" + String(i);
    preferences.putString(key.c_str(), identifier[i]);
  }
  preferences.end();
}

void loadString(){
  preferences.begin("rfid", true);
  for(int i = 0; i < MAX_STRING; i++) {
    String key = "str" + String(i);
    identifier[i] = preferences.getString(key.c_str(), "");
  }
  preferences.end();
}

void clearStringArray() {
    preferences.begin("rfid", false);
    for (int i = 0; i < MAX_STRING; i++) {
        String key = "str" + String(i);
        preferences.remove(key.c_str());
        identifier[i] = "";
    }
    preferences.end();
    Serial.println("Strings removidas com sucesso da memória NVS!");
}