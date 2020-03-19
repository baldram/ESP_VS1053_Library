#include <VS1053.h>
#ifdef ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3
#endif

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
#endif

// Default volume
#define VOLUME  80

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;

const char *ssid = "TP-Link";
const char *password = "xxxxxxxx";

// this is the audio stream which uses chunked transfer
const char *host = "icecast.radiofrance.fr";
const char *path = "/franceculture-lofi.mp3";
int httpPort = 80;

// The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
uint8_t mp3buff[64];

void setup() {
  Serial.begin(115200);

  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly
  delay(3000);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  Serial.println("\n\nSimple Radio Node WiFi Radio");

  SPI.begin();

  player.begin();
  player.switchToMp3Mode();
  player.setVolume(VOLUME);

  Serial.print("Connecting to SSID ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.setAutoConnect(true);
  WiFi.setAutoReconnect(true);
  WiFi.reconnect();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.print("connecting to ");
  Serial.println(host);

  if (!client.connect(host, httpPort)) {
    Serial.println("Connection failed");
    return;
  }

  Serial.print("Requesting stream: ");
  Serial.println(path);

  client.print(String("GET ") + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
}

void loop() {
  if (!client.connected()) {
    Serial.println("Reconnecting...");
    if (client.connect(host, httpPort)) {
      client.print(String("GET ") + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Connection: close\r\n\r\n");
    }
  }

  if (client.available() > 0) {
    // The buffer size 64 seems to be optimal. At 32 and 128 the sound might be brassy.
    uint8_t bytesread = client.read(mp3buff, 64);
    player.playChunk(mp3buff, bytesread);
  }
}
