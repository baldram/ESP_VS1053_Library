/**
 * A chunked stream handler to play web radio stations using ESP8266.
 * It shows how to remove chunk control data from the stream to avoid the sound glitches.
 *
 * More technical details about chunked transfer:
 * https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
 * https://en.wikipedia.org/wiki/Chunked_transfer_encoding
 *
 * Github discussion related to this demo:
 * https://github.com/baldram/ESP_VS1053_Library/issues/47
 */

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
#define BUFFER_SIZE 64
uint8_t mp3buff[BUFFER_SIZE];

uint8_t remove_chunk_control_data(uint8_t *data, size_t length);

void setup() {
  Serial.begin(115200);

  // Wait for VS1053 and PAM8403 to power up
  // otherwise the system might not start up correctly
  delay(3000);

  // This can be set in the IDE no need for ext library
  // system_update_cpu_freq(160);

  Serial.println("\n\nChunked Transfer Radio Node WiFi Radio");

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
    uint8_t bytesread = client.read(mp3buff, BUFFER_SIZE);
    bytesread = remove_chunk_control_data(mp3buff, bytesread);
    player.playChunk(mp3buff, bytesread);
  }
}

// Introduce here a new helper buffer with size of 8 bytes, to remove the chunked control bytes.
// Variables must be aligned to avoid the Exception (9) being thrown from the ESP8266
// See here: https://arduino-esp8266.readthedocs.io/en/latest/exception_causes.html
// See here: https://arduino.stackexchange.com/questions/67442/nodemcu-1-0-exception-9-fatal-exception-9loadstorealignmentcause
uint8_t __attribute__((aligned(4))) helperBuffer[8];
uint8_t __attribute__((aligned(4))) helperBufferCount = 0;

/***
 * Removes the chunk control data from the helper buffer.
 *
 * Only the following chunk control bytes are removed:
 * \r\n<byte>\r\n
 * \r\n<byte><byte>\r\n
 * \r\n<byte><byte><byte>\r\n
 * \r\n<byte><byte><byte><byte>\r\n
 *
 */
void remove_chunk_control_data_from_helper_buffer() {
  if (helperBuffer[0] != '\r') {
    // die fast
    return;
  }
  if (helperBuffer[1] != '\n') {
    // die fast
    return;
  }

  if (helperBuffer[3] == '\r' && helperBuffer[4] == '\n') {
    // 5 bytes length chunk control section discovered
    // \r\n<byte>\r\n
    helperBufferCount = 3;
    Serial.println("Removed control data: 5 bytes");

    return;
  }

  if (helperBuffer[4] == '\r' && helperBuffer[5] == '\n') {
    // 6 bytes length chunk control section discovered
    // \r\n<byte><byte>\r\n
    helperBufferCount = 2;
    Serial.println("Removed control data: 6 bytes");

    return;
  }

  if (helperBuffer[5] == '\r' && helperBuffer[6] == '\n') {
    // 7 bytes length chunk control section discovered
    // \r\n<byte><byte><byte>\r\n
    helperBufferCount = 1;
    Serial.println("Removed control data: 7 bytes");

    return;
  }

  if (helperBuffer[6] == '\r' && helperBuffer[7] == '\n') {
    // 8 bytes length chunk control section discovered
    // \r\n<byte><byte><byte><byte>\r\n
    helperBufferCount = 0;
    Serial.println("Removed control data: 8 bytes");

    return;
  }
}

/***
 * Puts a byte to the input of helper buffer and returns a byte from the output of helper buffer.
 * In the meantime it tries to remove the chunk control data (if any available) from the buffer.
 *
 * @param incoming byte of the audio stream
 * @return outgoing byte of the audio stream (if available) or -1 when no bytes available (this happens
 * when the buffer is being populated with the data or after removal of chunk control data)
 */
int16_t put_through_helper_buffer(uint8_t newValue) {
  //
  int16_t result = -1;

  if (helperBufferCount == 8) {
    result = helperBuffer[0];
  }

  helperBuffer[0] = helperBuffer[1];
  helperBuffer[1] = helperBuffer[2];
  helperBuffer[2] = helperBuffer[3];
  helperBuffer[3] = helperBuffer[4];
  helperBuffer[4] = helperBuffer[5];
  helperBuffer[5] = helperBuffer[6];
  helperBuffer[6] = helperBuffer[7];
  helperBuffer[7] = newValue;

  helperBufferCount++;
  if (helperBufferCount > 8) {
    helperBufferCount = 8;
  }

  remove_chunk_control_data_from_helper_buffer();

  return result;
}

/***
 * Removes the chunk control data from the input audio stream. Data are written back to the input
 * buffer and the number of bytes available (after processing) is returned.
 *
 * @see https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Transfer-Encoding
 * @see https://en.wikipedia.org/wiki/Chunked_transfer_encoding
 *
 * @param data a pointer to the input buffer of audio stream
 * @param length a number of input bytes to be processed
 * @return the number of available bytes in the input buffer after removing the chunk control data
 */
uint8_t remove_chunk_control_data(uint8_t *data, size_t length) {

  uint8_t writeindex = 0;
  uint8_t index = 0;
  for (index = 0; index < length; index++) {
    uint8_t input = data[index];
    int16_t output = put_through_helper_buffer(input);
    if (output >= 0) {
      data[writeindex] = (uint8_t) output;
      writeindex++;
    }
  }

  return writeindex;
}
