#ifndef AVR_PGMSPACE_H
#define AVR_PGMSPACE_H

#include <stdint.h>
#include <string.h>

#ifndef PROGMEM
#define PROGMEM
#endif

typedef const void* PGM_VOID_P;
typedef const char* PGM_P;

#ifndef PSTR
#define PSTR(str_literal) (str_literal)
#endif

#define pgm_read_byte(addr)     (*(const uint8_t*)(addr))
#define pgm_read_word(addr)     (*(const uint16_t*)(addr))
#define pgm_read_dword(addr)    (*(const uint32_t*)(addr))
#define pgm_read_float(addr)    (*(const float*)(addr))
#define pgm_read_ptr(addr)      (*(const void* const*)(addr))

#define memcpy_P(dst, src, len)    memcpy((dst), (src), (len))
#define memcmp_P(buf1, buf2, len)  memcmp((buf1), (buf2), (len))
#define memchr_P(buf, ch, len)     memchr((buf), (ch), (len))
#define strcpy_P(dst, src)         strcpy((dst), (src))
#define strncpy_P(dst, src, len)   strncpy((dst), (src), (len))
#define strcat_P(dst, src)         strcat((dst), (src))
#define strncat_P(dst, src, len)   strncat((dst), (src), (len))
#define strlen_P(src)              strlen((src))
#define strcmp_P(s1, s2)           strcmp((s1), (s2))
#define strncmp_P(s1, s2, len)     strncmp((s1), (s2), (len))
#define strchr_P(s, c)             strchr((s), (c))
#define strrchr_P(s, c)            strrchr((s), (c))
#define strstr_P(s1, s2)           strstr((s1), (s2))

#endif
