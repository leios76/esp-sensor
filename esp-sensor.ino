/*
    This sketch sends a message to a TCP server
  TODO:
  - RTC memory
*/
const char * version = "1.4-dev";

const char * server = "iot.nor.kr";
const char * ADMIN_APIKEY = "0fbb63ec236e0c8d66df2f4a8cb56234";
const char * APIKEY = "";
const char * SSIDNAME = "";
const char * PASSWORD = "";

char chip_id[9];

uint32_t period_sec = 60;

extern "C" {
#include <user_interface.h>
}

#include <ArduinoJson.h>
StaticJsonBuffer<512> jsonBuffer;

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

#include <FS.h>

#include <OneWire.h>
#include <DallasTemperature.h>

#define ONE_WIRE_BUS D4

/********************************************************************/
// Setup a oneWire instance to communicate with any OneWire devices
// (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

/********************************************************************/
// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);


void sendTeperatureApi(char * str_address, float temp) {
  if (strlen(APIKEY) == 0) {
    return;
  }
  String postStr = "apikey=" + String(APIKEY);
  postStr += "&node=ds18b20";
  postStr += "&data={" + String(str_address) + ":" + String((int)(temp * 1000)) + "}";

  Serial.print("Request: ");
  Serial.println(postStr);

  HTTPClient http;

  http.begin("http://iot.nor.kr/input/post");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(postStr);
  http.writeToStream(&Serial);

  http.end();
  Serial.println("");
}

void report_temperature() {
  float temp;
  uint8_t address[8];
  char str_address[18];

  /********************************************************************/
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures(); // Send the command to get temperature readings
  Serial.println("DONE");

  uint8_t deviceCount = sensors.getDeviceCount();
  Serial.printf("Found %d sensor(s)\n", deviceCount);
  for (uint8_t i = 0; i < deviceCount; i++) {
    /********************************************************************/
    // You can have more than one DS18B20 on the same bus.
    temp = sensors.getTempCByIndex(i);

    memset(str_address, '0', sizeof(str_address));
    str_address[sizeof(str_address) - 1] = 0;

    if (sensors.getAddress(address, i)) {
      sprintf(str_address, "%02x-%02x%02x%02x%02x%02x%02x%02x", address[0], address[1], address[2], address[3], address[4], address[5], address[6], address[7]);
    }

    Serial.printf("[%d][%s] %.3f\n", i, str_address, temp);
    if (temp > DEVICE_DISCONNECTED_C) {
      sendTeperatureApi(str_address, temp);
    }
  }
}

void report_admin() {
  if (strlen(ADMIN_APIKEY) == 0) {
    return;
  }
  String postStr = "apikey=" + String(ADMIN_APIKEY);
  postStr += "&node=esp-sensor";
  postStr += "&data=";
  postStr += "{" + String(chip_id) + ": " + String(version) + "}";

  Serial.print("Request: ");
  Serial.println(postStr);

  HTTPClient http;

  http.begin("http://iot.nor.kr/input/post");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(postStr);
  http.writeToStream(&Serial);

  http.end();
  Serial.println("");
}

void report_board() {
  if (strlen(APIKEY) == 0) {
    return;
  }
  String postStr = "apikey=" + String(APIKEY);
  postStr += "&node=";
  postStr += String(chip_id);
  postStr += "&data=";
  postStr += "{vcc: " + String(analogRead(A0)) + "}";

  Serial.print("Request: ");
  Serial.println(postStr);

  HTTPClient http;

  http.begin("http://iot.nor.kr/input/post");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.POST(postStr);
  http.writeToStream(&Serial);

  http.end();
  Serial.println("");
}

void next_tick() {
  unsigned long delay_ms;

  if (millis() < period_sec * 1000) {
    delay_ms = period_sec * 1000 - millis();
  } else {
    delay_ms = 1;
  }
  Serial.print("Going to sleep for ");
  Serial.print(delay_ms);
  Serial.println(" ms");

  //ESP.deepSleep(delay_ms * 1000, WAKE_RF_DISABLED);
  ESP.deepSleep(delay_ms * 1000, RF_DEFAULT);
}

