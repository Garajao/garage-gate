#include <AESLib.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <RTClib.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ArduinoJson.h>

#include <Servo.h>

#define EEPROM_SIZE 128
#define CODE_LENGTH 32
#define CODE_EXPIRE 10

#define SERVO_PORT 16
#define SERVO_DEGREES 180
#define LED_PORT 14

#define REQUEST_INTERVAL 2000

SoftwareSerial HC12(12, 13);
RTC_DS1307 RTC;
AES AES;

Servo servo;

// Code settings
extern String key;
extern String iv;

// Wi-fi settings
extern const char *SSID;
extern const char *PASS;

// Application settings
extern const char *server_name;
extern const char *token;
extern const char *gate_id;

unsigned long random_number, previous_time = 0;

void setup() {
  HC12.begin(9600);
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASS);
  EEPROM.begin(EEPROM_SIZE);

  pinMode(LED_PORT, OUTPUT);
  servo.attach(SERVO_PORT);

  if (!RTC.begin()) {
    Serial.println("DS1307 não encontrado");
    // RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    while (true) {}
  }

  if (getGate()) {
    servo.write(SERVO_DEGREES);
    digitalWrite(LED_PORT, HIGH);
  } else {
    servo.write(0);
    digitalWrite(LED_PORT, LOW);
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado | Endereço IP: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  unsigned long current_time = millis();

  if (current_time - previous_time > REQUEST_INTERVAL) {
    char path[200] = "", response[200] = "";
    strcpy(path, "/gates/");
    strcat(path, gate_id);

    requestGET(path, (char *)response);

    if (response[0] != '\0') {
      DynamicJsonDocument gate(32);
      deserializeJson(gate, response);

      int open = gate["provisional_open"];
      Serial.println(open);

      if (open != getGate()) {
        switchGate(open);

        char path[200] = "";
        strcpy(path, "/gates/");
        strcat(path, gate_id);
        strcat(path, "/solicitations/valid");

        DynamicJsonDocument status(1024);
        status["status"] = open;

        String request;
        serializeJson(status, request);

        requestPATCH(path, (char *)request.c_str());
      }
    }

    previous_time = current_time;
  }

  if (Serial.availableForWrite()) {
    String c = Serial.readStringUntil('\n');

    if (c[0] == 'a') {
      switchGate("Teste bem sucedido");
    } else if (c[0] == 't') {
      c = strtok((char *)c.c_str(), " ");
      c = strtok(NULL, " ");

      RTC.adjust(DateTime(__DATE__, c.c_str()));
    } else if (c[0] == 'h')
      printHour();
  }

  if (HC12.available()) {
    DateTime now = RTC.now();
    String encrypted_code = HC12.readString();

    String code = decryptCode(encrypted_code, iv);

    Serial.print("Descriptografado: ");
    Serial.println(code);

    if (code.startsWith("GJ")) {
      unsigned long splitted_code[3];
      splitCode(code, splitted_code);

      if (!checkSeed(splitted_code[1])) {
        randomSeedRefactored(splitted_code[1]);
        int i = 0;

        for (long n = 0; n != splitted_code[0]; i++)
          n = randomRefactored(10000, 99999);

        if (i > getSequence()) {
          setSequence(i);
          if (true) {
            // if (now.unixtime() - splitted_code[2] < CODE_EXPIRE) {
            Serial.print("Tempo de leitura: ");
            Serial.print(now.unixtime() - splitted_code[2]);
            Serial.println("s");
            switchGate(encrypted_code);
          } else {
            Serial.println("Código expirado");
            Serial.println();
            String solicitation = newSolicitation(6, encrypted_code);

            char path[200] = "", response[200] = "";
            strcpy(path, "/solicitations/");
            strcat(path, gate_id);
            strcat(path, "/gate");

            requestPOST(path, (char *)solicitation.c_str(), (char *)response);
          }
        } else {
          Serial.print("Código fora da sequência");
          Serial.println();
          String solicitation = newSolicitation(5, encrypted_code);

          char path[200] = "", response[200] = "";
          strcpy(path, "/solicitations/");
          strcat(path, gate_id);
          strcat(path, "/gate");

          requestPOST(path, (char *)solicitation.c_str(), (char *)response);
        }
      } else {
        setSequence(0);
        String message = paddingString("resetSeed");

        if (message != "") {
          message = encryptCode(message, iv);

          const char *send_hc12 = message.c_str();
          HC12.write(send_hc12);
          Serial.println("Comando para resetar seed enviado.");
          Serial.println();
        }
      }
    } else {
      Serial.println("Código inválido");
      Serial.println();
      String solicitation = newSolicitation(4, encrypted_code);

      char path[200] = "", response[200] = "";
      strcpy(path, "/solicitations/");
      strcat(path, gate_id);
      strcat(path, "/gate");

      requestPOST(path, (char *)solicitation.c_str(), (char *)response);
    }
  }
}

