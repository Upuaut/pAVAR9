//
//
// Code modified by Arko for the Si406x
// Code based on KT5TK's Si446x
//
/* pecanpico2 Si446x Driver copyright (C) 2013  KT5TK
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define STATUS_LED 4 
#include "config.h"
#include <math.h>
#include "radio_si406x.h"
#include <SPI.h>
#include <util/delay.h> 


#if defined(ARDUINO) && ARDUINO >= 100
#include <Arduino.h>
#else
#include <WProgram.h>
#endif


unsigned int si406x_powerlevel = 0;
unsigned long active_freq = RADIO_FREQUENCY;


void SendCmdReceiveAnswer(int byteCountTx, int byteCountRx, const char* pData)
{
    

  if (byteCountTx == 1)
        byteCountTx++;
    
    digitalWrite(SSpin,LOW);
    char answer;   
    
    
    for (int j = 0; j < byteCountTx; j++) // Loop through the bytes of the pData
    {
      byte wordb = pData[j];
      SPI.transfer(wordb);  
    } 
    
    digitalWrite(SSpin,HIGH);


    _delay_us(20);

    digitalWrite(SSpin,LOW);   
    
    int reply = 0x00;

    while (reply != 0xFF)
    {  

       reply = SPI.transfer(0x44);
       if (reply != 0xFF)
       {
//         digitalWrite(STATUS_LED,LOW);
         digitalWrite(SSpin,HIGH);
         _delay_us(20);
         digitalWrite(SSpin,LOW);   
  //       digitalWrite(STATUS_LED,HIGH);     
       }
    }
    for (int k = 1; k < byteCountRx; k++)
    {
      SPI.transfer(0x44);
    }
       
    digitalWrite(SSpin,HIGH);
    _delay_ms(500); // Wait half a second to prevent Serial buffer overflow
}


// Config reset ----------------------------------------------------------
void resetradio(void) 
{
  // Start talking to the SI406X radio chip

  const char PART_INFO_command[] = {0x01}; // Part Info
  SendCmdReceiveAnswer(1, 9, PART_INFO_command);

//divide VCXO_FREQ into its bytes; MSB first  
  unsigned int x3 = VCXO_FREQ / 0x1000000;
  unsigned int x2 = (VCXO_FREQ - x3 * 0x1000000) / 0x10000;
  unsigned int x1 = (VCXO_FREQ - x3 * 0x1000000 - x2 * 0x10000) / 0x100;
  unsigned int x0 = (VCXO_FREQ - x3 * 0x1000000 - x2 * 0x10000 - x1 * 0x100); 

//POWER_UP
  const char init_command[] = {0x02, 0x01, 0x01, x3, x2, x1, x0};// no patch, boot main app. img, FREQ_VCXO, return 1 byte
  SendCmdReceiveAnswer(7, 1 ,init_command); 

  
// Radio Ready
  const char get_int_status_command[] = {0x20, 0x00, 0x00, 0x00}; //  Clear all pending interrupts and get the interrupt status back
  SendCmdReceiveAnswer(4, 9, get_int_status_command);

// Switch off LED and do GPIO config 
  const char gpio_pin_cfg_command[] = {0x13, 0x02, 0x03, 0x02, 0x02, 0x08, 0x11, 0x00}; //  Set all GPIOs LOW; Link NIRQ to CTS; Link SDO to MISO; Max drive strength
  SendCmdReceiveAnswer(8, 8, gpio_pin_cfg_command);

// Set Frequency  
  setFrequency(active_freq,20);

// Set to CW mode 
  setModem(); 
  
// TX Tune  
  tune_tx();
  
}

void start_tx()
{
  char change_state_command[] = {0x34, 0x07}; //  Change to TX state
  SendCmdReceiveAnswer(2, 1, change_state_command);

}

void stop_tx()
{
  char change_state_command[] = {0x34, 0x03}; //  Change to Ready state
  SendCmdReceiveAnswer(2, 1, change_state_command);

}

void tune_tx()
{
  char change_state_command[] = {0x34, 0x05}; //  Change to TX tune state
  SendCmdReceiveAnswer(2, 1, change_state_command);

}




// Configuration parameter functions ---------------------------------------

void setModem()
{
  // Set to CW mode
  char set_modem_mod_type_command[] = {0x11, 0x20, 0x01, 0x00, 0xAA}; // Setting to FSK, it was orginally CW {0x11, 0x20, 0x01, 0x00, 0x00};
  SendCmdReceiveAnswer(5, 1, set_modem_mod_type_command);
  
}



void setFrequency(unsigned long freq, unsigned long step_size_hz)
{ 
  
  // Set the output divider according to recommended ranges given in si406x datasheet  
  int outdiv = 4;
  int band = 0;
  if (freq < 705000000UL) { outdiv = 6;  band = 1;};
  if (freq < 525000000UL) { outdiv = 8;  band = 2;};
  if (freq < 353000000UL) { outdiv = 12; band = 3;};
  if (freq < 239000000UL) { outdiv = 16; band = 4;};
  if (freq < 177000000UL) { outdiv = 24; band = 5;};
  
 
  unsigned long f_pfd = 2 * VCXO_FREQ / outdiv;
  
  unsigned int n = ((unsigned int)(freq / f_pfd)) - 1;
  
  float ratio = (float)freq / (float)f_pfd;
  float rest  = ratio - (float)n;
  

  unsigned long m = (unsigned long)(rest * 524288UL); 
 

// set the band parameter
  unsigned int sy_sel = 8; // 
  char set_band_property_command[] = {0x11, 0x20, 0x01, 0x51, (band + sy_sel)}; //   
  // send parameters
  SendCmdReceiveAnswer(5, 1, set_band_property_command);

// Set the pll parameters
  unsigned int m2 = m / 0x10000;
  unsigned int m1 = (m - m2 * 0x10000) / 0x100;
  unsigned int m0 = (m - m2 * 0x10000 - m1 * 0x100); 


// Set step size
  unsigned long step_size = (524288UL*outdiv*step_size_hz)/(2*VCXO_FREQ);
  unsigned char step1 = (unsigned char)((step_size << 8) >> 8);
  unsigned char step0 = (unsigned char)(step_size >> 8);

  // assemble parameter string
  char set_frequency_property_command[] = {0x11, 0x40, 0x06, 0x00, n, m2, m1, m0, step1, step0};
  // send parameters
  SendCmdReceiveAnswer(10, 1, set_frequency_property_command);
  
// Set frequency deviation
  char set_frequency_separation[] = {0x11, 0x20, 0x03, 0x0a, 0x00, 0x00, 0x22}; // Controls RTTY Shift
  // send parameters
  SendCmdReceiveAnswer(7, 1, set_frequency_separation);

}

void setChannel(unsigned char channel)
{
// Set Channel
  char set_channel_command[] = {0x31, 0x01, 0x01, channel};
  // send parameters
  SendCmdReceiveAnswer(4, 1, set_channel_command);
}


// Public functions -----------------------------------------------------------

void startup()
{
// Not much radio configuration to do here
// because we initialize the transmitter each time right before we transmit a packet
  
  // Set up SPI  
  pinMode(SCKpin,  OUTPUT);
  pinMode(SSpin,   OUTPUT);
  pinMode(MOSIpin, OUTPUT);
  pinMode(MISOpin, INPUT_PULLUP);
  digitalWrite(SSpin, HIGH);  // ensure SS stays high for now
  
  // initialize SPI:
  SPI.begin(); 
  SPI.setDataMode(SPI_MODE0);
  SPI.setBitOrder(MSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV8); // 8 MHz / 8 = 1 MHz
  _delay_ms(600);

}

void ptt_on()
{
  
  resetradio();
  
  // turn on the blue LED (GPIO2) to indicate TX
  char gpio_pin_cfg_command2[] = {0x13, 0x02, 0x03, 0x03, 0x02, 0x08, 0x11, 0x00}; //  Set GPIO2 HIGH; Link NIRQ to CTS; Link SDO to MISO; Max drive strength
  SendCmdReceiveAnswer(8, 1, gpio_pin_cfg_command2);

  start_tx();
  si406x_powerlevel = 1023;
}

void ptt_off()
{
  stop_tx();
  si406x_powerlevel = 0;
  // turn off the blue LED (GPIO2)
  char gpio_pin_cfg_command0[] = {0x13, 0x02, 0x02, 0x02, 0x02, 0x08, 0x11, 0x00}; //  Set all GPIOs LOW; Link NIRQ to CTS; Link SDO to MISO; Max drive strength
  SendCmdReceiveAnswer(8, 1, gpio_pin_cfg_command0);
}

void set_freq(unsigned long freq)
{
  active_freq = freq;
}


int get_powerlevel()
{
  return si406x_powerlevel;

}

