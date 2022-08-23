#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#define PIPELINE_STR "videotestsrc num-buffers=1000 ! x264enc ! queue ! mpegtsmux name=mux ! fakesink"

static void
_on_bus_message (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      g_print ("ERROR\n");
    case GST_MESSAGE_EOS:
      g_print ("ENDING LOOP\n");
      g_main_loop_quit (mainloop);
      break;
    default:
      break;
  }
}

static void
advertise_service (GstElement * mux)
{
  GstMpegtsSDTService *service;
  GstMpegtsSDT *sdt;
  GstMpegtsDescriptor *desc;
  GstMpegtsSection *section;

  sdt = gst_mpegts_sdt_new ();

  sdt->actual_ts = TRUE;
  sdt->transport_stream_id = 42;

  service = gst_mpegts_sdt_service_new ();
  service->service_id = 42;
  service->running_status =
    GST_MPEGTS_RUNNING_STATUS_RUNNING + service->service_id;
  service->EIT_schedule_flag = FALSE;
  service->EIT_present_following_flag = FALSE;
  service->free_CA_mode = FALSE;

  desc = gst_mpegts_descriptor_from_dvb_service
    (GST_DVB_SERVICE_DIGITAL_TELEVISION, "some-service", NULL);

  g_ptr_array_add (service->descriptors, desc);
  g_ptr_array_add (sdt->services, service);

  section = gst_mpegts_section_from_sdt (sdt);
  gst_mpegts_section_send_event (section, mux);
  gst_mpegts_section_unref (section);
}

int
main (int argc, char **argv)
{
  g_print ("main\n");
  GstElement *pipeline = NULL;
  GError *error = NULL;
  GstBus *bus;
  GMainLoop *mainloop;
  GstElement *mux;

  g_print ("gst_init\n");
  gst_init (&argc, &argv);

  g_print ("gst_parse_launch\n");
  pipeline = gst_parse_launch (PIPELINE_STR, &error);
  if (error) {
    g_print ("pipeline could not be constructed: %s\n", error->message);
    g_clear_error (&error);
    return 1;
  }

  g_print ("g_main_loop_new\n");
  mainloop = g_main_loop_new (NULL, FALSE);

  g_print ("gst_bin_get_by_name\n");
  mux = gst_bin_get_by_name (GST_BIN (pipeline), "mux");
  advertise_service (mux);
  gst_object_unref (mux);

  /* Put a bus handler */
  g_print ("gst_pipeline_get_bus\n");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) _on_bus_message, mainloop);

  /* Start pipeline */
  g_print ("gst_element_set_state GST_STATE_PLAYING\n");
  gst_element_set_state (pipeline, GST_STATE_PLAYING);
  g_print ("g_main_loop_run\n");
  g_main_loop_run (mainloop);

  g_print ("gst_element_set_state GST_STATE_NULL\n");
  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (bus);

  g_print ("done\n");
  return 0;
}