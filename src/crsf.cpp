#include <Arduino.h>
#include <AlfredoCRSF.h>
// crsf.cpp
#include "crsf.h"

namespace {
class BufferStream : public Stream {
 public:
  void push(const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
      if (count_ >= kBufferSize) {
        tail_ = (tail_ + 1) % kBufferSize;
        count_--;
      }
      buf_[head_] = data[i];
      head_ = (head_ + 1) % kBufferSize;
      count_++;
    }
  }

  int available() override { return count_; }

  int read() override {
    if (!count_) {
      return -1;
    }
    uint8_t b = buf_[tail_];
    tail_ = (tail_ + 1) % kBufferSize;
    count_--;
    return b;
  }

  int peek() override {
    if (!count_) {
      return -1;
    }
    return buf_[tail_];
  }

  void flush() override {}

  size_t write(uint8_t) override { return 1; }

 private:
  static const size_t kBufferSize = 256;
  uint8_t buf_[kBufferSize]{};
  size_t head_ = 0;
  size_t tail_ = 0;
  size_t count_ = 0;
};

static const uint8_t CRSF_FRAMETYPE_FLIGHT_MODE = 0x21;
static const float kRadToDeg = 57.2957795f;

BufferStream crsfStream;
uint8_t crsf_packet[CRSF_MAX_PACKET_LEN + 4]{};
uint8_t crsf_packet_len = 0;
char flight_mode_buf[32]{};
uint8_t flight_mode_lth = 0;
}  // namespace

AlfredoCRSF crsf;  // CRSF object instance

int16_t  espnow_len = 0;
int16_t  crsf_len = 0;
bool espnow_received = false;

uint8_t  hud_num_sats = 0;
float    hud_grd_spd = 0;
int16_t  hud_bat1_volts = 0;     // dV (V * 10)
int16_t  hud_bat1_amps = 0;      // dA )A * 10)
uint16_t hud_bat1_mAh = 0;

float crsf_att_pitch = 0.0f;
float crsf_att_roll = 0.0f;
float crsf_att_yaw = 0.0f;
float crsf_batt_volts = 0.0f;
float crsf_batt_amps = 0.0f;
uint16_t crsf_batt_mah = 0;
char crsf_flight_mode[32] = {0};

struct Location {
  float lat; // latitude
  float lon;
  float alt;
  float hdg;
  float alt_ag;
};

struct Location hom     = {
  0,0,0,0
};   // home location
struct Location cur      = {
  0,0,0,0
};   // current location

bool gpsGood = false;
bool gpsPrev = false;
bool hdgGood = false;
bool serGood = false;
bool lonGood = false;
bool latGood = false;
bool altGood = false;
bool boxhdgGood = false;
bool motArmed = false;   // motors armed flag
bool gpsfixGood = false;

bool finalHomeStored = false;

uint32_t  gpsGood_millis = 0;

void crsfBegin() {
  crsf.begin(crsfStream);
}

void crsfStoreEspnowPacket(const uint8_t *data, uint8_t len) {
  if (!data || len == 0) {
    return;
  }
  if (len > sizeof(crsf_packet)) {
    len = sizeof(crsf_packet);
  }
  memcpy(crsf_packet, data, len);
  crsf_packet_len = len;
}