void check_reset_reason() {
  rst_info * reset_info;

#if 0
  enum rst_reason {
    REASON_DEFAULT_RST = 0, /* normal startup by power on */
    REASON_WDT_RST = 1, /* hardware watch dog reset */
    REASON_EXCEPTION_RST = 2, /* exception reset, GPIO status won't change */
    REASON_SOFT_WDT_RST   = 3, /* software watch dog reset, GPIO status won't change */
    REASON_SOFT_RESTART = 4, /* software restart ,system_restart , GPIO status won't change */
    REASON_DEEP_SLEEP_AWAKE = 5, /* wake up from deep-sleep */
    REASON_EXT_SYS_RST      = 6 /* external system reset */
  };
#endif

  reset_info = ESP.getResetInfoPtr();
  Serial.println(reset_info->reason);
}

void setup_wifi() {
  uint32_t retry_count = 20;
  uint32_t column = 0;

  /*
    if (!SPIFFS.exists("/setting.json")) {
      File setting = SPIFFS.open("/setting.json", "w");
      if (setting) {
        setting.println("{\"SSIDNAM\": \"FALINUX_AP\", \"PASSWORD\": \"fa025729527\"}");
        setting.close();
      }
    }
  */

  if (digitalRead(D5) == 0) {
    Serial.println("Remove invalid config file");
    SPIFFS.remove("/setting.json");
  }

  if (SPIFFS.exists("/setting.json")) {
    File setting = SPIFFS.open("/setting.json", "r");
    Serial.println("Reading setting.json...");
    JsonObject& root = jsonBuffer.parse(setting);
    setting.close();

    if (root.containsKey("SSIDNAME")) {
      SSIDNAME = root["SSIDNAME"];
      Serial.print("SSID: ");
      Serial.println(SSIDNAME);
    }

    if (root.containsKey("PASSWORD")) {
      PASSWORD = root["PASSWORD"];

      Serial.print("PASS: ");
      Serial.println(PASSWORD);
    }
  }

  if (SSIDNAME == NULL || strlen(SSIDNAME) == 0) {
    Serial.println("Init wifi smart config...");
    WiFi.beginSmartConfig();
    retry_count = 0xFFFFFFFF;
  } else {
    Serial.println("Init wifi client...");

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);

    // We start by connecting to a WiFi network
    //WiFi.beginSmartConfig();
    WiFi.begin(SSIDNAME, PASSWORD);
    retry_count = 20;
  }

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    column++;
    if ((column % 80) == 0) {
      Serial.println("");
      Serial.print(String(chip_id) + ": ");
    }
    //Serial.println(WiFi.smartConfigDone());
    if (retry_count-- == 0) {
      next_tick();
    }
  }
  Serial.println("");

  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("PASS: ");
  Serial.println(WiFi.psk());

  if (!SPIFFS.exists("/setting.json")) {
    File setting = SPIFFS.open("/setting.json", "w");
    if (setting) {
      JsonObject& root = jsonBuffer.createObject();
      root["SSIDNAME"] = WiFi.SSID();
      root["PASSWORD"] = WiFi.psk();
      root.printTo(setting);
      setting.close();
    }
  }
}

void board_info() {
  Serial.print("getCoreVersion(): ");
  Serial.println(ESP.getCoreVersion());

  Serial.print("getSdkVersion(): ");
  Serial.println(ESP.getSdkVersion());

  Serial.print("getChipId(): ");
  Serial.println(chip_id);

  Serial.print("millis(): ");
  Serial.println(millis());

  Serial.print("getFlashChipSize(): ");
  Serial.println(ESP.getFlashChipSize());

  Serial.print("getFlashChipRealSize(): ");
  Serial.println(ESP.getFlashChipRealSize());

  Serial.print("getVcc(): ");
  Serial.println(ESP.getVcc());
}

