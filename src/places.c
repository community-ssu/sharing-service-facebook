#include <hildon/hildon.h>
#include <exempi/xmp.h>
#include <json-glib/json-glib.h>
#include <libintl.h>
#include <sharing-tag.h>
#include <locale.h>
#include <math.h>
#include <string.h>
#include <gst/gst.h>

#include "places.h"


#define SCHEMA_IPTC "http://iptc.org/std/Iptc4xmpCore/1.0/xmlns/"
#define SCHEMA_EXIF "http://ns.adobe.com/exif/1.0/"
#define SCHEMA_TIFF "http://ns.adobe.com/tiff/1.0/"

struct _FacebookSelectPlace
{
  const gchar *file;
  GSList *places;
  gboolean thread_active;
  gint orientation;
  gint selected;
  gboolean is_video;
};
typedef struct _FacebookSelectPlace FacebookSelectPlace;

struct _FacebookSearchData
{
  gchar *location;
  gdouble latitude;
  gdouble longitude;
};

typedef struct _FacebookSearchData FacebookSearchData;
/*
 * Seems like Nokia use dd,mm.mm format to encode GPS coordinates, at least on
 * N900 and N950.
 */
static inline gdouble
convert2deg_dm(const gchar *d, const gchar *m, const gchar *mm)
{
    return
        (gdouble)atoi(d) +
        ((gdouble)atoi(m)) / 60.0 +
        ((gdouble)atoi(mm)) / (60.0 * pow(10, strlen(mm)));
}

static gdouble
deg2dec(const gchar *s, gboolean *ok)
{
  gchar **v = g_strsplit_set(s, ",.", 3);
  gdouble rv = 0;

  if(v[0] && v[1] && v[2])
  {
    gchar *ref = &v[2][strlen(v[2]) - 1];
    switch (*ref)
    {
      case 'E':
      case 'N':
        *ref = '\0';
        rv = convert2deg_dm(v[0], v[1], v[2]);
        *ok = TRUE;
        break;
      case 'S':
      case 'W':
        *ref = '\0';
        rv = 0.0 - convert2deg_dm(v[0], v[1], v[2]);
        *ok = TRUE;
        break;
    default:
      *ok = FALSE;
    }
  }
  else
    *ok = FALSE;

  g_strfreev(v);

  return rv;
}

static int
select_place_thread(FacebookSelectPlace *select)
{
  GtkWidget *selector, *dialog, *image = NULL, *picker, *align;
  GdkPixbuf *pixbuf, *newpixbuf;
  gint i;
  gchar *s;

  gdk_threads_enter();

  selector = hildon_touch_selector_new_text();
  hildon_touch_selector_set_column_selection_mode(
        HILDON_TOUCH_SELECTOR(selector),
        HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE);

  for (i = 0; i < g_slist_length(select->places); i ++)
  {
    hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector),
                                      g_slist_nth_data(select->places, i));
  }

  s = g_strdup_printf("%s Facebook %s",
                      dgettext("osso-connectivity-ui", "conn_bd_dialog_ok"),
                      dgettext("gtk20","Location"));
  dialog = gtk_dialog_new_with_buttons(s, NULL,
                                       GTK_DIALOG_NO_SEPARATOR |
                                       GTK_DIALOG_DESTROY_WITH_PARENT |
                                       GTK_DIALOG_MODAL,
                                       dgettext("hildon-libs", "wdgt_bd_done"),
                                       GTK_RESPONSE_OK,
                                       NULL);
  g_free(s);
  picker = hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                    HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(picker),
                                    HILDON_TOUCH_SELECTOR(selector));
  hildon_button_set_title(HILDON_BUTTON(picker),
                          dgettext("gtk20","Location"));
  hildon_picker_button_set_active(HILDON_PICKER_BUTTON(picker), 0);
  hildon_button_set_alignment(HILDON_BUTTON(picker), 0.0, 0.5, 1.0, 0.0);

  align = gtk_alignment_new(0, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 8, 8, 8, 8);

  if (!select->is_video)
  {
    pixbuf =
        gdk_pixbuf_new_from_file_at_scale(select->file, 128, 128, TRUE, NULL);

    if (pixbuf)
    {
      /*
       * FIXME  - we may need to flip as well, unfortunately I have no images
       * to test with, so better leave it that way.
       */
      newpixbuf =
          gdk_pixbuf_rotate_simple(pixbuf,
                                   360 - (select->orientation % 4 - 1) * 90);
      g_object_unref(pixbuf);
      pixbuf = newpixbuf;

    }
  }
  else
  {
    pixbuf = gtk_icon_theme_load_icon(gtk_icon_theme_get_default (),
                                      "video-x-generic", 128,
                                      GTK_ICON_LOOKUP_FORCE_SIZE, NULL);
  }

  if (pixbuf)
  {
    image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
  }

  if (image)
    gtk_container_add(GTK_CONTAINER(align), image);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), align, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), picker, TRUE, TRUE, 0);
  gtk_widget_show_all(dialog);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    select->selected =
        hildon_picker_button_get_active(HILDON_PICKER_BUTTON(picker));
  }

  gtk_widget_destroy (GTK_WIDGET(dialog));
  gdk_threads_leave();

  select->thread_active = FALSE;

  return FALSE;
}

