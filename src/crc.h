/*
**  CRC.H - header file for SNIPPETS CRC and checksum functions
*/

#ifndef CRC__H
#define CRC__H

#include <stdlib.h>           /* For size_t                      */
#include <stdint.h>           /* For uint8_t, uint16_t, uint32_t */
#include <stdbool.h>          /* For bool, true, false           */

#ifdef __cplusplus
extern "C" {
#endif
  void init_crc_table(void);
  uint16_t crc_calc(uint16_t crc, char *buf, unsigned nbytes);
  void do_file(char *fn);
  uint16_t crc16(char *data_p, uint16_t length);
  uint16_t updcrc(uint16_t icrc, uint8_t *icp, size_t icnt);
  uint32_t updateCRC32(unsigned char ch, uint32_t crc);
  bool crc32file(char *name, uint32_t *crc, long *charcnt);
  uint32_t crc32buf( const char *buf, size_t len);
  unsigned checksum(void *buffer, size_t len, unsigned int seed);
  void checkexe(char *fname);

#ifdef __cplusplus
}
#endif

#endif /* CRC__H */