const char* host = "api.github.com";
const int httpsPort = 443;

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
//const char* fingerprint = "CF 05 98 89 CA FF 8E D8 5E 5C E0 C2 E4 F7 E6 C3 C7 50 DD 5C";
const char* fingerprint = "‎‎35 85 74 ef 67 35 a7 ce 40 69 50 f3 c0 f6 80 cf 80 3b 2e 19";

void ssl_test() {
  // Use WiFiClientSecure class to create TLS connection
  WiFiClientSecure client;
  Serial.print("connecting to ");
  Serial.println(host);
  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }

  if (client.verify(fingerprint, host)) {
    Serial.println("certificate matches");
  } else {
    Serial.println("certificate doesn't match");
  }

  String url = "/repos/esp8266/Arduino/commits/master/status";
  Serial.print("requesting URL: ");
  Serial.println(url);

  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: BuildFailureDetectorESP8266\r\n" +
               "Connection: close\r\n\r\n");

  Serial.println("request sent");
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      Serial.println("headers received");
      break;
    }
  }
  String line = client.readStringUntil('\n');
  if (line.startsWith("{\"state\":\"success\"")) {
    Serial.println("esp8266/Arduino CI successfull!");
  } else {
    Serial.println("esp8266/Arduino CI has failed");
  }
  Serial.println("reply was:");
  Serial.println("==========");
  Serial.println(line);
  Serial.println("==========");
  Serial.println("closing connection");
}


void get_config() {
  HTTPClient http;

  String url = "http://iot.nor.kr/esp/devices/" + String(chip_id) + "/config.json";

  http.begin(url);
  Serial.println("Open url: " + url);

  int http_code = http.GET();

  if (http_code == HTTP_CODE_OK) {
    String config = http.getString();
    Serial.println(config);

    JsonObject& root = jsonBuffer.parseObject(config);
    if (root.containsKey("period")) {
      period_sec = root["period"];
      Serial.println("period: " + String(period_sec));
    }

    if (root.containsKey("apikey")) {
      APIKEY = root["apikey"];
      Serial.println("apikey: " + String(APIKEY));
    }

    if (root.containsKey("version")) {
      const char * new_version = root["version"];
      if (strcmp(new_version, version) != 0) {
        Serial.print("Updating ");
        Serial.println(new_version);
        t_httpUpdate_return ret = ESPhttpUpdate.update("http://iot.nor.kr/esp/firmware/esp-sensor_" + String(new_version) + "_" + (int)(ESP.getFlashChipSize() / 1024 / 1024 ) + "M.bin");

        switch (ret) {
          case HTTP_UPDATE_FAILED:
            Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
            break;

          case HTTP_UPDATE_NO_UPDATES:
            Serial.println("HTTP_UPDATE_NO_UPDATES");
            break;

          case HTTP_UPDATE_OK:
            Serial.println("HTTP_UPDATE_OK");
            break;
        }
      }
      Serial.println("");

    }
  }


  http.end();
}


void setup() {
  Serial.begin(115200);

  //WiFi.printDiag(Serial);
  //Serial.setDebugOutput(true);

  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true);
  WiFi.setAutoConnect(false);
  WiFi.stopSmartConfig();

  sprintf(chip_id, "%08X", ESP.getChipId());

  Serial.print("Version: ");
  Serial.println(version);

  board_info();
  check_reset_reason();

  Serial.println("Init temperature sensor...");
  sensors.begin();

  delay(10);

  if (!SPIFFS.begin()) {
    Serial.println("Format SPIFFS...");
    SPIFFS.format();
  }

  setup_wifi();

  get_config();
}

void loop() {

  // connected
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("WiFi RSSI: ");
  Serial.println(WiFi.RSSI());

  report_temperature();
  report_admin();
  report_board();
  //ssl_test();

  next_tick();
}