void crsfReceive()
{
  crsf.update();

  if (espnow_received)  // flag
  {
    espnow_received = false;
    uint8_t len = crsf_packet_len;
    if (len < 4) {
      return;
    }

    crsfStream.push(crsf_packet, len);
    crsf.update();

    uint8_t crsf_id = crsf_packet[2];
    uint8_t payload_len = crsf_packet[1] >= 2 ? (crsf_packet[1] - 2) : 0;
    const uint8_t *payload = crsf_packet + 3;

    if (crsf_id == CRSF_FRAMETYPE_GPS)   // 0x02
    {
      const crsf_sensor_gps_t *gps = crsf.getGpsSensor();
      float heading = gps->heading / 1000.0f;
      // don't use gps heading, use attitude yaw below
      cur.lon = gps->longitude / 10000000.0f;
      cur.lat = gps->latitude / 10000000.0f;
      cur.alt = (float)gps->altitude - 1000.0f;
      gpsfixGood = (gps->satellites >=5);  // with 4 sats, altitude value can be bad
      lonGood = (gps->longitude != 0);
      latGood = (gps->latitude != 0);
      altGood = (gps->altitude != 0);
      hdgGood = true;
      if ((finalHomeStored))
      {
        cur.alt_ag = cur.alt - hom.alt;
      } else
      {
        cur.alt_ag = 0;
      }
      hud_num_sats = gps->satellites;         // these for the display
      hud_grd_spd = gps->groundspeed / 10.0f;
      (void)heading;
    }
    if (crsf_id == CRSF_FRAMETYPE_BATTERY_SENSOR)
    {
      if (payload_len >= 8) {
        uint16_t volts = (payload[0] << 8) | payload[1];
        uint16_t amps = (payload[2] << 8) | payload[3];
        uint32_t capacity = ((uint32_t)payload[4] << 16) | ((uint32_t)payload[5] << 8) | payload[6];
        uint8_t remaining = payload[7];
        hud_bat1_volts = volts;
        hud_bat1_amps = amps;
        hud_bat1_mAh = capacity;
        crsf_batt_volts = volts / 10.0f;
        crsf_batt_amps = amps / 10.0f;
        crsf_batt_mah = (uint16_t)(capacity > 0xFFFF ? 0xFFFF : capacity);
        Serial.print("BATTERY:");
        Serial.printf(" volts:%2.1f", volts / 10.0f);
        Serial.printf(" amps:%3.1f", amps / 10.0f);
        Serial.printf(" Ah_drawn:%3.1f", capacity / 1000.0f);
        Serial.printf(" remaining:%3u%%\n", remaining);
      }
    }

    if (crsf_id == CRSF_FRAMETYPE_ATTITUDE)
    {
      if (payload_len >= 6) {
        int16_t pitch_raw = (payload[0] << 8) | payload[1];
        int16_t roll_raw = (payload[2] << 8) | payload[3];
        int16_t yaw_raw = (payload[4] << 8) | payload[5];
        float pitch = (pitch_raw / 10000.0f) * kRadToDeg;
        float roll = (roll_raw / 10000.0f) * kRadToDeg;
        float yaw = (yaw_raw / 10000.0f) * kRadToDeg;
        cur.hdg = yaw;
        crsf_att_pitch = pitch;
        crsf_att_roll = roll;
        crsf_att_yaw = yaw;
        Serial.print("ATTITUDE:");
        Serial.print(" addr:0x");
        Serial.print(crsf_packet[0], HEX);
        Serial.printf(" pitch:%3.1fdeg", pitch);
        Serial.printf(" roll:%3.1fdeg", roll);
        Serial.printf(" yaw:%3.1fdeg\n", yaw);
      }
    }

    if (crsf_id == CRSF_FRAMETYPE_FLIGHT_MODE)
    {
      if (payload_len > 0) {
        size_t copy_len = payload_len;
        if (copy_len >= sizeof(flight_mode_buf)) {
          copy_len = sizeof(flight_mode_buf) - 1;
        }
        memcpy(flight_mode_buf, payload, copy_len);
        flight_mode_buf[copy_len] = '\0';
        flight_mode_lth = copy_len;
        strncpy(crsf_flight_mode, flight_mode_buf, sizeof(crsf_flight_mode) - 1);
        crsf_flight_mode[sizeof(crsf_flight_mode) - 1] = '\0';
      } else {
        flight_mode_buf[0] = '\0';
        flight_mode_lth = 0;
        crsf_flight_mode[0] = '\0';
      }
      motArmed = (strcmp(flight_mode_buf, "ARM") == 0);
      Serial.print("FLIGHT_MODE:");
      Serial.printf(" lth:%u %s motArmed:%u\n", flight_mode_lth, flight_mode_buf, motArmed);
      gpsGood = (gpsfixGood & lonGood & latGood & altGood);
      if (gpsGood) gpsGood_millis = millis();     // Time of last good GPS packet
    }
  }

  // No extra GPS status logging.
}
