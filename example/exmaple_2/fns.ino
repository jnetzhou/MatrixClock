String macToStr(const uint8_t* mac) {
  String result;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] < 0x10) result += "0";
    result += String(mac[i], HEX);
    if (i < 5)
      result += ':';
  }
  result.toUpperCase();
  return result;
}

String macLastThreeSegments(const uint8_t* mac) {
  String result;
  for (int i = 3; i < 6; ++i) {
    if (mac[i] < 0x10) result += "0";
    result += String(mac[i], HEX);
  }
  result.toUpperCase();
  return result;
}
//------------读取配置文件------------------
bool readConfig() {
  File stateFile = SPIFFS.open("/config.json", "r");//打开配置文件
  if (!stateFile) {
    Serial.println("Failed to read config file... first run?");
    Serial.println("Creating new file...");
    saveConfig();//保存配置文件
    return false;//如果打开失败则返回
  }
  DeserializationError error = deserializeJson(json, stateFile.readString());
  stateFile.close();
  return true;
}
//------------保存配置文件-----------------
bool saveConfig() {
  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }
  serializeJson(json, configFile);
  //serializeJson(json, Serial);
  configFile.close();
  return true;
}
