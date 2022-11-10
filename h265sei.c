#include <gst/gst.h>

static void
on_bus_message (GstBus * bus, GstMessage * message, GMainLoop * mainloop)
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

int main(int argc, char **argv)
{
  g_print ("h265sei\r\n");
  GstElement *pipeline = NULL;
  GstElement *video_source, *video_convert, *h265enc, *h265transform, *h265parse, *h265dec, *nvvidconv, *xvimagesink;
  GstBus *bus;
  GMainLoop *mainloop;

  gst_init (&argc, &argv);

  pipeline = gst_pipeline_new("video-file-metadata-pipeline");
  g_return_val_if_fail (pipeline != NULL, 1);

  video_source  = gst_element_factory_make("videotestsrc"     , "video_source");
  video_convert = gst_element_factory_make("autovideoconvert" , "video_convert");
  h265enc       = gst_element_factory_make("nvv4l2h265enc"    , "h265enc");
  h265transform = gst_element_factory_make("h265transform"    , "h265transform");
  h265parse     = gst_element_factory_make("h265parse"        , "h265parse");
  h265dec       = gst_element_factory_make("nvv4l2decoder"    , "h265dec");
  nvvidconv     = gst_element_factory_make("nvvidconv"        , "nvvidconv");
  xvimagesink   = gst_element_factory_make("xvimagesink"      , "xvimagesink");

  if(!video_source || !video_convert || !h265enc || !h265transform || !h265parse || !h265dec || !nvvidconv || !xvimagesink) {
    g_print ("could not create elements\n");
    return 1;
  }

  gst_bin_add_many(GST_BIN(pipeline), video_source, video_convert, h265enc, h265transform, h265parse, h265dec, nvvidconv, xvimagesink, NULL);

  if (gst_element_link_many(video_source, video_convert, h265enc, h265transform, h265parse, h265dec, nvvidconv, xvimagesink, NULL) != TRUE) {
    g_print ("could not link video elements\n");
    return 1;
  }

  g_object_set(video_source,
               "num-buffers", 50,
               NULL);

  mainloop = g_main_loop_new (NULL, FALSE);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) on_bus_message, mainloop);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  g_main_loop_run (mainloop);

  gst_element_set_state (pipeline, GST_STATE_NULL);

  gst_object_unref (pipeline);
  gst_object_unref (bus);

  return 0;
}