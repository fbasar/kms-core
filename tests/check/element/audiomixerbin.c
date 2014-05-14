/*
 * (C) Copyright 2014 Kurento (http://kurento.org/)
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Lesser General Public License
 * (LGPL) version 2.1 which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/lgpl-2.1.html
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/check/gstcheck.h>
#include <gst/gst.h>
#include <glib.h>

#ifdef MANUAL_CHECK
#define FILE_PREFIX "audiomixerbin_file_"
static guint id = 0;
#endif

static GstElement *pipeline, *audiomixer;
static GMainLoop *loop;
static GHashTable *hash;

static gboolean
quit_main_loop ()
{
  g_main_loop_quit (loop);

  return G_SOURCE_REMOVE;
}

static void
bus_msg (GstBus * bus, GstMessage * msg, gpointer data)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      GError *err = NULL;
      gchar *dbg_info = NULL;
      gchar *err_str;

      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
          GST_DEBUG_GRAPH_SHOW_ALL, "bus_error");
      gst_message_parse_error (msg, &err, &dbg_info);

      err_str = g_strdup_printf ("Error received on bus: %s: %s", err->message,
          dbg_info);
      g_error_free (err);
      g_free (dbg_info);

      fail (err_str);
      g_free (err_str);

      break;
    }
    case GST_MESSAGE_STATE_CHANGED:{
      GST_TRACE ("Event: %" GST_PTR_FORMAT, msg);
      break;
    }
    case GST_MESSAGE_EOS:{
      GST_DEBUG ("Receive EOS");
      quit_main_loop ();
    }
    default:
      break;
  }
}

static GstElement *
create_sink_element ()
{
  GstElement *sink;

#ifdef MANUAL_CHECK
  {
    gchar *filename;

    filename = g_strdup_printf (FILE_PREFIX "%u.wv", id);

    GST_DEBUG ("Setting location to %s", filename);
    sink = gst_element_factory_make ("filesink", NULL);
    g_object_set (G_OBJECT (sink), "location", filename, NULL);
    g_free (filename);
  }
#else
  {
    sink = gst_element_factory_make ("fakesink", NULL);
  }
#endif

  return sink;
}

GST_START_TEST (check_audio_connection)
{
  GstElement *audiotestsrc1, *audiotestsrc2, *wavenc, *sink;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
#ifdef MANUAL_CHECK
  id = 0;
#endif

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("audimixerbin0-test");
  audiotestsrc1 = gst_element_factory_make ("audiotestsrc", NULL);
  audiotestsrc2 = gst_element_factory_make ("audiotestsrc", NULL);
  audiomixer = gst_element_factory_make ("audiomixerbin", NULL);
  wavenc = gst_element_factory_make ("wavenc", NULL);
  sink = create_sink_element ();

  g_object_set (G_OBJECT (audiotestsrc1), "wave", 0, "num-buffers", 100, NULL);
  g_object_set (G_OBJECT (audiotestsrc2), "wave", 11, "num-buffers", 100, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc1, audiotestsrc2,
      audiomixer, wavenc, sink, NULL);
  gst_element_link (audiotestsrc1, audiomixer);
  gst_element_link (audiotestsrc2, audiomixer);
  gst_element_link_many (audiomixer, wavenc, sink, NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "entering_main_loop");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_main_loop");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");

  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST static gboolean
connect_audio_source (gpointer data)
{
  GstElement *audiotestsrc2;

  GST_DEBUG ("Adding audio source 2");
  audiotestsrc2 = gst_element_factory_make ("audiotestsrc", NULL);
  g_object_set (G_OBJECT (audiotestsrc2), "wave", 11, "num-buffers", 100, NULL);

  gst_bin_add (GST_BIN (pipeline), audiotestsrc2);
  gst_element_link (audiotestsrc2, audiomixer);
  gst_element_sync_state_with_parent (audiotestsrc2);

  return G_SOURCE_REMOVE;
}

GST_START_TEST (check_delayed_audio_connection)
{
  GstElement *audiotestsrc1, *wavenc, *sink;
  guint bus_watch_id;
  GstBus *bus;

  loop = g_main_loop_new (NULL, FALSE);
#ifdef MANUAL_CHECK
  id = 1;
#endif

  /* Create gstreamer elements */
  pipeline = gst_pipeline_new ("audimixerbin1-test");
  audiotestsrc1 = gst_element_factory_make ("audiotestsrc", NULL);
  audiomixer = gst_element_factory_make ("audiomixerbin", NULL);
  wavenc = gst_element_factory_make ("wavenc", NULL);
  sink = create_sink_element ();

  g_object_set (G_OBJECT (audiotestsrc1), "wave", 0, "num-buffers", 100, NULL);

  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  bus_watch_id = gst_bus_add_watch (bus, gst_bus_async_signal_func, NULL);
  g_signal_connect (bus, "message", G_CALLBACK (bus_msg), pipeline);
  g_object_unref (bus);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc1, audiomixer, wavenc, sink,
      NULL);
  gst_element_link_many (audiotestsrc1, audiomixer, wavenc, sink, NULL);

  g_timeout_add (1000, connect_audio_source, NULL);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "entering_main_loop");

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  GST_DEBUG ("Test running");

  g_main_loop_run (loop);

  GST_DEBUG ("Stop executed");

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, "after_main_loop");

  GST_DEBUG ("Setting pipline to NULL state");
  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_DEBUG ("Releasing pipeline");
  gst_object_unref (GST_OBJECT (pipeline));
  GST_DEBUG ("Pipeline released");

  g_source_remove (bus_watch_id);
  g_main_loop_unref (loop);
}

GST_END_TEST
/******************************/
/* audiomixer test suit */
/******************************/
static Suite *
audiomixerbin_suite (void)
{
  Suite *s = suite_create ("audiomixerbin");
  TCase *tc_chain = tcase_create ("element");

  suite_add_tcase (s, tc_chain);
  tcase_add_test (tc_chain, check_audio_connection);
  tcase_add_test (tc_chain, check_delayed_audio_connection);

  return s;
}

GST_CHECK_MAIN (audiomixerbin);