// Splits the code into sequence(index 0 of splitted_code), seed(index 1 of splitted_code) and unixtime(index 2 of splitted_code)
void splitCode(String decrypted_code, unsigned long *splitted_code) {

  const char *code_const_char = decrypted_code.c_str();
  char *code_char = (char *)code_const_char;
  code_char = strtok(code_char, " ");

  code_char = strtok(code_char, "-");
  code_char = strtok(NULL, "-");

  splitted_code[0] = atol(code_char);

  code_char = strtok(NULL, "-");

  splitted_code[1] = atol(code_char);

  code_char = strtok(NULL, "-");

  splitted_code[2] = atol(code_char);
}

// Store seed in EEPROM memory in cycles preceded by value 1
void storeSeed(unsigned long seed) {
  int lastIndex = searchEmptyIndexInMemory();

  writeMemory(lastIndex, seed);

  if (lastIndex == EEPROM_SIZE - 2)
    lastIndex = 4;
  else
    lastIndex += 2;

  writeMemory(lastIndex, 1);
}

// Check if the current seed is the last one registered in the EEPROM memory
bool checkLastSeed(unsigned long seed) {
  int lastIndex = searchEmptyIndexInMemory();

  if (lastIndex == 4)
    lastIndex = EEPROM_SIZE - 2;
  else
    lastIndex -= 2;

  if (readMemory(lastIndex) == seed)
    return true;
  else {
    Serial.println("Seed repetida.");
    return false;
  }
}

// Check if the seed has already been used in the EEPROM memory
bool checkSeed(unsigned long seed) {
  for (int i = 4; i < EEPROM_SIZE; i += 2) {
    // if (readMemory(i) == 0)
    //   break;

    if (seed == readMemory(i)) {
      if (checkLastSeed(seed))
        return false;
      else
        return true;
    }
  }
  setSequence(0);
  storeSeed(seed);

  return false;
}

// Returns the state of the gate (0) for closed and (1) for open
int getGate() {
  return readMemory(0);
}

// Return the sequence value recorded in the EEPROM memory
int getSequence() {
  return readMemory(2);
}

void setSequence(int sequence) {
  writeMemory(2, sequence);
}

// Changes the state of the gate at each call
void switchGate(String encrypted_code) {
  if (getGate()) {
    writeMemory(0, 0);
    digitalWrite(LED_PORT, LOW);
    Serial.println("Código correto, fechando portão!");
    Serial.println();

    for (int i = SERVO_DEGREES; i >= 0; i -= 3)
      servo.write(i);

    String solicitation = newSolicitation(2, encrypted_code);

    char path[200] = "", response[200] = "";
    strcpy(path, "/solicitations/");
    strcat(path, gate_id);
    strcat(path, "/gate");

    requestPOST(path, (char *)solicitation.c_str(), (char *)response);
  } else {
    writeMemory(0, 1);
    Serial.println("Código correto, abrindo portão!");
    Serial.println();

    for (int i = 0; i <= SERVO_DEGREES; i += 3)
      servo.write(i);

    digitalWrite(LED_PORT, HIGH);

    String solicitation = newSolicitation(1, encrypted_code);

    char path[200] = "", response[200] = "";
    strcpy(path, "/solicitations/");
    strcat(path, gate_id);
    strcat(path, "/gate");

    requestPOST(path, (char *)solicitation.c_str(), (char *)response);
  }
}

// Changes the state of the gate according to the status parameter
void switchGate(int status) {
  if (status) {
    writeMemory(0, 1);
    Serial.println("Abrindo portão!");
    Serial.println();
    for (int i = 0; i <= SERVO_DEGREES; i += 3)
      servo.write(i);

    digitalWrite(LED_PORT, HIGH);
  } else {
    writeMemory(0, 0);
    digitalWrite(LED_PORT, LOW);
    Serial.println("Fechando portão!");
    Serial.println();

    for (int i = SERVO_DEGREES; i >= 0; i -= 3)
      servo.write(i);
  }
}

// Print date and hour in serial monitor
void printHour() {
  DateTime now = RTC.now();

  Serial.print("Data: ");
  Serial.print(now.day(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.year(), DEC);
  Serial.print(" / Hora: ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();
}