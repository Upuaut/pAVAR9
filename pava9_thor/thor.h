
#ifndef _THOR_H
#define _THOR_H

#ifdef __cplusplus
extern "C" {
#endif

/* Both THOR8 and THOR16 use 15.625 Hz frequency steps */

#if defined(THOR8)
  #define THOR_BAUDRATE 7.8125
  #define THOR_DS 1
  #define THOR_RSID "\xF0\xB1\x48\xE6\x25\xA7\xDC\x09"
#elif defined(THOR16)
  #define THOR_BAUDRATE 15.625
  #define THOR_DS 1
  #define THOR_RSID "\xF0\xC0\x3A\xF5\x63\xC5\x99\x0A"
#elif defined(THOR32)
  #define THOR_BAUDRATE 31.25
  #define THOR_DS 2
  #define THOR_RSID "" /* This is not a standard mode, no RSID! */
#endif

#define RSID_BAUDRATE 10.7666

#define THOR_K 7
#define THOR_POLYA 0x6D
#define THOR_POLYB 0x4F
#define THOR_INTER 10

#include <stdint.h>
#include <avr/pgmspace.h>

extern void thor_init(void);
extern void thor_stop(void);
extern void inline thor_wait(void);
extern void thor_data(uint8_t *data, size_t length);
extern void thor_data_P(PGM_P data, size_t length);
extern void thor_string(char *s);
extern void thor_string_P(PGM_P s);

#ifdef __cplusplus
}
#endif

#endif

