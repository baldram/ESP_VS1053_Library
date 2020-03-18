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

// this is the audio stream which uses chunked transfer
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

// FIX CHUNKED GLITCHES BEGIN
// introduce here a new byte buffer in the middle with size of 8 bytes
uint8_t middleBuffer[8];
uint8_t middleBufferCount = 0;

/***
 * Searches the middle buffer for extra chunked control bytes and removes them if needed.
 */
void fix_middle_ring() {
	if (middleBuffer[0] != '\r') {
		return;
	}
	if (middleBuffer[1] != '\n') {
		return;
	}

	if (middleBuffer[4] == '\r' && middleBuffer[5] == '\n') {
		// we discover here control section which is 6 bytes length
		// \r\n<byte><byte>\r\n
		middleBufferCount = 2;
		Serial.println("Flush middle ring");

		return;
	}

	if (middleBuffer[5] == '\r' && middleBuffer[6] == '\n') {
		// we discover here control section which is 7 bytes length
		// \r\n<byte><byte><byte>\r\n
		middleBufferCount = 1;
		Serial.println("Flush middle ring");

		return;
	}

	if (middleBuffer[6] == '\r' && middleBuffer[7] == '\n') {
		// we discover here control section which is 8 bytes length
		// \r\n<byte><byte><byte><byte>\r\n
		middleBufferCount = 0;
		Serial.println("Flush middle ring");

		return;
	}
}

void put_middle_ring(uint8_t newValue) {
	if (middleBufferCount == 8) {
		putring(middleBuffer[0]);
	}

	middleBuffer[0] = middleBuffer[1];
	middleBuffer[1] = middleBuffer[2];
	middleBuffer[2] = middleBuffer[3];
	middleBuffer[3] = middleBuffer[4];
	middleBuffer[4] = middleBuffer[5];
	middleBuffer[5] = middleBuffer[6];
	middleBuffer[6] = middleBuffer[7];
	middleBuffer[7] = newValue;
	middleBufferCount++;
	if (middleBufferCount > 8) {
		middleBufferCount = 8;
	}

	fix_middle_ring();
}
// FIX CHUNKED GLITCHES END

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
			// FIX CHUNKED GLITCHES BEGIN
			// use the buffer in the middle to detect extra chunked control bytes
			put_middle_ring(client.read());
			// FIX CHUNKED GLITCHES END
		}
		yield();
		while (rcount && (player.data_request())) {
			playRing(getring());
		}
	}
}
