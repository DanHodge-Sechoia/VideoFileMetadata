#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

#define PIPELINE_STR "videotestsrc num-buffers=1000 ! x264enc ! queue ! mpegtsmux name=mux ! fakesink"

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

static void on_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
  GstPad *sinkpad;
  GstElement *decoder = (GstElement *) data;

  /* We can now link this pad with the its corresponding sink pad */
  g_print("Dynamic pad created, linking demuxer/decoder\n");
  sinkpad = gst_element_get_static_pad(decoder, "sink");
  gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
}

int
main (int argc, char **argv)
{
  // TODO DH check unrefs!
  g_print ("main\n");
  GstElement *pipeline = NULL;
  GstElement *video_source, *video_convert_1, *x264enc, *video_queue;
  GstElement *mpegtsmux, *tsdemux;
  GstElement *h264parse, *video_decoder, *video_convert_2, *video_sink;
  GstPad *video_queue_pad, *video_mux_pad;
  GstBus *bus;
  GMainLoop *mainloop;
  GstElementFactory *mpegtsmux_factory;

  video_source = video_convert_1 = x264enc = video_queue = mpegtsmux = tsdemux = NULL;
  h264parse = video_decoder = video_convert_2 = video_sink = NULL;

  g_print ("gst_init\n");
  gst_init (&argc, &argv);

  g_print ("creating pipeline\n");

  mpegtsmux_factory = gst_element_factory_find ("mpegtsmux");
  g_return_val_if_fail (mpegtsmux_factory != NULL, 1);

  pipeline = gst_pipeline_new("video-file-metadata-pipeline");
  g_return_val_if_fail (pipeline != NULL, 1);

  g_print ("adding elements\n");

  video_source    = gst_element_factory_make("videotestsrc", "video_source");
  video_convert_1 = gst_element_factory_make("autovideoconvert", "video_convert_1");
  x264enc         = gst_element_factory_make("x264enc", "x264enc");
  video_queue     = gst_element_factory_make("queue", "video_queue");

  mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");
  tsdemux   = gst_element_factory_make("tsdemux", "tsdemux");

  h264parse       = gst_element_factory_make("h264parse", "h264parse");
  video_decoder   = gst_element_factory_make("nvv4l2decoder", "video_decoder");
  video_convert_2 = gst_element_factory_make("videoconvert", "video_convert_2");
  video_sink      = gst_element_factory_make("nvoverlaysink", "video_sink");

  if(!video_source || !video_convert_1 || !x264enc || !video_queue || !mpegtsmux || !tsdemux || !video_sink) {
    g_print ("could not create elements\n");
    return 1;
  }

  gst_bin_add_many(GST_BIN(pipeline), video_source, video_convert_1, x264enc, video_queue, mpegtsmux, tsdemux,
                   h264parse, video_decoder, video_convert_2, video_sink, NULL);

  if (gst_element_link_many(video_source, video_convert_1, x264enc, video_queue, NULL) != TRUE) {
    g_print ("could not link video pre-demux elements\n");
    return 1;
  }

  video_queue_pad = gst_element_get_static_pad(video_queue, "src");
  video_mux_pad   = gst_element_get_request_pad(mpegtsmux, "sink_%d");
  g_print("Obtained request pad %s for video mux.\n", gst_pad_get_name(video_mux_pad));

  if (gst_pad_link(video_queue_pad, video_mux_pad) != GST_PAD_LINK_OK) {
    g_print ("video mux could not be linked\n");
    return 1;
  }

  if (gst_element_link(mpegtsmux, tsdemux) != TRUE) {
    g_print ("could not link mux to demux elements\n");
    return 1;
  }

  if (gst_element_link_many(h264parse, video_decoder, video_convert_2, video_sink, NULL) != TRUE) {
    g_print ("could not link pre-demux elements\n");
    return 1;
  }

  g_signal_connect(tsdemux, "pad-added", G_CALLBACK(on_pad_added), h264parse);

  g_object_set(video_source,
               "num-buffers", 100,
               NULL);

  g_print ("g_main_loop_new\n");
  mainloop = g_main_loop_new (NULL, FALSE);

  /* Put a bus handler */
  g_print ("gst_pipeline_get_bus\n");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", (GCallback) on_bus_message, mainloop);

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