static gint
select_place_id(const gchar *file, GSList *places, gint orientation,
                            gboolean is_video)
{
  FacebookSelectPlace select;
  DEBUG_LOG(__func__);

  select.thread_active = TRUE;
  select.file = file;
  select.selected = -1;
  select.places = places;
  select.orientation = orientation;
  select.is_video = is_video;

  g_thread_create_full((GThreadFunc)select_place_thread, &select, 0, FALSE,
                       FALSE, G_THREAD_PRIORITY_NORMAL, NULL);

  while (select.thread_active)
  {
    while (g_main_context_pending(NULL))
      g_main_context_iteration(NULL, TRUE);
  }

  return select.selected;
}

static gchar *
get_place_id(facebook_graph_request *request, const gchar *access_token,
             const gchar *path, ConIcConnection *con,
             const FacebookSearchData *data, gboolean is_video,
             gint orientation)
{
  gchar *rv = NULL;
  DEBUG_LOG(__func__);

  if (data->location && data->latitude != NAN && data->longitude != NAN)
  {
    GString *url;
    int http_result;
    GArray *response = g_array_new(1, 1, 1);

    facebook_graph_request_reset(request);

    g_hash_table_insert(request->query_params, "q",
                        g_strdup(data->location));
    g_hash_table_insert(request->query_params, "type",
                        g_strdup("place"));
    {
      locale_t loc = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
      locale_t old = uselocale(loc);

      g_hash_table_insert(request->query_params, "center",
                          g_strdup_printf("%f,%f",
                                          data->latitude,
                                          data->longitude));
      uselocale(old);
      freelocale(loc);
    }

    g_hash_table_insert(request->query_params, "distance",
                        g_strdup("50000"));
    g_hash_table_insert(request->query_params, "fields",
                        g_strdup("id,name"));
    g_hash_table_insert(request->query_params, "limit",
                        g_strdup("128")); /* why not? */
    g_hash_table_insert(request->query_params, "access_token",
                        g_strdup(access_token));

    url = g_string_new("https://graph.facebook.com/search");

    http_result = network_utils_get(url, response, NULL,
                                    request->query_params, con, NULL);
    g_string_free(url, FALSE);

    if (http_result == 200)
    {
      JsonParser *parser = json_parser_new();

      if (json_parser_load_from_data(parser, response->data,
                                     response->len, NULL))
      {
        JsonNode *root = json_parser_get_root(parser);
        JsonObject *object;

        if (root && JSON_NODE_HOLDS_OBJECT(root) &&
            (object = json_node_get_object(root)) &&
            json_object_has_member(object, "data"))
        {
          JsonArray *data = json_object_get_array_member(object, "data");
          guint len = json_array_get_length(data);
          gint i;
          GSList *l = NULL;

          for (i = 0; i < len; i ++)
          {
            object = json_array_get_object_element(data, i);
            if (object)
              l = g_slist_append(
                    l,
                    (gpointer)json_object_get_string_member(object,
                                                            "name"));
          }

          i = select_place_id(path, l, orientation, is_video);
          g_slist_free(l);

          if (i >= 0)
          {
            rv = (gchar *)json_object_get_string_member(
                  json_array_get_object_element(data, i), "id");
          }

          if (rv)
            rv = g_strdup(rv);
        }

        g_object_unref(parser);
      }
    }

    g_array_free(response, TRUE);
  }

  return rv;
}

static gchar *
get_photo_place_id(const SharingEntryMedia *entry,
                   facebook_graph_request *request, const gchar *access_token,
                   const gchar *path, ConIcConnection *con)
{
  XmpPtr xmp;
  XmpFilePtr fp;
  FacebookSearchData data = {NULL, NAN, NAN};
  guint orientation = 1;
  gchar *rv;
  gboolean ok;

  DEBUG_LOG(__func__);

  xmp_init();

  if (!(xmp = xmp_new_empty()))
    return NULL;

  if (!(fp = xmp_files_open_new(path, (XmpOpenFileOptions)(XMP_OPEN_READ |
                                                           XMP_OPEN_ONLYXMP))))
  {
    xmp_free(xmp);

    return NULL;
  }

  if (xmp_files_get_xmp(fp, xmp))
  {
    XmpStringPtr value = xmp_string_new();

    if (xmp_get_property(xmp, SCHEMA_IPTC, "Iptc4xmpCore:location", value,
                         NULL) ||
        xmp_get_property(xmp, SCHEMA_IPTC, "Iptc4xmpCore:Location", value,
                                 NULL))
      data.location = g_strdup(xmp_string_cstr(value));

    if (xmp_get_property(xmp, SCHEMA_EXIF, "exif:GPSLatitude", value, NULL))
    {
      data.latitude = deg2dec(g_strdup(xmp_string_cstr(value)), &ok);

      if (!ok)
        goto error;
    }

    if (xmp_get_property(xmp, SCHEMA_EXIF, "exif:GPSLongitude", value, NULL))
    {
      data.longitude = deg2dec(g_strdup(xmp_string_cstr(value)), &ok);

      if (!ok)
        goto error;
    }

    if (xmp_get_property(xmp, SCHEMA_TIFF, "tiff:Orientation", value, NULL))
      orientation = atoi(xmp_string_cstr(value));
error:
    xmp_string_free(value);
  }

  rv =
      get_place_id(request, access_token, path, con, &data, FALSE, orientation);

  g_free(data.location);

  xmp_free(xmp);
  xmp_terminate();

  return rv;
}

