#include <FS.h>
#include <ArduinoJson.h>
#include <math.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
//#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
//#include <ArduinoOTA.h>
#include <TimeLib.h>
#include <Ticker.h>
#include <Timezone.h>

#include <SPI.h>
#include <Ticker.h>



#include <Wire.h>
#include <time.h>
#include "zk.h"
#define SDA        5      // Pin sda (I2C)
#define SCL        4      // Pin scl (I2C)
#define CS         15     // Pin cs  (SPI)
#define anzMAX     4        //点阵模块数量
unsigned short maxPosX = anzMAX * 8 - 1;            
unsigned short LEDarr[anzMAX][8];                   
unsigned short helpArrMAX[anzMAX * 8];              
unsigned short helpArrPos[anzMAX * 8];              
unsigned int z_PosX = 0;                            
unsigned int d_PosX = 0;                            
bool f_tckr1s = false;
bool f_tckr50ms = false;
bool kk;
struct DateTime {
    unsigned short sek1, sek2, sek12, min1, min2, min12, std1, std2, std12;
    unsigned short tag1, tag2, tag12, mon1, mon2, mon12, jahr1, jahr2, jahr12, WT;//Tage日, Monate月, Jahre年(德语)
} MEZ;

// The object for the Ticker
Ticker tckr;
// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

#define AP_NAME "MatrixClock_"
#define FW_VERSION "2.00"
#define CONFIG_TIMEOUT 300000 // 300000mS = 5 minutes

// ONLY CHANGE DEFINES BELOW IF YOU KNOW WHAT YOU'RE DOING!
#define NORMAL_MODE 0
#define OTA_MODE 1
#define CONFIG_MODE 2
#define CONFIG_MODE_LOCAL 3
#define CONNECTION_FAIL 4
#define UPDATE_SUCCESS 1
#define UPDATE_FAIL 2


// User global vars用户全局变量
const char* update_path = "/update";
const char* update_username = "nick";
const char* update_password = "nick";
const char* ntpServerName = "pool.ntp.org";



uint8_t bri_vals[3] = {
  3, // LOW
  20, // MEDIUM
  60, // HIGH
};


// Better left alone global vars最好别碰全局变量
unsigned long configStartMillis, prevDisplayMillis;

uint8_t deviceMode = NORMAL_MODE;
bool timeUpdateFirst = true;
bool toggleSeconds = false;
byte mac[6];
volatile uint8_t multiplexState = 0;
volatile uint8_t digitsCache[] = {0, 0, 0, 0};
uint8_t healIterator, bri;
uint8_t timeUpdateStatus = 0; // 0 = no update, 1 = update success, 2 = update fail,
uint8_t failedAttempts = 0;
bool scroll;
IPAddress ip_addr;

TimeChangeRule EDT = {"EDT", Last, Sun, Mar, 1, 120};  //UTC + 2 hours
TimeChangeRule EST = {"EST", Last, Sun, Oct, 1, 60};  //UTC + 1 hours
Timezone TZ(EDT, EST);


DynamicJsonDocument json(2048); // config buffer

