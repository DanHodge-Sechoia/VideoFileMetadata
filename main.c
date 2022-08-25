#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>

struct _OutputQueues
{
  GstElement *video;
  GstElement *audio;
};

typedef struct _OutputQueues OutputQueues;

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
  OutputQueues *output_queues = (OutputQueues *)data;
  GstElement *output_queue;

  /* We can now link this pad with the its corresponding sink pad */
  g_print("Dynamic pad created %s.\n", gst_pad_get_name(pad));

  if (strncmp(gst_pad_get_name(pad), "video", 5) == 0) {
    g_print("Linking video demuxer/decoder\n");
    output_queue = output_queues->video;
  }
  else if (strncmp(gst_pad_get_name(pad), "audio", 5) == 0) {
    g_print("Linking audio demuxer/decoder\n");
    output_queue = output_queues->audio;
  }
  else {
    g_print("Unhandled pad type: %s", gst_pad_get_name(pad));
    return;
  }

  sinkpad = gst_element_get_static_pad(output_queue, "sink");
  gst_pad_link(pad, sinkpad);
  gst_object_unref(sinkpad);
}

int
main (int argc, char **argv)
{
  // TODO DH check unrefs!
  g_print ("main\n");
  GstElement *pipeline = NULL;
  GstElement *video_source, *video_convert_1, *x264enc, *video_queue_1;
  GstElement *audio_source, *audio_convert_1, *voaacenc, *audio_queue_1;
  GstElement *mpegtsmux, *tsdemux;
  OutputQueues output_queues;
  GstElement *video_parser, *video_decoder, *video_convert_2, *video_sink;
  GstElement *audio_parser, *audio_decoder, *audio_convert_2, *wavescope, *audio_sink;
  GstPad *video_queue_pad, *video_mux_pad;
  GstPad *audio_queue_pad, *audio_mux_pad;
  GstBus *bus;
  GMainLoop *mainloop;
  GstElementFactory *mpegtsmux_factory;

  g_print ("gst_init\n");
  gst_init (&argc, &argv);

  g_print ("creating pipeline\n");

  mpegtsmux_factory = gst_element_factory_find ("mpegtsmux");
  g_return_val_if_fail (mpegtsmux_factory != NULL, 1);

  pipeline = gst_pipeline_new("video-file-metadata-pipeline");
  g_return_val_if_fail (pipeline != NULL, 1);

  g_print ("adding elements\n");

  video_source    = gst_element_factory_make("videotestsrc"    , "video_source");
  video_convert_1 = gst_element_factory_make("autovideoconvert", "video_convert_1");
  x264enc         = gst_element_factory_make("x264enc"         , "x264enc");
  video_queue_1   = gst_element_factory_make("queue"           , "video_queue_1");

  audio_source    = gst_element_factory_make("audiotestsrc", "audio_source");
  audio_convert_1 = gst_element_factory_make("audioconvert", "audio_convert_1");
  voaacenc        = gst_element_factory_make("voaacenc"    , "voaacenc");
  audio_queue_1   = gst_element_factory_make("queue"       , "audio_queue_1");

  mpegtsmux = gst_element_factory_make("mpegtsmux", "mpegtsmux");
  tsdemux   = gst_element_factory_make("tsdemux"  , "tsdemux");

  output_queues.video = gst_element_factory_make("queue"        , "video_queue_2");
  video_parser        = gst_element_factory_make("h264parse"    , "h264parse");
  video_decoder       = gst_element_factory_make("nvv4l2decoder", "video_decoder");
  video_convert_2     = gst_element_factory_make("videoconvert" , "video_convert_2");
  video_sink          = gst_element_factory_make("nvoverlaysink", "video_sink");

  output_queues.audio = gst_element_factory_make("queue"        , "audio_queue_2");
  audio_parser        = gst_element_factory_make("aacparse"     , "aacparse");
  audio_decoder       = gst_element_factory_make("avdec_aac"    , "audio_decoder");
  audio_convert_2     = gst_element_factory_make("audioconvert" , "audio_convert_2");
  wavescope           = gst_element_factory_make("wavescope"    , "wavescope");
  audio_sink          = gst_element_factory_make("ximagesink"   , "audio_sink");

  if(!video_source        || !video_convert_1 || !x264enc         || !video_queue_1   ||
     !audio_source        || !audio_convert_1 || !voaacenc        || !audio_queue_1   ||
     !mpegtsmux           || !tsdemux         ||
     !output_queues.video || !video_parser    || !video_decoder   || !video_convert_2 || !video_sink  ||
     !output_queues.audio || !audio_parser    || !audio_decoder   || !audio_convert_2 || !wavescope   || !audio_sink) {
    g_print ("could not create elements\n");
    return 1;
  }

  gst_bin_add_many(GST_BIN(pipeline),
                   video_source       , video_convert_1, x264enc      , video_queue_1  ,
                   audio_source       , audio_convert_1, voaacenc     , audio_queue_1  ,
                   mpegtsmux          , tsdemux        ,
                   output_queues.video, video_parser   , video_decoder, video_convert_2, video_sink,
                   output_queues.audio, audio_parser   , audio_decoder, audio_convert_2, wavescope , audio_sink, NULL);

  if (gst_element_link_many(video_source, video_convert_1, x264enc, video_queue_1, NULL) != TRUE) {
   g_print ("could not link video pre-demux elements\n");
   return 1;
  }

  if (gst_element_link_many(audio_source, audio_convert_1, voaacenc, audio_queue_1, NULL) != TRUE) {
    g_print ("could not link audio pre-demux elements\n");
    return 1;
  }

  video_queue_pad = gst_element_get_static_pad(video_queue_1, "src");
  video_mux_pad   = gst_element_get_request_pad(mpegtsmux, "sink_%d");
  g_print("Obtained request pad %s for video mux.\n", gst_pad_get_name(video_mux_pad));

  if (gst_pad_link(video_queue_pad, video_mux_pad) != GST_PAD_LINK_OK) {
    g_print ("video mux could not be linked\n");
    return 1;
  }

  audio_queue_pad = gst_element_get_static_pad(audio_queue_1, "src");
  audio_mux_pad   = gst_element_get_request_pad(mpegtsmux, "sink_%d");
  g_print("Obtained request pad %s for audio mux.\n", gst_pad_get_name(audio_mux_pad));

  if (gst_pad_link(audio_queue_pad, audio_mux_pad) != GST_PAD_LINK_OK) {
    g_print ("audio mux could not be linked\n");
    return 1;
  }

  if (gst_element_link(mpegtsmux, tsdemux) != TRUE) {
    g_print ("could not link mux to demux elements\n");
    return 1;
  }

  if (gst_element_link_many(output_queues.video, video_parser, video_decoder, video_convert_2, video_sink, NULL) != TRUE) {
   g_print ("could not link video output elements\n");
   return 1;
  }

  if (gst_element_link_many(output_queues.audio, audio_parser, audio_decoder, audio_convert_2, wavescope, audio_sink, NULL) != TRUE) {
    g_print ("could not link audio output elements\n");
    return 1;
  }

  g_signal_connect(tsdemux, "pad-added", G_CALLBACK(on_pad_added), &output_queues);

  g_object_set(video_source,
              "num-buffers", 50,
              NULL);

  g_object_set(audio_source,
               "num-buffers", 50,
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