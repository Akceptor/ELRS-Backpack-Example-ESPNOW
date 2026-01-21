// crsf.h
#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>

// Variable declarations
extern int16_t espnow_len;
extern int16_t crsf_len;
extern bool espnow_received;

// Function prototypes and variable declarations
void crsfBegin();
void crsfStoreEspnowPacket(const uint8_t *data, uint8_t len);
void crsfReceive();  // Example function

#endif
