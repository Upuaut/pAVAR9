/*
 PAVA R9 Tracker Code
 SI4060 Based RF Module
 
 By Anthony Stirk M0UPU 
 
 December 2013
 Subversion 1.00 (Initial Commit)
 
 Thanks and credits :
 
 GPS Code from jonsowman and Joey flight computer CUSF
 https://github.com/cuspaceflight/joey-m/tree/master/firmware
 
 SI4060 Code Code modified by Ara Kourchians for the Si406x originally based on
 KT5TK's Si446x code. 
 
 Big thanks to Dave Akerman, Phil Heron, Mark Jessop, Leo Bodnar for suggestions
 ideas and assistance. 
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 See <http://www.gnu.org/licenses/>.
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/crc16.h>
#include <SPI.h>
#define F_CPU 2000000

#include "config.h"
#include "radio_si406x.h"

uint8_t buf[60]; 
char txstring[80];
volatile int txstatus=1;
volatile int txstringlength=0;
volatile char txc;
volatile int txi;
volatile int txj;
volatile int count=1;
volatile boolean lockvariables = 0;
uint8_t lock =0, sats = 0, hour = 0, minute = 0, second = 0;
uint8_t oldhour = 0, oldminute = 0, oldsecond = 0;
int navmode = 0, GPSerror = 0, lat_int=0,lon_int=0;
int32_t lat = 0, lon = 0, alt = 0, maxalt = 0, lat_dec = 0, lon_dec =0;
int psm_status = 0, batteryadc_v=0;
int32_t tslf=0;
int errorstatus=0; 
/* Bit 0 = GPS Error Condition Noted Switch to Max Performance Mode
 Bit 1 = GPS Error Condition Noted Cold Boot GPS
 Bit 2 = Reserved
 Bit 3 = Current Dynamic Model 0 = Flight 1 = Pedestrian
 Bit 4 = PSM Status 0 = PSM On 1 = PSM Off                   
 Bit 5 = Lock 0 = GPS Locked 1= Not Locked
 */


void setup() {
  pinMode(STATUS_LED, OUTPUT); 
  pinMode(BATTERY_ADC, INPUT);
  pinMode(SHUTDOWN_SI406x_PIN, OUTPUT);
  digitalWrite(SHUTDOWN_SI406x_PIN, LOW);
  blinkled(2);
  wait(500);
  blinkled(1);
  startup();
  ptt_on();
  analogWrite(AUDIO_PIN, 127);
}

void loop()
{
  setChannel(0);
  start_tx();
  delay(250);
  stop_tx();
  delay(50);
  setChannel(2);
  start_tx();
  delay(250);
  stop_tx();
  delay(50);
}


void blinkled(int blinks)
{
  for(int blinkledx = 0; blinkledx <= blinks; blinkledx++) {
    digitalWrite(STATUS_LED,HIGH);
    wait(100);
    digitalWrite(STATUS_LED,LOW);
    wait(100);
  }    
}    

void wait(unsigned long delaytime) // Arduino Delay doesn't get CPU Speeds below 8Mhz
{
  unsigned long _delaytime=millis();
  while((_delaytime+delaytime)>=millis()){
  }
}

