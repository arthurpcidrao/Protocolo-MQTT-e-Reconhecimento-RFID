// Bibliotecas
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <WiFi.h>
#include <PubSubClient.h>

#include <Preferences.h>

// Definição do id que sera liberado o acesso

#define ID "33 A1 BA 0E"  // exemplo

#define LedVerde            32
#define LedVermelho         25
#define tranca              33
#define buzzer               4
#define SS_PIN               5
#define RST_PIN             15
#define MSG_BUFFER_SIZE     50

#define MAX_STRING          10

Preferences preferences;

MFRC522 mfrc522(SS_PIN, RST_PIN);   // define os pinos de controle do modulo de leitura de cartoes RFID
LiquidCrystal_I2C lcd(0x27, 16, 2); // define informacoes do lcd como o endereço I2C (0x27) e tamanho do mesmo

//BROKER MQTT
const char* mqtt_server = "test.mosquitto.org"; //servidor mqtt
WiFiClient espClient;
PubSubClient client(espClient);    
char msg[MSG_BUFFER_SIZE];

// wifi e senha da rede local
const char* ssid = "Cidrao family2g";  //motorola one vision_6109
const char* password = "07021970";  //12345678



class ArrayList {

  private:
    char vetor[10][10]; // vetor de 10 unidades e, em cada unidade, tem 10 espaços 'char' para ocupar
    int tamanho;

  public:

    // Construtor
    ArrayList(){
      tamanho = 0;
    }

    void add(const char* elemento){
      if (tamanho < 10){
        strcpy(vetor[tamanho], elemento);
        tamanho++;
        Serial.print("id cadastrado: ");
        Serial.println(elemento);
      }
      else{
        Serial.println("memória cheia!");
      }

      preferences.begin("rfid", false);

      for(int i = 0; i < MAX_STRING; i++) {
        String key = "str" + String(i);
        preferences.putString(key.c_str(), elemento);
      }

      preferences.end();
    }

    int size(){
      return tamanho;
    }

    bool remove(const char* elemento){
      int pos = -1;
      for (int i = 0; i < tamanho; i++){
        if (strcmp(vetor[i], elemento) == 0){
          pos = i;
          break;
        }
      }

      if (pos != -1){
        remove(pos);
        return true;
      }

      return false;
    }

    void remove(int pos){
      for (int i = pos; i < tamanho -1; i++){
        strcpy(vetor[i], vetor[i+1]);
      }
      tamanho--;
    }

    char* getId(int i){
      if (i >= 0 && i < tamanho){
        return vetor[i];
      }
      return "";
    }

};


ArrayList cadastroId;


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

}




void setup() {

  Serial.print("Conectando-se ao Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println(" Conectado!");

  SPI.begin();   // inicia a comunicacao SPI que sera usada para comunicacao com o mudulo RFID

  lcd.init();   // inicia o lcd
  lcd.backlight();

  mfrc522.PCD_Init();  //inicia o modulo RFID

  Serial.begin(115200);  // inicia a comunicacao serial com o computador na velocidade de 115200 baud rate

  Serial.println("RFID + ESP32");
  Serial.println("Passe alguma tag RFID para verificar o id da mesma.");  // tag = cartão

  // define alguns pinos como saida
  pinMode(LedVerde, OUTPUT);
  pinMode(LedVermelho, OUTPUT);
  pinMode(tranca, OUTPUT);
  pinMode(buzzer, OUTPUT);

  cadastroId.add(ID);

  conectarBroker();

}


void loop() {

  reconectarBroker();

  lcd.home();                // bota o cursor do lcd na posicao inicial
  lcd.print("Aguardando");   // imprime na primeira linha a string "Aguardando"
  lcd.setCursor(0,1);        // seta o cursor para a segunda linha
  lcd.print("Leitura RFID"); // mostra na tela a string "Leitura RFID"

  if (Serial.available() > 0){
    String string = "";
    string = Serial.readString();
    string.trim();
    cadastroId.add(string.c_str());
  }


  if ( ! mfrc522.PICC_IsNewCardPresent()) {
     return;                 // se nao tiver um cartao para ser lido recomeça o void loop
  }
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;                  //se nao conseguir ler o cartao recomeça o void loop tambem
  }

  String conteudo = "";      // cria uma string

  Serial.print("id da tag :"); //imprime na serial o id do cartao

  for (byte i = 0; i < mfrc522.uid.size; i++){       // faz uma verificacao dos bits da memoria do cartao
     //ambos comandos abaixo vão concatenar as informacoes do cartao...
     //porem os 2 primeiros irao mostrar na serial e os 2 ultimos guardarao os valores na string de conteudo para fazer as verificacoes
     Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
     Serial.print(mfrc522.uid.uidByte[i], HEX);
     conteudo.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " "));
     conteudo.concat(String(mfrc522.uid.uidByte[i], HEX));
  }
  Serial.println();
  conteudo.toUpperCase();                      // deixa as letras da string todas maiusculas

  bool liberado = false;

  for(int i = 0; i < cadastroId.size(); i++){

    if (strcmp(conteudo.substring(1).c_str(), cadastroId.getId(i)) == 0){ // verifica se o ID do cartao lido tem o mesmo ID do cartao que queremos liberar o acesso

      digitalWrite(LedVerde, HIGH);            // ligamos o led verde
      lcd.clear();                             // limpamos oque havia sido escrito no lcd
      lcd.print("Acesso Liberado");            // informamos pelo lcd que a tranca foi aberta

      digitalWrite(tranca, HIGH);              //abrimos a tranca por 5 segundos

      for(byte s = 5; s > 0; s--){             //vai informando ao usuario quantos segundos faltao para a tranca ser fechada
        lcd.setCursor(8,1);
        lcd.print(s);
        delay(1000);
      }

      digitalWrite(tranca, LOW);               // fecha a tranca
      digitalWrite(LedVerde, LOW);             // e desliga o led
      lcd.clear();                           // limpa os caracteres q estao escritos no lcd

      liberado = true;
      break;
    }
  }

  if (!liberado){
    digitalWrite(LedVermelho, HIGH);           // vamos ligar o led vermelho

    sprintf(msg, "%s", conteudo.substring(1));
    client.publish("/estacionamento/IDcadastrados", msg);


      for(byte s = 5; s > 0; s--){               // uma contagem / espera para poder fazer uma nova leitura

        lcd.clear();                           // limpa as informacoes que estao na tela
        lcd.home();                            // nota na posicao inicial
        lcd.print("Acesso negado");            // infoma ao usuario que ele nao tem acesso
        lcd.setCursor(8,1);                    // coloca o cursor na coluna 8 da linha 2
        lcd.print(s);                          // informa quantos segundos faltam para pode fazer uma nova leitura

        // faz o buzzer emitir um bip por segundo
          delay(800);
          digitalWrite(buzzer, HIGH);
          delay(200);
          digitalWrite(buzzer, LOW);
      }
          digitalWrite(LedVermelho, LOW);         // desliga o led vermelho
          lcd.clear();                            // limpa as informacoes do lcd
  }
 // recomeça
}