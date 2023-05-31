#include <AESLib.h>
#include <arduino_base64.hpp>
#include <SoftwareSerial.h>
#include <EEPROM.h>
#include <RTClib.h>

#define ARDUINO_MEMORY 1024
#define CODE_LENGTH 32

SoftwareSerial HC12(2, 3);
RTC_DS1307 rtc;

extern uint8_t key[];
int sequence = 0;
unsigned long seed;

void setup() {
  Serial.begin(9600);
  HC12.begin(9600);

  if (!rtc.begin()) {
    Serial.println("DS1307 não encontrado");
    while (true) {}
  }

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
}

void loop() {

  if (HC12.available()) {
    DateTime now = rtc.now();
    String code = HC12.readStringUntil("\n");

    code = decryptCode(code);

    Serial.print("Descriptografado: ");
    Serial.println(code);

    unsigned long splittedCode[3];
    splitCode(code, splittedCode);

    if (!checkSeed(splittedCode[1])) {
      randomSeed(splittedCode[1]);
      int i = 0;

      for (long n = 0; n != splittedCode[0]; i++)
        n = random(10000, 99999);

      if (i > sequence) {
        sequence = i;
        Serial.print("Tempo de leitura (s): ");
        Serial.println(now.unixtime() - splittedCode[2]);
        switchGate();
      } else {
        Serial.println("Código fora da sequência.");
        Serial.println();
      }
    } else {
      sequence = 0;
      String message = paddingString("resetSeed");

      if (message != "") {
        message = encryptCode(message);

        char *send_hc12 = message.c_str();
        HC12.write(send_hc12);
        Serial.println("Comando para resetar seed enviado.");
        Serial.println();
      }
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

  EEPROM.write(address, hb);
  EEPROM.write(address + 1, lb);
}

// Clear EEPROM memory by assigning 0 to all bytes
void clearMemory() {
  for (int i = 2; i < ARDUINO_MEMORY; i += 2)
    writeMemory(i, 0);
}

// Searches the first empty position and returns in integer format
int searchEmptyIndexInMemory() {
  int index = 2;

  while (index < ARDUINO_MEMORY - 2) {
    if (readMemory(index) == 1)
      break;

    index += 2;
  }

  return index;
}

// Splits the code into sequence(index 0 of splittedCode), seed(index 1 of splittedCode) and unixtime(index 2 of splittedCode)
void splitCode(String decrypted_code, unsigned long *splittedCode) {

  char *code_char = decrypted_code.c_str();
  code_char = strtok(code_char, " ");

  code_char = strtok(code_char, "-");
  code_char = strtok(NULL, "-");

  splittedCode[0] = atol(code_char);

  code_char = strtok(NULL, "-");

  splittedCode[1] = atol(code_char);

  code_char = strtok(NULL, "-");

  splittedCode[2] = atol(code_char);
}

// Store seed in EEPROM memory in cycles preceded by value 1
void storeSeed(unsigned long seed) {
  int lastIndex = searchEmptyIndexInMemory();

  writeMemory(lastIndex, seed);

  if (lastIndex == ARDUINO_MEMORY - 2)
    lastIndex = 2;
  else
    lastIndex += 2;

  writeMemory(lastIndex, 1);
}

// Check if the current seed is the last one registered in the EEPROM memory
bool checkLastSeed(unsigned long seed) {
  int lastIndex = searchEmptyIndexInMemory();

  if (lastIndex == 2)
    lastIndex = ARDUINO_MEMORY - 2;
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
  for (int i = 2; i < ARDUINO_MEMORY; i += 2) {
    // if (readMemory(i) == 0)
    //   break;

    if (seed == readMemory(i)) {
      if (checkLastSeed(seed))
        return false;
      else
        return true;
    }
  }
  sequence = 0;
  storeSeed(seed);

  return false;
}

// Returns the state of the gate (0) for closed and (1) for open
int getGate() {
  return readMemory(0);
}

// Changes the state of the gate at each call
void switchGate() {
  if (readMemory(0) == 0) {
    writeMemory(0, 1);
    Serial.println("Código correto, abrindo portão!");
    Serial.println();
  } else {
    writeMemory(0, 0);
    Serial.println("Código correto, fechando portão!");
    Serial.println();
  }
}

// Pads with spaces until the string is mod 16 characters long
String paddingString(String text) {
  char *text_char = text.c_str();

  if (strlen(text_char) <= CODE_LENGTH) {
    for (int i = strlen(text_char); i < CODE_LENGTH; i++)
      text.concat(" ");

    return text;
  } else
    return "";
}

// Encrypts a string with AES and base64
String encryptCode(String code) {
  char code_char[CODE_LENGTH];

  for (int i = 0; i < CODE_LENGTH; i++) {
    code_char[i] = code.c_str()[i];
  }

  aes128_enc_multiple(key, code_char, sizeof(code_char));

  auto code_length = sizeof(code_char);
  char encrypted_code[base64::encodeLength(code_length)];
  base64::encode(code_char, code_length, encrypted_code);

  return encrypted_code;
}

// Decrypts a string with base64 and AES
String decryptCode(String code) {
  const char *code_char = code.c_str();

  uint8_t decrypted_code[base64::decodeLength(code_char)];
  base64::decode(code_char, decrypted_code);

  aes128_dec_multiple(key, decrypted_code, sizeof(decrypted_code));

  return decrypted_code;
}