#include <Arduino.h>
#include <VS1053.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#define VS1053_CS     D8
#define VS1053_DCS    D4
#define VS1053_DREQ   D3
#endif
#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
#endif

#define VOLUME        75

#define VS_BUFF_SIZE  32
#define RING_BUF_SIZE 30000
#define CLIENT_BUFF   2048
#define HTTPVER       "HTTP/1.1" //use HTTP/1.0 to force not chunked transfer encoding

//for esp8266 use build_flags = -D PIO_FRAMEWORK_ARDUINO_LWIP2_HIGHER_BANDWIDTH

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;

const char* ssid = "TP-Link";
const char* password = "xxxxxxxx";
     
const char *host = "chisou-02.cdn.eurozet.pl";
const char *path = "/;";
int httpPort = 8112;

unsigned long logLoopCounter = 0;

uint16_t rcount = 0;
uint8_t* ringbuf;
uint16_t rbwindex = 0;
uint8_t* mp3buff;
uint16_t rbrindex = RING_BUF_SIZE - 1;

inline bool ringspace() {
  return ( rcount < RING_BUF_SIZE );     
}

void putring(uint8_t b ) {
  *(ringbuf + rbwindex) = b;
  if ( ++rbwindex == RING_BUF_SIZE ) {
    rbwindex = 0;
  }
  rcount++;
}
uint8_t getring() {
  if ( ++rbrindex == RING_BUF_SIZE ) {
    rbrindex = 0;
  }
  rcount--;
  return *(ringbuf + rbrindex);
}

void playRing(uint8_t b) {
  static int bufcnt = 0;
  mp3buff[bufcnt++] = b;
  if (bufcnt == sizeof(mp3buff)) {
    player.playChunk(mp3buff, bufcnt); 
    bufcnt = 0; 
  }  
}	

void setup() {
  Serial.begin(115200);
  mp3buff = (uint8_t *) malloc (VS_BUFF_SIZE);
  ringbuf = (uint8_t *) malloc (RING_BUF_SIZE);
  SPI.begin();
  player.begin();
  player.switchToMp3Mode();
  player.setVolume(VOLUME);
  WiFi.begin(ssid, password);      
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (!client.connect(host, httpPort)) {
    Serial.println("Connection failed");
    return;
  }    
  client.print(String("GET ")+path+" "+HTTPVER+"\r\nHost: "+host+"\r\nConnection: close\r\n\r\n");
  Serial.println("Start playing");
}

void loop() {
  uint32_t maxfilechunk;
  unsigned long nowMills = millis();
	
  if(!client.connected()){
    Serial.println("Reconnecting...");
    if(client.connect(host, httpPort)){
      client.print(String("GET ")+path+" "+HTTPVER+"\r\nHost: "+host+"\r\nConnection: close\r\n\r\n");
    }
  }

  maxfilechunk = client.available();
  if(maxfilechunk > 0){
    if ( maxfilechunk > CLIENT_BUFF ) {
      maxfilechunk = CLIENT_BUFF;
    }		
    while ( ringspace() && maxfilechunk-- ) {
      putring(client.read());
    }
    yield();
  }
  while (rcount && (player.data_request())) {
    playRing(getring());
  }	  
  if ((nowMills - logLoopCounter) >= 500) {
    Serial.printf("Buffer: %d%%\r", rcount * 100 / RING_BUF_SIZE);
    logLoopCounter = nowMills;
  }
}