Ticker ticker;
Ticker ledsTicker;
Ticker onceTicker;
Ticker colonTicker;
ESP8266WebServer server(80);
WiFiClient espClient;
WiFiUDP Udp;
ESP8266HTTPUpdateServer httpUpdateServer;
unsigned int localPort = 8888;  // local port to listen for UDP packets用于侦听UDP数据包的本地端口
//**************************************************************************************************
void timer50ms() {
    static unsigned int cnt50ms = 0;
    f_tckr50ms = true;
    cnt50ms++;
    if (cnt50ms == 20) {
        f_tckr1s = true; // 1 sec
        cnt50ms = 0;
    }
}
// the setup function runs once when you press reset or power the board当您按下重置或给电路板通电时，设置功能运行一次
void setup() {
    pinMode(16, INPUT);
    pinMode(CS, OUTPUT);
    digitalWrite(CS, HIGH);
    Serial.begin(115200);
    SPI.begin();
    helpArr_init();
    max7219_init();
    rtc_init(SDA, SCL);
    clear_Display();
    refresh_display(); //take info into LEDarr
    tckr.attach(0.05, timer50ms);    // every 50 msec

    if (!SPIFFS.begin()) {
       Serial.println("SPIFFS: Failed to mount file system");
     }
    readConfig();//读取配置文件

    WiFi.macAddress(mac);
  bri = json["bri"].as<int>();
  max7219_set_brightness(bri);//设置亮度
  Serial.print("brightness: ");
  Serial.println(bri);
scroll = json["zero"].as<int>();

  const char* ssid = json["ssid"].as<const char*>();
  const char* pass = json["pass"].as<const char*>();
  const char* ip = json["ip"].as<const char*>();
  const char* gw = json["gw"].as<const char*>();
  const char* sn = json["sn"].as<const char*>();

  if (ssid != NULL && pass != NULL && ssid[0] != '\0' && pass[0] != '\0') {//如果之前存有wifi信息
    Serial.println("WIFI: Setting up wifi");
    WiFi.mode(WIFI_STA);//设置无线终端模式

    if (ip != NULL && gw != NULL && sn != NULL && ip[0] != '\0' && gw[0] != '\0' && sn[0] != '\0') {
      IPAddress ip_address, gateway_ip, subnet_mask;//定义IPAddress类型变量
      if (!ip_address.fromString(ip) || !gateway_ip.fromString(gw) || !subnet_mask.fromString(sn)) {
        Serial.println("Error setting up static IP, using auto IP instead. Check your configuration.");
      } else {
        WiFi.config(ip_address, gateway_ip, subnet_mask);//配置
      }
    }

    // serializeJson(json, Serial);

    WiFi.begin(ssid, pass);//开始连接

    for (int i = 0; i < 500; i++) {
       char2Arr('W', 28, 0);
       char2Arr('i', 22, 0);
       char2Arr('-', 18, 0);
       char2Arr('F', 12, 0);
       char2Arr('i', 6, 0);
       refresh_display(); 
      if (WiFi.status() != WL_CONNECTED) {//如果失败
        if (i > 100) {
          deviceMode = CONFIG_MODE;
          Serial.print("WIFI: Failed to connect to: ");
          Serial.print(ssid);
          Serial.println(", going into config mode.");
          clear_Display();
          char2Arr('E', 25, 0);
          char2Arr('r', 19, 0);
          char2Arr('r', 12, 0);
          char2Arr('!', 6, 0);
          refresh_display(); 
          delay(500);
          break;//尝试100次无法连上就跳出
        }
       
        
        delay(100);
      } else {            //成功连接
        Serial.println("WIFI: Connected...");
        Serial.print("WIFI: Connected to: ");
        Serial.println(WiFi.SSID());
        Serial.print("WIFI: Mac address: ");
        Serial.println(WiFi.macAddress());
        Serial.print("WIFI: IP address: ");
        Serial.println(WiFi.localIP());
        clear_Display();
          char2Arr('O', 25, 0);
          char2Arr('K', 19, 0);
          char2Arr('!', 12, 0);
          char2Arr('!', 6, 0);
          refresh_display(); 
        delay(500);
        break;
      }
    }

  } else {//如果之前没有配置过
    deviceMode = CONFIG_MODE;
     clear_Display();
          char22Arr('C', 25, 0);
          char22Arr('F', 19, 0);
          char22Arr('G', 13, 0);
          char22Arr('!', 7, 0);
         

        
          refresh_display(); 
       delay(1000);
    Serial.println("SETTINGS: No credentials set, going to config mode.");
  }

  
  

  if (deviceMode == CONFIG_MODE || deviceMode == CONNECTION_FAIL) {
    startConfigPortal(); // Blocking loop如果是配置模式或联接失败就启动配置门户
  } else {
    ndp_setup(); //NTP初始配置
    startLocalConfigPortal();//只启用升级门户
  }
  time_t t;
  tm* tt;
    t = getNtpLocalTime() ;//获取网络时间
    tt = localtime(&t);
    if (t != 0)   //如果获取到网络时间则写入RTC
        rtc_set(tt);
        //Serial.println(t);
    else
        Serial.println("no timepacket received");
}

