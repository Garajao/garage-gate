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

// Clear EEPROM memory by assigning 0 to all bytes
void clearMemory() {
  for (int i = 4; i < EEPROM_SIZE; i += 2)
    writeMemory(i, 0);
}