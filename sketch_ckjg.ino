/*
   天猫精灵控制
   2020-05-12
   QQ交流群：824273231
   官网https://bemfa.com
*/
#include <ESP8266WiFi.h>
#include <WiFiClient.h>


//巴法云服务器地址默认即可
#define TCP_SERVER_ADDR "bemfa.com"
//服务器端口，tcp创客云端口8344
#define TCP_SERVER_PORT "8344"

//********************需要修改的部分*******************//

//WIFI名称，区分大小写，不要写错
#define DEFAULT_STASSID  "野蛮"
//WIFI密码
#define DEFAULT_STAPSW   "00000000"
//用户私钥，可在控制台获取,修改为自己的UID
String UID = "4d3ae1cd19bb4248f3a70cd6a2ca651f";
//主题名字，可在控制台新建
String TOPIC = "switch";
//单片机LED引脚值
const int LED_Pin = 14;//控制开关灯的引脚
const int led = 2;//指示灯，用来判断服务器连接状态
const int btn = 5;//按钮，用来切换开关灯

bool is_btn = 0;//按钮的标志位，用来逻辑处理对比，判断按钮有没有改变状态
//**************************************************//

//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值30s
#define KEEPALIVEATIME 30*1000



//tcp客户端相关初始化，默认即可
WiFiClient TCPclient;
String TcpClient_Buff = "";
unsigned int TcpClient_BuffIndex = 0;
unsigned long TcpClient_preTick = 0;
unsigned long preHeartTick = 0;//心跳
unsigned long preTCPStartTick = 0;//连接
bool preTCPConnected = false;

void setup() 
{
  Serial.begin(115200);
  pinMode(btn, INPUT_PULLUP);
  pinMode(LED_Pin, OUTPUT);
  digitalWrite(LED_Pin, 0);
  pinMode(led, OUTPUT);
  digitalWrite(led, 1);
  is_btn = digitalRead(btn);
}

void loop() 
{
  doWiFiTick();
  doTCPClientTick();
  BtnEvent();
}

//启动WiFi连接
void startSTA() 
{
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_STASSID, DEFAULT_STAPSW);

}

//检查WiFi是否连接上
void doWiFiTick() 
{
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
    Serial.printf("Heap size:%d\r\n", ESP.getFreeHeap());
  }

  //未连接1s重连
  if ( WiFi.status() != WL_CONNECTED ) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
  }
  //连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      startTCPClient();
    }
  }
}

//初始化和服务器建立连接
void startTCPClient() 
{
  if (TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT))) {
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n", TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT));
    char tcpTemp[128];
    //sprintf(tcpTemp, "cmd=1&uid=%s&topic=%s\r\n", UID, TOPIC);
    String to_data = "";
    to_data = "cmd=1&uid=" + UID + "&topic=" + TOPIC + "\r\n";
    sendtoTCPServer(to_data);
    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  }
  else {
    Serial.print("Failed connected to server:");
    Serial.println(TCP_SERVER_ADDR);
    TCPclient.stop();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}

//发送数据到TCP服务器
void sendtoTCPServer(String p) 
{

  if (!TCPclient.connected())
  {
    Serial.println("客户端未连接到服务器");
    return;
  }
  TCPclient.print(p);
  Serial.print("向服务器发送数据：");
  Serial.println(p);
}

//向服务器反馈当前状态，data的值：on是打开，off是关闭
void send_State(String data)
{
  String To_data = "";
  //To_data = "cmd=2&uid=c02b3f812c2aab9d9bdf1d22f8680f2a&topic=my1led002&msg=#" + data + "#\r\n";
  To_data = "cmd=2&uid=c02b3f812c2aab9d9bdf1d22f8680f2a&topic=my1led002&msg=" + data + "\r\n";
  //To_data="cmd=2&uid="+UID+"&topic="+TOPIC+"&msg=#"+data+"#\r\n";
  sendtoTCPServer(To_data);
}

//检查数据，发送心跳
void doTCPClientTick() 
{
  //检查是否断开，断开后重连
  if (WiFi.status() != WL_CONNECTED) return;

  if (!TCPclient.connected()) {//断开重连
    digitalWrite(led, 1);
    if (preTCPConnected == true) {

      preTCPConnected = false;
      preTCPStartTick = millis();
      Serial.println();
      Serial.println("TCP Client disconnected.");
      TCPclient.stop();
    }
    else if (millis() - preTCPStartTick > 1 * 1000) //重新连接
      startTCPClient();
  }
  else
  {
    digitalWrite(led, 0);
    if (TCPclient.available()) {//收数据
      char c = TCPclient.read();
      TcpClient_Buff += c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();

      if (TcpClient_BuffIndex >= MAX_PACKETSIZE - 1) {
        TcpClient_BuffIndex = MAX_PACKETSIZE - 2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if (millis() - preHeartTick >= KEEPALIVEATIME) { //保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("cmd=0&msg=keep\r\n");
    }
  }
  if ((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick >= 200))
  { //data ready
    TCPclient.flush();
    Serial.println("Buff");
    Serial.println(TcpClient_Buff);
    if ((TcpClient_Buff.indexOf("&msg=on") > 0)) {
      turnOnLed();
    } else if ((TcpClient_Buff.indexOf("&msg=off") > 0)) {
      turnOffLed();
    }
    TcpClient_Buff = "";
    TcpClient_BuffIndex = 0;
  }
}

//打开灯泡
void turnOnLed() 
{
  Serial.println("网络指令开灯");
  digitalWrite(LED_Pin, 1);
}

//关闭灯泡
void turnOffLed() 
{
  Serial.println("网络指令关灯");
  digitalWrite(LED_Pin, 0);
}

//监听按钮状态，执行相应处理
void BtnEvent()
{
  bool is = digitalRead(btn);
  if ( is != is_btn)
  {
    bool is_led = digitalRead(LED_Pin);
    digitalWrite(LED_Pin, !is_led);
    if (is_led)
    {
      send_State("off");
      Serial.println("按钮对灯已执行关闭");
    }
    else
    {
      send_State("on");
      Serial.println("按钮对灯已执行打开");
    }
    delay(200);
    is_btn = digitalRead(btn);
  }
}
