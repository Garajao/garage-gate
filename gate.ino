#include <AESLib.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <RTClib.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

#include <ArduinoJson.h>

#include <Servo.h>

#define EEPROM_SIZE 1024
#define CODE_LENGTH 32
#define CODE_EXPIRE 10
#define SERVO_PORT 16
#define SERVO_DEGREES 180
#define LED_PORT 14

ESP8266WebServer web_server(80);
SoftwareSerial HC12(12, 13);
RTC_DS1307 RTC;
AES AES;

Servo servo;

extern String SSID;
extern String PASS;
extern String server_name;
extern String gate_id;

extern String key;
extern String iv;
unsigned long seed, random_number;

void setup() {
  HC12.begin(9600);
  Serial.begin(115200);
  WiFi.begin(SSID, PASS);
  EEPROM.begin(EEPROM_SIZE);
  pinMode(LED_PORT, OUTPUT);
  servo.attach(SERVO_PORT);

  if (getGate()) {
    servo.write(SERVO_DEGREES);
    digitalWrite(LED_PORT, HIGH);
  } else {
    servo.write(0);
    digitalWrite(LED_PORT, LOW);
  }

  if (!RTC.begin()) {
    Serial.println("DS1307 não encontrado");
    // RTC.adjust(DateTime(F(__DATE__), F(__TIME__)));
    while (true) {}
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Conectado | Endereço IP: ");
  Serial.println(WiFi.localIP());

  web_server.on("/update-gate", responseFromAPI);
  web_server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {

  web_server.handleClient();

  char c = Serial.read();

  if (c == 'a') {
    switchGate("Teste bem sucedido");
  } else if (c == 'h')
    printHour();

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
            requestToAPI(400, "Código expirado", encrypted_code);
            Serial.println("Código expirado");
            Serial.println();
          }
        } else {
          requestToAPI(400, "Código fora da sequência", encrypted_code);
          Serial.print("Código fora da sequência");
          Serial.println();
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
      requestToAPI(404, "Código inválido", encrypted_code);
      Serial.println("Código inválido");
      Serial.println();
    }
  }
}

// Read memory from EEPROM each 2 addresses
long readMemory(int address) {
  byte hb = EEPROM.read(address);
  byte lb = EEPROM.read(address + 1);

  word bytes;
  bytes = word(hb, lb);

  return bytes;
}

// Write in memory from EEPROM in 2 addresses
void writeMemory(int address, unsigned long seed) {
  byte hb = highByte(seed);
  byte lb = lowByte(seed);

  word bytes;
  bytes = word(hb, lb);

  EEPROM.put(address, hb);
  EEPROM.put(address + 1, lb);
  EEPROM.commit();
}

// Clear EEPROM memory by assigning 0 to all bytes
void clearMemory() {
  for (int i = 4; i < EEPROM_SIZE; i += 2)
    writeMemory(i, 0);
}

