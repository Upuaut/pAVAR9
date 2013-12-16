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
#include "dominoexvaricode.h"
#define F_CPU 2000000

#include "config.h"
#include "radio_si406x.h"
static char _txstring[120];
static volatile char _txstatus = 0;
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

char debug_count = 0;

void setup() {
  pinMode(STATUS_LED, OUTPUT); 
  pinMode(4,OUTPUT);
  digitalWrite(4,LOW);
  
  pinMode(AUDIO_PIN, OUTPUT);
  digitalWrite(AUDIO_PIN, LOW);
  
  Serial.begin(9600);
  wait(500);

  pinMode(SHUTDOWN_SI406x_PIN, OUTPUT);
  digitalWrite(SHUTDOWN_SI406x_PIN, LOW);
  
  startup();
  ptt_on();
  
  digitalWrite(AUDIO_PIN, HIGH);
  ptt_off();
  initialise_interrupt();
}

void loop()
{
	/* wait for the modem to finish */
	while(_txstatus);
	
	/* Transmit a string */
	snprintf(_txstring, 100, "THIS IS JUST A TEST\n");
	_txstatus = 1;
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
void initialise_interrupt() 
{
	// initialize Timer1
	cli();          // disable global interrupts
	TCCR1A = 0;     // set entire TCCR1A register to 0
	TCCR1B = 0;     // same for TCCR1B
	//  OCR1A = F_CPU / 1024 / BAUDRATE - 1;  // set compare match register to desired timer count:
	OCR1A = F_CPU/16000-1; // DOMINOEX16
//	OCR1A = F_CPU/22050-1; // DOMINOEX22
//	OCR1A = F_CPU/8000-1; // DOMINOEX8
//	OCR1A = F_CPU/4000-1; // DOMINOEX4
	TCCR1B |= (1 << WGM12);   // turn on CTC mode:
	// Set CS10 and CS12 bits for:
	TCCR1B |= (1 << CS10);
	TCCR1B |= (1 << CS12);
	// enable timer compare interrupt:
	TIMSK1 |= (1 << OCIE1A);
	sei();          // enable global interrupts
}

ISR(TIMER1_COMPA_vect)
{
      //  digitalWrite(STATUSLED, !digitalRead(STATUSLED)); 
        static uint8_t sym = 0;  /* Currently transmitting symbol */
	static uint8_t c = 0x00; /* Current character */
	static uint8_t s = 0;    /* Current symbol index */
	static char *p = 0;      /* Pointer to primary tx buffer */
	
	uint8_t nsym;
	uint8_t dacbuf[3];
	uint16_t dacval;
	
	/* Fetch the next symbol */
	nsym = varicode[c][s++];
	
	/* Update the transmitting symbol */
	sym = (sym + 2 + nsym) % 18;
	

        setChannel(sym);
	
	/* Check if this character has less than 3 symbols */
	if(s < 3 && !(varicode[c][s] & 0x08)) s = 3;
	
	/* Still more symbols to send? Exit interrupt */
	if(s != 3) return;
	
	/* We're done with this character, fetch the next one */
	s = 0;
	
	switch(_txstatus)
	{
	case 0:
		/* Nothing is ready for us, transmit a NUL */
		c = 0x00;
		break;
	
	case 1:
		/* 1 signals a new primary string is ready */
		p = _txstring;
		_txstatus++;
		/* Fall through... */
	
	case 2:
		/* Read the next character from the primary buffer */
		c = (uint8_t) *(p++);
		
		/* Reached the end of the string? */
		if(c == 0x00) _txstatus = 0;
		
		break;
	}
}
