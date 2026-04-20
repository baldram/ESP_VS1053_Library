/**
  A simple example to use ESP_VS1053_Library for MIDI
  https://github.com/baldram/ESP_VS1053_Library

  Copyright (C) 2021 M. Hund (github.com/Dr-Dawg)

  Licensed under GNU GPLv3 <http://gplv3.fsf.org/>
 
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License or later version.
 
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  Wiring:
  --------------------------------
  | VS1053  | ESP8266 |  ESP32   |
  --------------------------------
  |   SCK   |   D5    |   IO18   |
  |   MISO  |   D6    |   IO19   |
  |   MOSI  |   D7    |   IO23   |
  |   XRST  |   RST   |   EN     |
  |   CS    |   D1    |   IO5    |
  |   DCS   |   D0    |   IO16   |
  |   DREQ  |   D3    |   IO4    |
  |   5V    |   5V    |   5V     |
  |   GND   |   GND   |   GND    |
  --------------------------------

  Note: It's just an example, you may use a different pins definition.
 

  To run this example define the platformio.ini as below.

  [env:nodemcuv2]
  platform = espressif8266
  board = nodemcuv2
  framework = arduino
  lib_deps =
    ESP_VS1053_Library

  [env:esp32dev]
  platform = espressif32
  board = esp32dev
  framework = arduino
  lib_deps =
    ESP_VS1053_Library

*/

// This ESP_VS1053_Library
#include <VS1053.h>

#include "patches/rtmidi1003b.h"
#include "patches/rtmidi1053b.h"

// Wiring of VS1053 board (SPI connected in a standard way)
#ifdef ARDUINO_ARCH_ESP8266
#define VS1053_CS     D1
#define VS1053_DCS    D0
#define VS1053_DREQ   D3
#endif

#ifdef ARDUINO_ARCH_ESP32
#define VS1053_CS     5
#define VS1053_DCS    16
#define VS1053_DREQ   4
#endif

#define VOLUME  100 // volume level 0-100

VS1053 player(VS1053_CS, VS1053_DCS, VS1053_DREQ);

// noteOn and noteOff based on MP3_Shield_RealtimeMIDI demo 
// by Matthias Neeracher, Nathan Seidle's Sparkfun Electronics example respectively

//Send a MIDI note-on message.  Like pressing a piano key
void noteOn(uint8_t channel, uint8_t note, uint8_t attack_velocity) {
  player.sendMidiMessage( (0x90 | channel), note, attack_velocity);
}

//Send a MIDI note-off message.  Like releasing a piano key
void noteOff(uint8_t channel, uint8_t note, uint8_t release_velocity) {
  player.sendMidiMessage( (0x80 | channel), note, release_velocity);
}

void setup() {
  
    Serial.begin(115200);

    // initialize SPI
    SPI.begin();
                           
    // initialize the player
    player.begin();  

    if(player.getChipVersion() == 4)
       Serial.println("Hello VS1053!");
    else if(player.getChipVersion()==3)       
       Serial.println("Hello VS1003!"); 
    else
       Serial.println("Please check whether your device is properly connected!");         

  
    if(player.getChipVersion()==4 /*4*/) { // MIDI using a VS1053 chip
        player.loadUserCode(MIDI1053, MIDI1053_SIZE); 
        player.writeRegister(0xA /*SCI_AIADDR*/, 0x50);  // setting VS1053 Start adress for user code
        Serial.println(" MIDI plugin VS1053 loaded");                 
    }
             
    if(player.getChipVersion()==3) { // MIDI using a VS1003 chip
        player.loadUserCode(MIDI1003, MIDI1003_SIZE);      
        player.writeRegister(0xA /*SCI_AIADDR*/, 0x30);  // setting VS1003 Start adress for user code             
        Serial.println(" MIDI plugin VS1003 loaded");                     
    }     

    player.setVolume(VOLUME);  

}

void loop() {

 uint8_t channel          = (uint8_t) random(16);
 //channel                  = 9; //uncomment this, if you just want to hear percussion
 uint8_t instrument       = (uint8_t) random(128);
 uint8_t note             = (uint8_t) random(128);
 uint8_t attack_velocity  = (uint8_t) random(128);
 uint8_t release_velocity = (uint8_t) random(128);
 unsigned long duration   = (unsigned long) (300 + random(2000));
 

/**  MIDI messages, 0x80 to 0xEF Channel Messages,  0xF0 to 0xFF System Messages
 *   a MIDI message ranges from 1 byte to three bytes
 *   the first byte consists of 4 command bits and 4 channel bits, i.e. 16 channels
 *   
 *   0x80     Note Off
 *   0x90     Note On
 *   0xA0     Aftertouch
 *   0xB0     Continuous controller
 *   0xC0     Patch change
 *   0xD0     Channel Pressure
 *   0xE0     Pitch bend
 *   0xF0     (non-musical commands)
 */

/** 0xB0 Continuous controller commands, 0-127
 *  0 Bank Select (MSB)
 *  1 Modulation Wheel
 *  2 Breath controller
 *  3 Undefined
 *  4 Foot Pedal (MSB)
 *  5 Portamento Time (MSB)
 *  6 Data Entry (MSB)
 *  7 Volume (MSB)
 *  8 Balance (MSB)
 *  9 Undefined
 *  10 Pan position (MSB)
 *  11 Expression (MSB)
 *  ...
 */

  // Continuous controller, set channel volume to high, i.e. 127
  player.sendMidiMessage(0xB0| channel, 0x07, 127); 

 // Continuous controller 0, bank select: 0 gives you the default bank depending on the channel
 // 0x78 (percussion) for Channel 10, i.e. channel = 9 , 0x79 (melodic)  for other channels
  player.sendMidiMessage(0xB0| channel, 0, 0x00); //0x00 default bank 

  // Patch change, select instrument
  player.sendMidiMessage(0xC0| channel, instrument, 0);

// Serial output for the actual parameters
 Serial.print("Channel: ");
 Serial.println(channel, DEC);
 Serial.print("Instrument: ");
 Serial.println(instrument, DEC);
 Serial.print("Note: ");
 Serial.println(note, DEC); 
 Serial.print("Attack: ");
 Serial.println(attack_velocity, DEC);  
 Serial.print("Release: ");
 Serial.println(release_velocity, DEC);   
 Serial.print("Duration: ");
 Serial.print(duration, DEC);   
 Serial.println(" milliseconds: ");  
 Serial.println("--------------------------------------");   

  //playing one (pseudo-)random note
  noteOn(channel, note, attack_velocity);
  delay(duration);
  noteOff(channel, note, release_velocity);
  delay(100);
  
}
