// Creation of solicitation object in json format
String newSolicitation(int message, String code) {
  DynamicJsonDocument solicitation(1024);

  solicitation["status"] = getGate();
  solicitation["message"] = message;
  solicitation["code"] = code;
  solicitation["valid"] = true;

  String json_data;
  serializeJson(solicitation, json_data);

  return json_data;
}

//
void requestGET(char *path_url, char *output) {
  if ((WiFi.status() != WL_CONNECTED)) {
    Serial.println("WiFi disconnected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setReuse(false);

  char url[200];
  strcpy(url, server_name);
  strcat(url, path_url);

  if (!https.begin(client, url)) {
    Serial.println("[HTTPS] Could not connect");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", token);
  https.addHeader("Connection", "close");

  int httpsResponse = https.GET();

  if (httpsResponse < 0) {
    Serial.print("[HTTPS] ");
    Serial.println(https.errorToString(httpsResponse).c_str());
    https.end();
    return;
  }

  strcpy(output, https.getString().c_str());
  https.end();
  return;
}

//
void requestPOST(char *path_url, char *json_data, char *output) {
  if ((WiFi.status() != WL_CONNECTED)) {
    Serial.println("WiFi disconnected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setReuse(false);

  char url[200];
  strcpy(url, server_name);
  strcat(url, path_url);

  if (!https.begin(client, url)) {
    Serial.println("[HTTPS] Could not connect");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", token);
  https.addHeader("Connection", "close");

  int httpsResponse = https.POST(json_data);

  if (httpsResponse < 0) {
    Serial.print("[HTTPS] ");
    Serial.println(https.errorToString(httpsResponse).c_str());
    https.end();
    return;
  }

  strcpy(output, https.getString().c_str());
  https.end();
  return;
}

//
void requestPATCH(char *path_url, char *json_data) {
  if ((WiFi.status() != WL_CONNECTED)) {
    Serial.println("WiFi disconnected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  https.setReuse(false);

  char url[200];
  strcpy(url, server_name);
  strcat(url, path_url);

  if (!https.begin(client, url)) {
    Serial.println("[HTTPS] Could not connect");
    return;
  }

  https.addHeader("Content-Type", "application/json");
  https.addHeader("Authorization", token);
  https.addHeader("Connection", "close");

  int httpsResponse = https.PATCH(json_data);

  if (httpsResponse < 0) {
    Serial.print("[HTTPS] ");
    Serial.println(https.errorToString(httpsResponse).c_str());
    https.end();
    return;
  }

  Serial.println(httpsResponse);

  https.end();
  return;
}