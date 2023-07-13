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

// Pads with spaces until the string is mod 16 characters long
String paddingString(String text) {
  const char *text_char = text.c_str();

  if (strlen(text_char) <= CODE_LENGTH) {
    for (int i = strlen(text_char); i < CODE_LENGTH; i++)
      text.concat(" ");

    return text;
  }

  return "";
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