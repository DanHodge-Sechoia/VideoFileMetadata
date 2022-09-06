#include <gst/gst.h>
#include <gst/codecparsers/gsth264parser.h>
#include <stdint-gcc.h>

#include "h264_sei_ntp.h"

// Taken from: https://blog.csdn.net/Cheers724/article/details/99822937

static void need_data_callback(GstElement *appsrc, guint unused,
                               gpointer udata) {
  g_print("need_data_callback\r\n");
  GstBuffer *buffer;
  GstFlowReturn ret;
  static uint64_t next_ms_time_insert_sei = 0;
  struct timespec one_ms;
  struct timespec rem;
  uint8_t *h264_sei = NULL;
  size_t length = 0;

  one_ms.tv_sec = 0;
  one_ms.tv_nsec = 10000000;

  while(now_ms() <= next_ms_time_insert_sei) {
    //g_print("sleep to wait time trigger\r\n");
    nanosleep(&one_ms, &rem);
  }

  if(!h264_sei_ntp_new(&h264_sei, &length)) {
    g_warning("h264_sei_ntp_new failed\r\n");
    return;
  }

  if(h264_sei != NULL && length > 0) {
    buffer = gst_buffer_new_allocate(NULL, START_CODE_PREFIX_BYTES + length, NULL);

    if(buffer != NULL) {
      uint8_t start_code_prefix[] = START_CODE_PREFIX;
      size_t bytes_copied = gst_buffer_fill(buffer, START_CODE_PREFIX_BYTES, h264_sei, length);

      if(bytes_copied == length) {
        g_signal_emit_by_name(appsrc, "push-buffer", buffer, &ret);
        g_print("H264 SEI NTP timestamp inserted\r\n");
      } else {
        g_warning("GstBuffer.fill without all bytes copied\r\n");
      }
    } else {
      g_warning("gst_buffer_new_allocate failed\r\n");
    }

    gst_buffer_unref(buffer);
  }

  next_ms_time_insert_sei = now_ms() + 1000;
  free(h264_sei);
}

static void handoff_callback(GstElement *identity, GstBuffer *buffer,
                             gpointer user_data) {
  //g_print("handoff_callback\r\n");
  GstMapInfo info = GST_MAP_INFO_INIT;
  GstH264NalParser *nalparser = NULL;
  GstH264NalUnit nalu;

  if(gst_buffer_map(buffer, &info, GST_MAP_READ)) {
    nalparser = gst_h264_nal_parser_new();
    if(nalparser != NULL) {
      if(gst_h264_parser_identify_nalu_unchecked(nalparser, info.data, 0, info.size, &nalu) == GST_H264_PARSER_OK) {
        if(nalu.type == GST_H264_NAL_SEI) {
          g_print("identify sei nalu with size: %d offset: %d sc_offset: %d\r\n", nalu.size, nalu.offset, nalu.sc_offset);
          int64_t delay = -1;

          if(h264_sei_ntp_parse(nalu.data + nalu.offset, nalu.size, &delay)) {
            g_print("delay: %ld ms\r\n", delay);
          }
        }
      } else {
        g_warning("gst_h264_parser_identify_nalu_unchecked failed");
      }

      gst_h264_nal_parser_free(nalparser);

    } else {
      g_warning("gst_h264_nal_parser_new failed\r\n");
    }

    gst_buffer_unmap(buffer, &info);
  } else {
    g_warning("gst_buffer_map failed\r\n");
  }
}
/* OUT
funnel name=f appsrc name=appsrc-h264-sei do-timestamp=true block=true is-live=true ! video/x-h264, stream-format=byte-stream, alignment=au ! queue ! f. \
videotestsrc is-live=true ! x264enc ! video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! queue ! f. \
f. ! queue ! h264parse ! video/x-h264, stream-format=byte-stream, alignment=au ! rtph264pay ! udpsink sync=false clients=127.0.0.1:5004
*/

/* IN
udpsrc uri=udp://127.0.0.1:5004 caps="application/x-rtp, media=video, encoding-name=H264" ! rtph264depay ! video/x-h264, stream-format=byte-stream, alignment=nal ! identity name=identity ! fakesink
*/



int main(int argc, char **argv) {
  GstElement *pipeline;
  GstBus *bus;
  GstMessage *msg;

  /* Initialize GStreamer */
  gst_init (&argc, &argv);

  /* Build the pipeline */
  pipeline =
    gst_parse_launch
      ("funnel name=f appsrc name=appsrc-h264-sei do-timestamp=true block=true is-live=true ! video/x-h264, stream-format=byte-stream, alignment=au ! queue ! f. videotestsrc is-live=true ! x264enc ! video/x-h264, stream-format=byte-stream, alignment=au, profile=baseline ! queue ! f. f. ! queue ! h264parse ! video/x-h264, stream-format=byte-stream, alignment=au ! rtph264pay ! queue ! rtph264depay ! video/x-h264, stream-format=byte-stream, alignment=nal ! identity name=identity ! h264parse ! nvv4l2decoder ! nvvidconv ! xvimagesink",
       NULL);

  GstElement *appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "appsrc-h264-sei");

  if(!appsrc) {
    g_error("failed to get appsrc");
    return -1;
  }

  g_signal_connect (appsrc, "need-data", G_CALLBACK (need_data_callback), NULL);

  GstElement *identity = gst_bin_get_by_name(GST_BIN(pipeline), "identity");

  if(!identity) {
    g_error("failed to get identity");
    return -1;
  }

  g_signal_connect (identity, "handoff", G_CALLBACK (handoff_callback), NULL);

  /* Start playing */
  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  /* Wait until error or EOS */
  bus = gst_element_get_bus (pipeline);
  msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

  /* See next tutorial for proper error message handling/parsing */
  if (GST_MESSAGE_TYPE (msg) == GST_MESSAGE_ERROR) {
    g_error ("An error occurred! Re-run with the GST_DEBUG=*:WARN environment "
             "variable set for more details.");
  }

  /* Free resources */
  gst_message_unref (msg);
  gst_object_unref (bus);
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  return 0;
}
