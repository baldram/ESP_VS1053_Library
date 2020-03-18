#include <Arduino.h>
#include <VS1053.h>
#include <ESP8266WiFi.h>

#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3

#define VOLUME  80

#define VS_BUFF_SIZE 32
#define RING_BUF_SIZE 20000
#define CLIENT_BUFF   2048

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);
WiFiClient client;

const char *ssid = "TP-Link";
const char *password = "xxxxxxxx";

const char *host = "icecast.radiofrance.fr";
const char *path = "/franceculture-lofi.mp3";
int httpPort = 80;

uint16_t rcount = 0;
uint8_t *ringbuf;
uint16_t rbwindex = 0;
uint8_t *mp3buff;
uint16_t rbrindex = RING_BUF_SIZE - 1;

inline bool ringspace() {
	return (rcount < RING_BUF_SIZE);
}

void putring(uint8_t b) {
	*(ringbuf + rbwindex) = b;
	if (++rbwindex == RING_BUF_SIZE) {
		rbwindex = 0;
	}
	rcount++;
}
uint8_t getring() {
	if (++rbrindex == RING_BUF_SIZE) {
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
	mp3buff = (uint8_t*) malloc(VS_BUFF_SIZE);
	ringbuf = (uint8_t*) malloc(RING_BUF_SIZE);
	SPI.begin();
	player.begin();
	player.switchToMp3Mode();
	player.setVolume(VOLUME);
	WiFi.begin(ssid, password);
	WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
	WiFi.reconnect();
	while (WiFi.status() != WL_CONNECTED) {
		delay(500);
		Serial.print(".");
	}
	if (!client.connect(host, httpPort)) {
		Serial.println("Connection failed");
		return;
	}
	client.print(
			String("GET ") + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n"
					+ "Icy-MetaData: 0" + "\r\n" + "Connection: close\r\n\r\n");
}

void loop() {
	uint32_t maxfilechunk;

	if (!client.connected()) {
		Serial.println("Reconnecting...");
		if (client.connect(host, httpPort)) {
			client.print(
					String("GET ") + path + " HTTP/1.1\r\n" + "Host: " + host
							+ "\r\n" + "Icy-MetaData: 0" + "\r\n"
							+ "Connection: close\r\n\r\n");
		}
	}

	maxfilechunk = client.available();
	if (maxfilechunk > 0) {
		if (maxfilechunk > CLIENT_BUFF) {
			maxfilechunk = CLIENT_BUFF;
		}
		while (ringspace() && maxfilechunk--) {
			putring(client.read());
		}
		yield();
		while (rcount && (player.data_request())) {
			playRing(getring());
		}
	}
}