// Searches the first empty position and returns in integer format
int searchEmptyIndexInMemory() {
  int index = 4;

  while (index < EEPROM_SIZE - 2) {
    if (readMemory(index) == 1)
      break;

    index += 2;
  }

  return index;
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

  if (readMemory(lastIndex) == seed) {
    Serial.println("Última seed.");
    return true;
  } else {
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
  if (readMemory(0)) {
    writeMemory(0, 0);
    digitalWrite(LED_PORT, LOW);
    Serial.println("Código correto, fechando portão!");
    Serial.println();

    for (int i = SERVO_DEGREES; i >= 0; i--)
      servo.write(i);

    requestToAPI(200, "Fechando portão", encrypted_code);
  } else {
    writeMemory(0, 1);
    Serial.println("Código correto, abrindo portão!");
    Serial.println();

    for (int i = 0; i <= SERVO_DEGREES; i++)
      servo.write(i);

    digitalWrite(LED_PORT, HIGH);
    requestToAPI(200, "Abrindo portão", encrypted_code);
  }
}

// Changes the state of the gate according to the status parameter
void switchGate(int status) {
  if (status != getGate()) {
    if (status) {
      writeMemory(0, 1);
      Serial.println("Abrindo portão!");
      Serial.println();
      for (int i = 0; i <= SERVO_DEGREES; i++)
        servo.write(i);
        
      digitalWrite(LED_PORT, HIGH);
    } else {
      writeMemory(0, 0);
      digitalWrite(LED_PORT, LOW);
      Serial.println("Fechando portão!");
      Serial.println();

      for (int i = SERVO_DEGREES; i >= 0; i--)
        servo.write(i);
    }
  }
}

// Pads with spaces until the string is mod 16 characters long
String paddingString(String text) {
  const char *text_char = text.c_str();

  if (strlen(text_char) <= CODE_LENGTH) {
    for (int i = strlen(text_char); i < CODE_LENGTH; i++)
      text.concat(" ");

    return text;
  } else
    return "";
}

// Encrypts a string with AES and base64
String encryptCode(String code, String iv) {
  char code_char[CODE_LENGTH];

  for (int i = 0; i < CODE_LENGTH; i++)
    code_char[i] = code.c_str()[i];

  AES.set_key((byte *)key.c_str(), 16);
  AES.cbc_encrypt((byte *)code_char, (byte *)code_char, CODE_LENGTH / 16, (byte *)iv.c_str());

  char encrypted_code[1000];
  base64_encode(encrypted_code, (char *)code_char, CODE_LENGTH);

  return encrypted_code;
}

// Decrypts a string with base64 and AES
String decryptCode(String code, String iv) {
  char decrypted_code[code.length()];
  base64_decode((char *)decrypted_code, (char *)code.c_str(), code.length());

  AES.set_key((byte *)key.c_str(), 16);
  AES.cbc_decrypt((byte *)decrypted_code, (byte *)decrypted_code, CODE_LENGTH / 16, (byte *)iv.c_str());

  return decrypted_code;
}

// Refactoring of the randomization algorithm based on Linear congruential generator
long randomRefactored(long howbig) {
  if (howbig == 0)
    return 0;

  random_number = random_number * 1103515245 + 12345;
  return (unsigned int)(random_number / 65536) % howbig;
}

// Refactoring of the randomization algorithm based on Linear congruential generator
long randomRefactored(long howsmall, long howbig) {
  if (howsmall >= howbig)
    return howsmall;

  long diff = howbig - howsmall;
  return randomRefactored(diff) + howsmall;
}

// Refactoring of the randomization algorithm based on Linear congruential generator
void randomSeedRefactored(long value) {
  random_number = value;
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

// Send a request to the API
void requestToAPI(int status_code, String message, String encrypted_code) {
  String path = "/solicitation/";
  path += gate_id;
  path += "/gate";

  if ((WiFi.status() == WL_CONNECTED)) {
    WiFiClient client;
    HTTPClient http;

    if (http.begin(client, server_name + path)) {
      http.setTimeout(5000);
      http.addHeader("Content-Type", "application/json");

      DynamicJsonDocument gate(1024);
      gate["status"] = getGate();
      gate["method"] = "ARDUINO";
      gate["status_code"] = status_code;
      gate["message"] = message;
      gate["code"] = encrypted_code;

      String request;
      serializeJson(gate, request);

      int httpResponseCode = http.POST(request);

      if (httpResponseCode > 0) {
        Serial.print("[HTTP] Code ");
        Serial.print(httpResponseCode);
        Serial.println();

        String payload = http.getString();
        Serial.println(payload);
        Serial.println();
      } else
        Serial.printf("[HTTP] Erro: %s\n", http.errorToString(httpResponseCode).c_str());

      http.end();
    } else
      Serial.printf("[HTTP] Não foi possível conectar");
  }
}

// Receive a response from the API
void responseFromAPI() {

  if (!web_server.hasArg("plain")) {
    web_server.send(200, "text/plain", "Body not received");
    return;
  }

  String payload = web_server.arg("plain");

  web_server.send(200, "text/plain", payload);

  DynamicJsonDocument payload_json(1024);
  deserializeJson(payload_json, payload);
  int status = payload_json["status"];
  String method = payload_json["method"];

  if (method)
    switchGate(status);
}