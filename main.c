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

int
main (int argc, char **argv)
{
  g_print ("main\n");
  GstElement *pipeline = NULL;
  GstElement *source, *convert, *sink;
  GError *error = NULL;
  GstBus *bus;
  GMainLoop *mainloop;

  source = convert = sink = NULL;

  g_print ("gst_init\n");
  gst_init (&argc, &argv);

  g_print ("creating pipeline\n");

  pipeline = gst_pipeline_new("video-file-metadata-pipeline");

  if (!pipeline) {
    g_print ("pipeline could not be constructed\n");
    return 1;
  }

  g_print ("adding elements\n");

  source  = gst_element_factory_make("videotestsrc", "source");
  convert = gst_element_factory_make("autovideoconvert", "convert");
  sink    = gst_element_factory_make("xvimagesink", "sink");

  if(!source || !convert || !sink) {
    g_print ("could not create elements\n");
    return 1;
  }

  gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL);
  if (gst_element_link_many(source, convert, sink, NULL) != TRUE) {
    g_print ("could not link elements\n");
    return 1;
  }

  g_object_set(source,
               "num-buffers", 100,
               NULL);

  g_print ("g_main_loop_new\n");
  mainloop = g_main_loop_new (NULL, FALSE);

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