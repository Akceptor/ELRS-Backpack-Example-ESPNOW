// crsf.h
#ifndef CRSF_H
#define CRSF_H

#include <stdint.h>

// Variable declarations
extern int16_t espnow_len;
extern int16_t crsf_len;
extern bool espnow_received;

extern float crsf_att_pitch;
extern float crsf_att_roll;
extern float crsf_att_yaw;
extern float crsf_batt_volts;
extern float crsf_batt_amps;
extern uint16_t crsf_batt_mah;
extern char crsf_flight_mode[32];

// Function prototypes and variable declarations
void crsfBegin();
void crsfStoreEspnowPacket(const uint8_t *data, uint8_t len);
void crsfReceive();  // Example function

#endif
