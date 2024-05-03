// Bibliotecas
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Definição do id que sera liberado o acesso

#define ID "33 A1 BA 0E"

#define LedVerde 32
#define LedVermelho 25
#define tranca 33
#define buzzer 4
#define SS_PIN 5
#define RST_PIN 15

MFRC522 mfrc522(SS_PIN, RST_PIN);   // define os pinos de controle do modulo de leitura de cartoes RFID
LiquidCrystal_I2C lcd(0x27, 16, 2); // define informacoes do lcd como o endereço I2C (0x27) e tamanho do mesmo



String identifier[10];

void setup()
{
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

  identifier[0] = ID;
}

int contadorId = 1;

void loop() {

  lcd.home();                // bota o cursor do lcd na posicao inicial
  lcd.print("Aguardando");   // imprime na primeira linha a string "Aguardando"
  lcd.setCursor(0,1);        // seta o cursor para a segunda linha
  lcd.print("Leitura RFID"); // mostra na tela a string "Leitura RFID"

  if (Serial.available() > 0){
    String string = "";
    string = Serial.readString();
    string.trim();
    identifier[contadorId] = string;
    Serial.println("id cadastrado: "string);
    contadorId++;
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

  int count = 0;
  for(int i = 0; i < 10; i++){
    if (conteudo.substring(1) == identifier[i]){ // verifica se o ID do cartao lido tem o mesmo ID do cartao que queremos liberar o acesso

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

        count = 0;
        break;

    }else{                                       // caso o cartao lido nao foi registrado

      count++;
      }
  }

  if (count > 0){
    digitalWrite(LedVermelho, HIGH);           // vamos ligar o led vermelho

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
