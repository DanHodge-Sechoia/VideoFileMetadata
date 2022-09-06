#include "h264_sei_ntp.h"
#include "h264bitstream/h264_stream.h"  //https://github.com/D-Y-Innovations/h264bitstream
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <glib.h>

#define NALU_BUFFER_MAX_SIZE 50
#define SEI_PAYLOAD_SIZE 24
#define H264_SEI_NTP_UUID_SIZE 16

/*
 * start_code_prefix_one_3bytes: 0x000001
 * nal_unit_type: 0x06, Supplemetal enhancement information (SEI)
 * SEI payload_type_byte: 0x05
 * SEI payload_size_byte: 0x18
 * uuid_iso_iec_11578: 602b0db6-2d3d-44b5-ab9e-ec8ad71f3f8e   TODO DH do we need a new UUID? What is this used for?
 * user_data_payload_byte: 0x0000012345678912
 * rbsp_trailing_bits(): 0x80
 *
 * emulation_prevention_three_byte: 0x03
 */

uint64_t now_ms() {
  long ms;
  time_t s;
  struct timespec spec;

  clock_gettime(CLOCK_REALTIME, &spec);

  s = spec.tv_sec;
  ms = round(spec.tv_nsec / 1.0e6);

  if(ms > 999) {
    s++;
    ms = 0;
  }

  return s * 1000 + ms;
}

bool h264_sei_ntp_new(uint8_t **h264_sei, size_t *length) {
  uint8_t sei_uuid[] = H264_SEI_UUID_NTP_TIMESTAMP;
  uint8_t buffer[NALU_BUFFER_MAX_SIZE] = {0};
  uint8_t payloadData[SEI_PAYLOAD_SIZE] = {0};

  h264_stream_t *h264_nalu_sei = h264_new();
  h264_nalu_sei->nal->nal_ref_idc = NAL_REF_IDC_PRIORITY_DISPOSABLE;
  h264_nalu_sei->nal->nal_unit_type = NAL_UNIT_TYPE_SEI;

  memcpy(payloadData, sei_uuid, sizeof(sei_uuid));
  uint64_t  milli_time = now_ms();
  memcpy(&(payloadData[16]), &milli_time, sizeof(uint64_t));

  sei_t *seis[1] = {sei_new()};
  sei_t *sei = seis[0];
  sei->payloadType = SEI_TYPE_USER_DATA_UNREGISTERED;
  sei->payloadSize = SEI_PAYLOAD_SIZE;
  sei->data = payloadData;

  h264_nalu_sei->seis = seis;
  h264_nalu_sei->num_seis = 1;

  int len = write_nal_unit(h264_nalu_sei, buffer, NALU_BUFFER_MAX_SIZE);
  if(len <= 0) {
    g_print("len <= 0");
    return false;
  }

  uint8_t *h264_sei_tmp = malloc(len);

  if (h264_sei_tmp == NULL) {
    g_print("h264_sei_tmp == NULL");
    return false;
  }

  memcpy(h264_sei_tmp, buffer, len);

  *length = len;
  *h264_sei = h264_sei_tmp;
  return true;
}

bool h264_sei_ntp_parse(uint8_t *h264_sei, size_t length, int64_t *delay) {
  uint8_t sei_uuid[] = H264_SEI_UUID_NTP_TIMESTAMP;

  h264_stream_t *h264_nalu_sei = h264_new();
  if(read_nal_unit(h264_nalu_sei, h264_sei, length) > 0) {
    if(h264_nalu_sei->num_seis == 1) {
      sei_t *sei = h264_nalu_sei->seis[0];
      if(sei->payloadType == SEI_TYPE_USER_DATA_UNREGISTERED) {
        if(sei->payloadSize == SEI_PAYLOAD_SIZE) {
          if(memcmp(sei->data, sei_uuid, H264_SEI_NTP_UUID_SIZE) == 0) {
            uint64_t timestamp = 0;
            memcpy(&timestamp, sei->data + H264_SEI_NTP_UUID_SIZE, sizeof(timestamp));
            *delay = now_ms() - timestamp;
            if(*delay >= 0) {
              return true;
            } else {
              printf("delay < 0\n");
            }
          } else {
            printf("memcmp != 0\n");
          }
        } else {
          printf("payloadSize !== SEI_PAYLOAD_SIZE\n");
        }
      } else {
        printf("payloadType !== SEI_TYPE_USER_DATA_UNREGISTERED\n");
      }
    } else {
      printf("num_seis !== 1\n");
    }
  } else {
    printf("read_nal_unit <= 0\n");
  }
  return false;
}