// the loop function runs over and over again forever循环函数永远重复运行
void loop() {
    unsigned int sek1 = 0, sek2 = 0, min1 = 0, min2 = 0, std1 = 0, std2 = 0;
    unsigned int sek11 = 0, sek12 = 0, sek21 = 0, sek22 = 0;
    unsigned int min11 = 0, min12 = 0, min21 = 0, min22 = 0;
    unsigned int std11 = 0, std12 = 0, std21 = 0, std22 = 0;
    signed int x = 0; //x1,x2;
    signed int y = 0, y1 = 0, y2 = 0, y3=0;
    bool updown = false;
    unsigned int sc1 = 0, sc2 = 0, sc3 = 0, sc4 = 0, sc5 = 0, sc6 = 0;
    bool f_scrollend_y = false;
    unsigned int f_scroll_x = false;

    z_PosX = maxPosX;
    d_PosX = -8;
    //  x=0; x1=0; x2=0;

    refresh_display();
    updown = true;
    if (updown == false) {
        y2 = -9;
        y1 = 8;
    }
    if (updown == true) { //scroll  up to down
        y2 = 8;
        y1 = -8;
    }
    while (true) {
        yield();  //在 ESP8266 当中，如果无可避免的需要长时间运行在 loop 当中（例如解算浮点）， 应当时不时调用 yield 保证 8266 后台正常运行，以不重启、不断网为标准。
        if ( MEZ.std12==0 && MEZ.min12==20 && MEZ.sek12==0 ) //syncronisize RTC every day 00:20:00
        { 
            clear_Display();
            delay(500);
            ESP.restart();
        }
        
        
        if(deviceMode == CONFIG_MODE) {
           time_t remainingSeconds = (CONFIG_TIMEOUT - (millis() - configStartMillis)) / 1000;
           if (millis() - configStartMillis > CONFIG_TIMEOUT) { //5分钟时间到
             deviceMode = CONNECTION_FAIL;
             WiFi.mode(WIFI_STA);
            }
          if(WiFi.smartConfigDone())
           {
            clear_Display();
            char2Arr('O', 25, 0);
            char2Arr('K', 19, 0);
            char2Arr('!', 12, 0);
            char2Arr('!', 6, 0);
            refresh_display(); 
            Serial.println("SmartConfig Success");
            Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
            Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
            WiFi.setAutoConnect(true);  // 设置自动连接
          
            Serial.println("WiFi connected");
            Serial.println(WiFi.localIP());
            Serial.println("Starting UDP");
            udp.begin(localPort);
            Serial.print("Local port: ");
            Serial.println(udp.localPort());
            delay(1000); 
            ESP.restart();
            
           }
        }
        if (f_tckr1s == true)        // 1秒时间到
        {if(!digitalRead(16)){kk=1;}else kk=0;//水银开关判断
            rtc2mez(); //读取RTC
            sek1 = MEZ.sek1;
            sek2 = MEZ.sek2;
            min1 = MEZ.min1;
            min2 = MEZ.min2;
            std1 = MEZ.std1;
            std2 = MEZ.std2;
            y = y2;                 //scroll updown
            sc1 = 1;
            sek1++;
            if (sek1 == 10) {
                sc2 = 1;
                sek2++;
                sek1 = 0;
            }
            if (sek2 == 6) {
                min1++;
                sek2 = 0;
                sc3 = 1;
            }
            if (min1 == 10) {
                min2++;
                min1 = 0;
                sc4 = 1;
            }
            if (min2 == 6) {
                std1++;
                min2 = 0;
                sc5 = 1;
            }
            if (std1 == 10) {
                std2++;
                std1 = 0;
                sc6 = 1;
            }
            if ((std2 == 2) && (std1 == 4)) {
                std1 = 0;
                std2 = 0;
                sc6 = 1;
            }

            sek11 = sek12;
            sek12 = sek1;
            sek21 = sek22;
            sek22 = sek2;
            min11 = min12;
            min12 = min1;
            min21 = min22;
            min22 = min2;
            std11 = std12;
            std12 = std1;
            std21 = std22;
            std22 = std2;
            f_tckr1s = false;
            if (MEZ.sek12 == 45)   //秒为45则滚动显示一次
                f_scroll_x = scroll;//滚动开关
        } // end 1s
        if (f_tckr50ms == true) {//以下部分50ms执行一次，用于控制滚动速度
            f_tckr50ms = false;
            if (f_scroll_x == true) {
                z_PosX++;
                d_PosX++;
                if (d_PosX == 101)
                    z_PosX = 0;
                if (z_PosX == maxPosX) {
                    f_scroll_x = false;
                    d_PosX = -8;
                }
            }
            if (sc1 == 1) {
                if (updown == 1)
                    y--;
                else
                    y++;
               y3 = y;
               if (y3 > 0) {
                y3 = 0;
                }     
               char22Arr(48 + sek12, z_PosX - 27, y3);
               char22Arr(48 + sek11, z_PosX - 27, y + y1);
                if (y == 0) {
                    sc1 = 0;
                    f_scrollend_y = true;
                }
            }
            else
                char22Arr(48 + sek1, z_PosX - 27, 0);

            if (sc2 == 1) {
                char22Arr(48 + sek22, z_PosX - 23, y3);
                char22Arr(48 + sek21, z_PosX - 23, y + y1);
                if (y == 0)
                    sc2 = 0;
            }
            else
              char22Arr(48 + sek2, z_PosX - 23, 0);

            if (sc3 == 1) {
                char2Arr(48 + min12, z_PosX - 18, y);
                char2Arr(48 + min11, z_PosX - 18, y + y1);
                if (y == 0)
                    sc3 = 0;
            }
            else
                char2Arr(48 + min1, z_PosX - 18, 0);

            if (sc4 == 1) {
                char2Arr(48 + min22, z_PosX - 13, y);
                char2Arr(48 + min21, z_PosX - 13, y + y1);
                if (y == 0)
                    sc4 = 0;
            }
            else
                char2Arr(48 + min2, z_PosX - 13, 0);

              char2Arr(':', z_PosX - 10 + x, 0);

            if (sc5 == 1) {
                char2Arr(48 + std12, z_PosX - 4, y);
                char2Arr(48 + std11, z_PosX - 4, y + y1);
                if (y == 0)
                    sc5 = 0;
            }
            else
                char2Arr(48 + std1, z_PosX - 4, 0);

            if (sc6 == 1) {
                char2Arr(48 + std22, z_PosX + 1, y);
                char2Arr(48 + std21, z_PosX + 1, y + y1);
                if (y == 0)
                    sc6 = 0;
            }
            else
                char2Arr(48 + std2, z_PosX + 1, 0);

            char2Arr(' ', d_PosX+5, 0);        //day of the week
            char2Arr(WT_arr[MEZ.WT][0], d_PosX - 1, 0);        //day of the week
            char2Arr(WT_arr[MEZ.WT][1], d_PosX - 7, 0);
            char2Arr(WT_arr[MEZ.WT][2], d_PosX - 13, 0);
            char2Arr(WT_arr[MEZ.WT][3], d_PosX - 19, 0);
            char2Arr(48 + MEZ.tag2, d_PosX - 24, 0);           //day
            char2Arr(48 + MEZ.tag1, d_PosX - 30, 0);
            char2Arr(M_arr[MEZ.mon12 - 1][0], d_PosX - 39, 0); //month
            char2Arr(M_arr[MEZ.mon12 - 1][1], d_PosX - 43, 0);
            char2Arr(M_arr[MEZ.mon12 - 1][2], d_PosX - 49, 0);
            char2Arr(M_arr[MEZ.mon12 - 1][3], d_PosX - 55, 0);
            char2Arr(M_arr[MEZ.mon12 - 1][4], d_PosX - 61, 0);
            char2Arr('2', d_PosX - 68, 0);                     //year
            char2Arr('0', d_PosX - 74, 0);
            char2Arr(48 + MEZ.jahr2, d_PosX - 80, 0);
            char2Arr(48 + MEZ.jahr1, d_PosX - 86, 0);

            refresh_display(); //alle 50ms
            if (f_scrollend_y == true) {
                f_scrollend_y = false;
            }
        } //end 50ms
        if (y == 0) {
            // do something else
        }
        server.handleClient();//监听客户请求并处理
    }  //end while(true)
    //this section can not be reached

  //MDNS.update();
}
