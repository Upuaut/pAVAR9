
/* An adaptation of the THOR modulator from fldigi */
/* GPL3+ */

#include "config.h"
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>
#include "thor.h"
#include "thor_varicode.h"

/* For Si radio channel setting */
#include "si406x.h"

/* THOR uses 18 tones */
#define TONES 18

/* Interleaver settings */
#define INTER_SIZE 4
#define INTER_LEN (THOR_INTER * INTER_SIZE * INTER_SIZE)
#define INTER_BYTES (INTER_LEN / 8)

/* THOR state */
static uint8_t _tone;
static uint16_t _conv_sh;
static uint8_t _preamble;
static uint8_t _inter_table[INTER_BYTES];
static uint16_t _inter_offset;

/* RSID state */
static uint8_t _rsid = 0;
PROGMEM static const char _rsid_msg[] = THOR_RSID;

/* Message being sent */
volatile static uint8_t  _txpgm;
volatile static uint8_t *_txbuf;
volatile static uint16_t _txlen;

static inline void _wtab(uint16_t i, uint8_t v)
{
	if(i >= INTER_LEN) i -= INTER_LEN;
	if(!v) _inter_table[i >> 3] &= ~(1 << (7 - (i & 7)));
	else   _inter_table[i >> 3] |= 1 << (7 - (i & 7));
}

/* Calculate the parity of byte x */
static inline uint8_t _parity(uint8_t x)
{
	x ^= x >> 4;
	x ^= x >> 2;
	x ^= x >> 1;
	return(x & 1);
}

static inline uint16_t _thor_lookup_code(uint8_t c, uint8_t sec)
{
	/* Primary character set */
	if(!sec) return(pgm_read_word(&_varicode[c]));
	
	/* Secondary character set (restricted range) */
	if(c >= ' ' && c <= 'z')
		return(pgm_read_word(&_varicode_sec[c - ' ']));
	
	/* Else return NUL */
	return(pgm_read_word(&_varicode[0]));
}

ISR(TIMER1_COMPA_vect)
{
	static uint16_t code;
	static uint8_t len = 0;
	uint8_t i, bit_sh;
	
	/* Transmit the tone */
	//si_set_channel(_tone);
	si_set_offset(_tone * 2);
	
	if(_preamble)
	{
		_tone = (_tone + 2);
		if(_tone >= TONES) _tone -= TONES;
		_preamble--;
		return;
	}
	
	/* Calculate the next tone */
	bit_sh = 0;
	for(i = 0; i < 2; i++)
	{
		uint8_t data;
		
		/* Done sending the current varicode? */
		if(!len)
		{
			if(_txlen)
			{
				/* Read the next character */
				if(_txpgm == 0) data = *(_txbuf++);
				else data = pgm_read_byte(_txbuf++);
				_txlen--;
			}
			else data = 0;
			
			/* Get the varicode for this character */
			code = _thor_lookup_code(data, 0);
			len  = code >> 12;
		}
		
		/* Feed the next bit into the convolutional encoder */
		_conv_sh = (_conv_sh << 1) | ((code >> --len) & 1);
		bit_sh = (bit_sh << 2)
		       | (_parity(_conv_sh & THOR_POLYA) << 1)
		       | _parity(_conv_sh & THOR_POLYB);
	}
	
	/* Add the new data to the interleaver */
	_wtab(_inter_offset +   0, bit_sh & 0x08);
	_wtab(_inter_offset +  41, bit_sh & 0x04);
	_wtab(_inter_offset +  82, bit_sh & 0x02);
	_wtab(_inter_offset + 123, bit_sh & 0x01);
	
	/* Read next symbol to transmit from the interleaver */
	bit_sh = _inter_table[_inter_offset >> 3];
	if(_inter_offset & 7) bit_sh &= 0x0F;
	else bit_sh >>= 4;
	
	/* Shift the interleaver table offset forward */
	_inter_offset = (_inter_offset + INTER_SIZE);
	if(_inter_offset >= INTER_LEN) _inter_offset -= INTER_LEN;
	
	/* Calculate the next tone */
	_tone = (_tone + 2 + bit_sh);
	if(_tone >= TONES) _tone -= TONES;
}

void thor_init(void)
{
	/* Clear the THOR state */
	_tone = 0;
	_conv_sh = 0;
	_txpgm = 0;
	_txbuf = NULL;
	_txlen = 0;
	_preamble = 16;
	_rsid = 15;
	
	memset(_inter_table, 0, INTER_BYTES);
	_inter_offset = 0;
	
	/* The modem is driven by TIMER1 in CTC mode */
	TCCR1A = 0; /* Mode 2, CTC */
	TCCR1B = _BV(WGM12) | _BV(CS12) | _BV(CS10); /* prescaler /1024 */
	OCR1A = F_CPU / 1024 / THOR_BAUDRATE - 1;
	TIMSK1 = _BV(OCIE1A); /* Enable interrupt */
}

void thor_stop(void)
{
	/* Flush the buffer */
	thor_data_P(PSTR("\0\0\0\0\0\0\0\0"), 8);
	thor_wait();
	
	/* Disable the interrupt */
	TIMSK1 = 0;
}

void inline thor_wait(void)
{
	/* Wait for interrupt driven TX to finish */
	while(_txlen > 0) while(_txlen > 0);
}

void thor_data(uint8_t *data, size_t length)
{
	thor_wait();
	_txpgm = 0;
	_txbuf = data;
	_txlen = length;
}

void thor_data_P(PGM_P data, size_t length)
{
	thor_wait();
	_txpgm = 1;
	_txbuf = (uint8_t *) data;
	_txlen = length;
}

void thor_string(char *s)
{
	uint16_t length = strlen(s);
	thor_data((uint8_t *) s, length);
}

void thor_string_P(PGM_P s)
{
	uint16_t length = strlen_P(s);
	thor_data_P(s, length);
}