static void
sharing_tag_f(gpointer data, gpointer user_data)
{
  const SharingTag *tag = (const SharingTag *)data;

  switch (sharing_tag_get_type(tag))
  {
    case SHARING_TAG_GEO_SUBURB:
      ((FacebookSearchData *)user_data)->location =
        (gchar *)sharing_tag_get_word(tag);
      break;
    default:
      break;
  }
}

static void
read_one_tag(const GstTagList *list, const gchar *tag, gpointer user_data)
{
  FacebookSearchData *data = user_data;
  int n = gst_tag_list_get_tag_size(list, tag);

  if (!g_strcmp0(tag, "geo-location-latitude") && n == 1)
    data->latitude = g_value_get_double(
          gst_tag_list_get_value_index(list, tag, 0));

  if (!g_strcmp0(tag, "geo-location-longitude") && n == 1)
    data->longitude = g_value_get_double(
          gst_tag_list_get_value_index(list, tag, 1));
}

static void
on_new_pad(GstElement *dec, GstPad *pad, GstElement *fakesink)
{
  GstPad *sinkpad;

  sinkpad = gst_element_get_static_pad (fakesink, "sink");

  if (!gst_pad_is_linked (sinkpad))
    gst_pad_link(pad, sinkpad);

  gst_object_unref (sinkpad);
}

static void
gst_get_gps_coord(const gchar *path, FacebookSearchData *data)
{
  GstElement *pipe, *dec, *sink;
  GstMessage *msg;
  gchar *uri = g_strdup_printf("file://%s", path);

  gst_init(NULL, NULL);

  pipe = gst_pipeline_new("pipeline");

  dec = gst_element_factory_make("uridecodebin", NULL);
  g_object_set(dec, "uri", uri, NULL);
  g_free(uri);
  gst_bin_add(GST_BIN (pipe), dec);

  sink = gst_element_factory_make("fakesink", NULL);
  gst_bin_add(GST_BIN (pipe), sink);

  g_signal_connect(dec, "pad-added", G_CALLBACK (on_new_pad), sink);

  gst_element_set_state(pipe, GST_STATE_PAUSED);

  while (TRUE)
  {
    GstTagList *tags = NULL;

    msg = gst_bus_timed_pop_filtered(GST_ELEMENT_BUS (pipe),
                                     GST_CLOCK_TIME_NONE,
                                     GST_MESSAGE_ASYNC_DONE |
                                     GST_MESSAGE_TAG | GST_MESSAGE_ERROR);

    if (GST_MESSAGE_TYPE(msg) != GST_MESSAGE_TAG)
    {
      gst_message_unref(msg);
      break;
    }

    gst_message_parse_tag(msg, &tags);

    gst_tag_list_foreach(tags, read_one_tag, data);

    gst_tag_list_free(tags);
    gst_message_unref(msg);
  };

  gst_element_set_state(pipe, GST_STATE_NULL);
  gst_object_unref(pipe);
}

static gchar *
get_video_place_id(const SharingEntryMedia *entry,
                   facebook_graph_request *request, const gchar *access_token,
                   const gchar *path, ConIcConnection *con)
{
  FacebookSearchData data ={NULL, NAN, NAN};
  gchar *rv = NULL;
  GSList *tags = (GSList *)sharing_entry_media_get_tags(entry);

  /* Try to get suburb */
  g_slist_foreach(tags, sharing_tag_f, &data);

  if (!data.location)
    return NULL;

  gst_get_gps_coord(path, &data);

  if (!data.latitude == 0.0 || data.longitude == 0.0)
    return NULL;

  rv = get_place_id(request, access_token, path, con, &data, TRUE, 0);

  return rv;
}

gchar *
fb_sharing_plugin_get_place_id(const SharingEntryMedia *media,
                               facebook_graph_request *request,
                               const gchar *access_token, const gchar *path,
                               ConIcConnection *con, gboolean is_video)
{
  if (!is_video)
    return
        get_photo_place_id(media, request, access_token, path, con);
  else
    return
        get_video_place_id(media, request, access_token, path, con);
}
