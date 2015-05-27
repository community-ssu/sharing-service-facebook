#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <libosso.h>
#include <libintl.h>
#include <pthread.h>
#include <sharing-account.h>
#include <sharing-service-option.h>

#include <tablet-browser-interface.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "places.h"

/* Those should go in some -dev package someday */
extern gboolean
sharing_system_has_account_with_username(
    SharingService *self, const gchar *username);
extern gint
sharing_service_get_password_max_length(SharingService *self);
extern int sharing_connection_monitor_check();
extern SharingService *sharing_account_get_service(SharingAccount *self);
extern gboolean sharing_entry_sync_filesystem(SharingEntry *self);
extern gint sharing_account_ref(SharingAccount *self);
extern gint sharing_account_unref(SharingAccount *self);

struct _FacebookUpdateOptions
{
  SharingAccount *account;
  ConIcConnection *con;
  gboolean *dead_mans_switch;
  gboolean *continue_update;
  void (*cb_func)(SharingPluginInterfaceUpdateOptionsResult, gpointer);
  gpointer cb_data;
  SharingPluginInterfaceUpdateOptionsResult result;
  gboolean thread_active;
};
typedef struct _FacebookUpdateOptions FacebookUpdateOptions;

struct _PluginDataTransfer
{
  guint64 upload_sum_size;
  guint64 upload_done;
  SharingTransfer *transfer;
  SharingEntryMedia *media;
  gboolean *dead_mans_switch;
};
typedef struct _PluginDataTransfer PluginDataTransfer;

static void
fb_sharing_plugin_launch_browser(GtkWidget *widget,
                                 osso_context_t *osso_context)
{
  if (sharing_connection_monitor_check() == 3)
  {
    osso_rpc_run_with_defaults(osso_context, "osso_browser",
                                   OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, NULL,
                                   DBUS_TYPE_STRING, "www.facebook.com/r.php",
                                   DBUS_TYPE_BOOLEAN, FALSE,
                                   DBUS_TYPE_INVALID);
  }
  else
    hildon_banner_show_information(GTK_WIDGET(widget), NULL,
                                   dgettext("osso-sharing-ui",
                                            "share_ib_no_connection"));
}

SharingPluginInterfaceAccountSetupResult
fb_sharing_plugin_account_setup(GtkWindow *parent,
                                SharingService *service,
                                SharingAccount **account_worked_on,
                                osso_context_t *osso_context)
{
  gchar *s;
  GtkWidget *dialog;
  GtkWidget *password_label;
  GtkWidget *table;
  GtkWidget *button;
  GtkWidget *email_entry;
  GtkWidget *email_label;
  GtkWidget *password_entry;
  gboolean email_needs_entry = FALSE;

  DEBUG_LOG(__func__);

  s = dgettext("osso-sharing-ui", "share_ti_accounts_setup");
  s = g_strdup_printf(s, "Facebook");

  dialog =
      gtk_dialog_new_with_buttons(s, GTK_WINDOW(parent),
                                  GTK_DIALOG_NO_SEPARATOR |
                                  GTK_DIALOG_DESTROY_WITH_PARENT |
                                  GTK_DIALOG_MODAL,
                                  dgettext("hildon-libs", "wdgt_bd_save"),
                                  GTK_RESPONSE_OK, 0);
  g_free(s);

  button = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT,
                             HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_button_set_text(HILDON_BUTTON(button),
                         dgettext("osso-sharing-ui",
                                  "share_bd_accounts_register_new"),
                         NULL);
  g_signal_connect_data(button, "clicked",
                        G_CALLBACK(fb_sharing_plugin_launch_browser),
                        osso_context, 0, 0);

  email_entry = hildon_entry_new(HILDON_SIZE_AUTO_WIDTH);
  hildon_gtk_widget_set_theme_size(email_entry, HILDON_SIZE_FINGER_HEIGHT);
  hildon_gtk_entry_set_input_mode(GTK_ENTRY(email_entry),
                                  HILDON_GTK_INPUT_MODE_FULL);

  email_label = gtk_label_new("Email");
  gtk_misc_set_alignment(GTK_MISC(email_label), 0.0, 0.5);

  password_entry = hildon_entry_new(0);
  hildon_gtk_widget_set_theme_size(password_entry, HILDON_SIZE_FINGER_HEIGHT);
  hildon_gtk_entry_set_input_mode(GTK_ENTRY(password_entry),
                                  HILDON_GTK_INPUT_MODE_FULL |
                                  HILDON_GTK_INPUT_MODE_INVISIBLE);

  password_label =
      gtk_label_new(dgettext("osso-sharing-ui",
                             "share_fi_cpa_edit_account_password"));
  gtk_misc_set_alignment(GTK_MISC(password_label), 0.0, 0.5);

  if (*account_worked_on)
  {
    gchar *username =
        sharing_account_get_param(*account_worked_on, "username");

    if (!username || !(*username) || !(username = facebook_get_email()))
      email_needs_entry = TRUE;

    hildon_entry_set_text(HILDON_ENTRY(email_entry), username);
    hildon_entry_set_text(HILDON_ENTRY(password_entry),
                          sharing_account_get_param(*account_worked_on,
                                                    "password"));
  }
  else
    email_needs_entry = TRUE;

  table = gtk_table_new(3, 2, FALSE);
  gtk_table_set_row_spacings(GTK_TABLE(table), 8);

  gtk_table_attach(GTK_TABLE(table), button, 0, 2, 0, 1, GTK_FILL | GTK_EXPAND,
                   GTK_FILL | GTK_EXPAND, 8, 0);
  gtk_table_attach(GTK_TABLE(table), email_label, 0, 1, 1, 2, GTK_FILL,
                   GTK_FILL, 8, 0);
  gtk_table_attach(GTK_TABLE(table), email_entry, 1, 2, 1, 2,
                   GTK_FILL | GTK_EXPAND, GTK_FILL|GTK_EXPAND, 16, 0);
  gtk_table_attach(GTK_TABLE(table), password_label, 0, 1, 2, 3,
                   GTK_FILL, GTK_FILL, 8, 0);
  gtk_table_attach(GTK_TABLE(table), password_entry, 1, 2, 2, 3,
                   GTK_FILL | GTK_EXPAND, GTK_FILL | GTK_EXPAND, 16, 0);

  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, TRUE, TRUE, 0);

  g_object_set_data(G_OBJECT(dialog), "email_entry", email_entry);
  g_object_set_data(G_OBJECT(dialog), "password_entry", password_entry);
  g_object_set_data(G_OBJECT(dialog), "service", service);
  gtk_widget_show_all(dialog);

  if (email_needs_entry)
    gtk_widget_grab_focus(email_entry);
  else
    gtk_widget_grab_focus(password_entry);

  for ( ; ; )
  {
    const gchar *username;
    const gchar *password;
    GtkWidget *note;

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_OK)
    {
      gtk_widget_destroy(dialog);

      return SHARING_ACCOUNT_SETUP_ERROR_UNKNOWN;
    }

    username = hildon_entry_get_text(
          HILDON_ENTRY(g_object_get_data(G_OBJECT(dialog), "email_entry")));

    password = hildon_entry_get_text(
          HILDON_ENTRY(g_object_get_data(G_OBJECT(dialog),"password_entry")));

    if (!username || !*username || !password || !*password)
    {
      hildon_banner_show_information(
            dialog, NULL, dgettext("osso-sharing-ui",
                                   "share_ib_enter_credentials_first"));
      continue;
    }

    service = g_object_get_data(G_OBJECT(dialog),"service");

    if (sharing_system_has_account_with_username(service, username))
    {
      gchar *sevice_name = sharing_service_get_name(service);

      s = dgettext("osso-sharing-ui", "share_ni_username_existing");
      s = g_strdup_printf(s, username, sevice_name);

      note = hildon_note_new_information(GTK_WINDOW(dialog), s);
      gtk_dialog_run(GTK_DIALOG(dialog));

      gtk_widget_destroy(note);
      g_free(sevice_name);
      g_free(s);

      continue;
    }

    if (sharing_connection_monitor_check() == 3)
      break;

    hildon_banner_show_information(
          dialog, NULL, dgettext("osso-sharing-ui", "share_ib_no_connection"));
  }

  sharing_account_set_param(*account_worked_on, "username",
                            hildon_entry_get_text(HILDON_ENTRY(email_entry)));
  sharing_account_set_param(*account_worked_on, "password",
                            hildon_entry_get_text(HILDON_ENTRY(password_entry)));
  gtk_widget_destroy(dialog);

  return SHARING_ACCOUNT_SETUP_SUCCESS;
}

SharingPluginInterfaceAccountSetupResult
sharing_plugin_interface_account_setup(GtkWindow *parent,
                                       SharingService *service,
                                       SharingAccount **account_worked_on,
                                       osso_context_t *osso)
{
  DEBUG_LOG(__func__);
  return
      fb_sharing_plugin_account_setup(parent, service, account_worked_on, osso);
}

static void
fb_sharing_plugin_reset_dead_mans_switch(gboolean *dead_mans_switch)
{
  if (*dead_mans_switch)
    *dead_mans_switch = FALSE;
}

static gboolean fb_sharing_plugin_login_progress(gdouble bytes_sent,
                                                 gboolean *user_data)
{
  gboolean rv = FALSE;

  DEBUG_LOG(__func__);

  if (user_data)
    rv = (*user_data == FALSE);

  return rv;
}

SharingPluginInterfaceAccountValidateResult
sharing_plugin_interface_account_validate(SharingAccount *account,
                                          ConIcConnection *con, gboolean *cont,
                                          gboolean *dead_mans_switch)
{
  gchar *username;
  gchar *password;
  SharingPluginInterfaceAccountValidateResult
      rv = SHARING_ACCOUNT_VALIDATE_SUCCESS;
  facebook_graph_credentials *credentials;
  facebook_graph_request *request;
  HttpProgress progress;
  GError *error = NULL;

  DEBUG_LOG(__func__);

  fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);

  username = sharing_account_get_param(account, "username");
  password = sharing_account_get_param(account, "password");

  if (password && username)
  {
    progress.user_data = cont;
    progress.callback = (HttpProgressCallback)fb_sharing_plugin_login_progress;

    request = facebook_graph_request_new();
    request->email = g_strdup(username);
    request->password = g_strdup(password);
    request->scope =
        g_strdup("user_photos,publish_actions");

    credentials = facebook_graph_login(request, con, &progress, &error);

    fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);

    if (credentials)
    {
      if (!facebook_get_email())
        facebook_store_graph_credentials_to_gconf(credentials);

      sharing_account_set_param(account, "access_token",
                                credentials->access_token);
    }
    else
    {
      sharing_account_set_param(account, "access_token", " ");

      if (error)
      {
        switch (error->code)
        {
          case -1022:
            rv = SHARING_ACCOUNT_VALIDATE_ERROR_CONNECTION;
            break;
          case -1021:
            rv = SHARING_ACCOUNT_VALIDATE_CANCELLED;
            break;
          case -1023:
            rv = SHARING_ACCOUNT_VALIDATE_FAILED;
            break;
          default:
            rv = SHARING_ACCOUNT_VALIDATE_ERROR_UNKNOWN;
            break;
        }
      }
      else
        rv = SHARING_ACCOUNT_VALIDATE_ACCOUNT_NOT_FOUND;
    }

    facebook_graph_credentials_free(credentials);
    facebook_graph_request_free(request);

    if (error)
      g_error_free(error);
  }
  else
    rv = SHARING_ACCOUNT_VALIDATE_ERROR_UNKNOWN;

  fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);
  g_free(username);
  g_free(password);

  return rv;
}

static gboolean
fb_sharing_plugin_editor_validate_changes(GtkWidget *dialog,
                                          SharingAccount *account)
{
  GObject *entry;

  DEBUG_LOG(__func__);

  g_return_val_if_fail(dialog != NULL, FALSE);
  g_return_val_if_fail(account != NULL, FALSE);

  entry = g_object_get_data(G_OBJECT(dialog), "password_entry");

  if (entry)
  {
    const gchar *password = hildon_entry_get_text(HILDON_ENTRY(entry));

    if (password && *password)
    {
      sharing_account_set_param(account, "password", password);

      return TRUE;
    }

    hildon_banner_show_information(
          dialog, NULL,
          dgettext("osso-sharing-ui", "share_ib_enter_credentials_first"));
  }

  return FALSE;
}

static gboolean
fb_update_options_free(FacebookUpdateOptions *self)
{
  DEBUG_LOG(__func__);

  g_free(self);

  return TRUE;
}

static FacebookUpdateOptions *
fb_update_options_new(SharingAccount *account, ConIcConnection *con,
                      gboolean *continue_update, gboolean *dead_mans_switch,
                      UpdateOptionsCallback cb_func, gpointer cb_data)
{
  FacebookUpdateOptions *options = g_try_new0(FacebookUpdateOptions, 1);

  DEBUG_LOG(__func__);

  if (options)
  {
    options->account = account;
    options->con = con;
    options->cb_func = cb_func;
    options->continue_update = continue_update;
    options->dead_mans_switch = dead_mans_switch;
    options->cb_data = cb_data;
  }

  return options;
}

static int
fb_http_progress_callback(gdouble bytes_sent, FacebookUpdateOptions *options)
{
  *options->dead_mans_switch = FALSE;

  while (g_main_context_pending(0))
    g_main_context_iteration(0, 1);

  return (*options->continue_update == FALSE);
}

static void
albums_cb(JsonArray *array, guint index, JsonNode *element_node,
          gpointer user_data)
{
  JsonObject *object = json_node_get_object(element_node);
  GSList **albums = user_data;

  if (object)
  {
    const gchar *name;

    if(json_object_has_member(object, "name"))
    {
      name = json_object_get_string_member(object, "name");

      if (json_object_has_member(object, "id"))
      {
        const gchar *id = json_object_get_string_member(object, "id");
        const gchar *description = NULL;
        gboolean can_upload =
            json_object_get_boolean_member(object, "can_upload");

        if (json_object_has_member(object, "description"))
          description = json_object_get_string_member(object, "description");

        if (can_upload || !g_strcmp0(name, "Mobile Uploads"))
          *albums =
            g_slist_append(*albums,
                           sharing_service_option_value_new(id, name,
                                                            description));
      }
    }
  }
}

int
fb_update_options_request(FacebookUpdateOptions *options)
{
  gchar *access_token;
  facebook_graph_request *request;
  GString *url;
  int http_result;
  GSList *albums = NULL;
  GArray *response;
  HttpProgress progress;
  GError *error;

  DEBUG_LOG(__func__);

  pthread_setcancelstate(0, 0);
  pthread_setcanceltype(1, 0);
  options->result = SHARING_UPDATE_OPTIONS_ERROR_UNKNOWN;

  access_token = sharing_account_get_param(options->account, "access_token");
  if (access_token && *access_token && *access_token != ' ')
  {
    request = facebook_graph_request_new();
    request->access_token = access_token;

    progress.callback = (HttpProgressCallback)fb_http_progress_callback;
    progress.user_data = options;

    g_hash_table_insert(request->query_params, "access_token",
                        g_strdup(request->access_token));
    g_hash_table_insert(request->query_params, "fields",
                        g_strdup("id,name,description,can_upload"));
    error = NULL;
    response = g_array_new(FALSE, FALSE, 1);
    url = g_string_new("https://graph.facebook.com/me/albums");

    http_result = network_utils_get_with_progress(url,
                                                  response,
                                                  NULL,
                                                  request->query_params,
                                                  options->con,
                                                  &progress,
                                                  &error);
    g_string_free(url, FALSE);

    if (http_result == 200)
    {
      if (response)
      {
        JsonParser *parser = json_parser_new();

        if (!json_parser_load_from_data(parser, response->data, response->len,
                                        &error))
        {
          if (error)
          {
            g_warning("%s\n", error->message);
            g_error_free(error);
          }
        }
        else
        {
          JsonNode *root = json_parser_get_root(parser);
          if (root && JSON_NODE_HOLDS_OBJECT(root))
          {
            JsonObject *object = json_node_get_object(root);

            if (object && json_object_has_member(object, "data"))
            {
              JsonArray *data = json_object_get_array_member(object, "data");

              if (data)
                json_array_foreach_element(data, albums_cb, &albums);
            }
          }
        }

        if (albums)
        {
          sharing_account_set_option_values(options->account, "album",
                                            albums);
          sharing_service_option_values_free(albums);
        }

        g_object_unref(parser);
      }

      g_array_free(response, TRUE);
      options->result = SHARING_UPDATE_OPTIONS_SUCCESS;
    }
    else
    {
      g_array_free(response, TRUE);

      if (error)
      {
        if (error->code != -1022 && error->code == -1021)
          options->result = SHARING_UPDATE_OPTIONS_CANCELLED;
        else
          options->result = SHARING_UPDATE_OPTIONS_ERROR_UNKNOWN;

        g_error_free(error);
      }
      else
        options->result = SHARING_UPDATE_OPTIONS_ERROR_UNKNOWN;
    }
  }

  g_free(access_token);
  options->thread_active = FALSE;

  return 0;
}

static gboolean
fb_update_options_run(FacebookUpdateOptions *self)
{
  self->result = SHARING_UPDATE_OPTIONS_ERROR_UNKNOWN;
  self->thread_active = TRUE;
  g_thread_create_full((GThreadFunc)fb_update_options_request, self, 0, 0, 0,
                       G_THREAD_PRIORITY_NORMAL, 0);

  while (self->thread_active)
  {
    while ( g_main_context_pending(NULL) )
      g_main_context_iteration(0, 1);
  }

  if (self->cb_func)
    self->cb_func(self->result, self->cb_data);

  return TRUE;
}

gboolean sharing_plugin_interface_update_options(SharingAccount *account,
                                                 ConIcConnection *con,
                                                 gboolean *continue_update,
                                                 gboolean *dead_mans_switch,
                                                 UpdateOptionsCallback cb_func,
                                                 gpointer cb_data)
{
  FacebookUpdateOptions *options;
  gboolean rv;

  options = fb_update_options_new(account, con, continue_update,
                                  dead_mans_switch, cb_func, cb_data);
  rv = fb_update_options_run(options);
  fb_update_options_free(options);

  return rv;
}

guint
sharing_plugin_interface_init(gboolean *dead_mans_switch)
{
  DEBUG_LOG(__func__);

  fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);

  return 0;
}

guint
sharing_plugin_interface_uninit(gboolean *dead_mans_switch)
{
  DEBUG_LOG(__func__);

  fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);

  return 0;
}

SharingPluginInterfaceEditAccountResult
fb_sharing_plugin_edit_account(GtkWindow *parent, SharingAccount *account,
                               ConIcConnection *con, gboolean *dead_mans_switch)
{
  gchar *s;
  gchar *username;
  GtkWidget *table;
  GtkWidget *email_label;
  GtkWidget *username_label;
  GtkWidget *password_label;
  GtkWidget *password_entry;
  GtkWidget *dialog;
  SharingPluginInterfaceEditAccountResult rv;

  DEBUG_LOG(__func__);

  s = dgettext("osso-sharing-ui", "share_ti_cpa_edit_account");
  s = g_strdup_printf(s, "Facebook");
  dialog =
      gtk_dialog_new_with_buttons(
        s,
        parent,
        GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_DESTROY_WITH_PARENT |
                                                               GTK_DIALOG_MODAL,
        dgettext("hildon-libs", "wdgt_bd_delete"),
        2,
        dgettext("hildon-libs", "wdgt_bd_save"),
        1,
        NULL);
  g_free(s);

  /* table */
  table = gtk_table_new(2, 2, 0);
  gtk_table_set_col_spacings(GTK_TABLE(table), 8);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, 1, 1, 0);

  /* email label */
  email_label = gtk_label_new("Email");
  gtk_misc_set_alignment(GTK_MISC(email_label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table), email_label, 0, 1, 0, 1, GTK_FILL,
                   GTK_FILL, 0, 0);

  /* username label */
  username_label = gtk_label_new(NULL);
  gtk_misc_set_alignment(GTK_MISC(username_label), 0.0, 0.5);
  gtk_misc_set_padding(GTK_MISC(username_label), 16, 0);
  username = sharing_account_get_param(account, (const gchar *)"username");
  if (username)
  {
    gtk_label_set_text(GTK_LABEL(username_label), username);
    g_free(username);
  }
  gtk_table_attach_defaults(GTK_TABLE(table), username_label, 1, 3, 0, 1);

  /* password label */
  password_label =
      gtk_label_new(dgettext("osso-sharing-ui",
                             "share_fi_cpa_edit_account_password"));
  gtk_misc_set_alignment(GTK_MISC(password_label), 0.0, 0.5);
  gtk_table_attach(GTK_TABLE(table),
                   password_label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

  /* password entry */
  password_entry = hildon_entry_new(HILDON_SIZE_AUTO_WIDTH);
  hildon_gtk_widget_set_theme_size(password_entry, HILDON_SIZE_FINGER_HEIGHT);
  gtk_entry_set_max_length(GTK_ENTRY(password_entry),
                           sharing_service_get_password_max_length(
                                         sharing_account_get_service(account)));
  hildon_gtk_entry_set_input_mode(GTK_ENTRY(password_entry),
                                  HILDON_GTK_INPUT_MODE_FULL |
                                  HILDON_GTK_INPUT_MODE_INVISIBLE);
  gtk_table_attach(GTK_TABLE(table), password_entry, 1, 2, 1, 2,
                   GTK_FILL | GTK_EXPAND, GTK_FILL, 16, 0);

  /* show it */
  g_object_set_data(G_OBJECT(dialog), "password_entry", password_entry);
  gtk_widget_show_all(dialog);

  for ( ; ; )
  {
    switch (gtk_dialog_run(GTK_DIALOG(dialog)))
    {
      case 1:
        if (fb_sharing_plugin_editor_validate_changes(dialog, account))
        {
          rv = SHARING_EDIT_ACCOUNT_SUCCESS;
          goto out;
        }
        break;
      case 2:
        rv = SHARING_EDIT_ACCOUNT_DELETE;
        goto out;
      default:
        rv = SHARING_EDIT_ACCOUNT_CANCELLED;
        goto out;
    }
  }

out:
  gtk_widget_destroy(dialog);

  return rv;
}

SharingPluginInterfaceEditAccountResult
sharing_plugin_interface_edit_account(GtkWindow *parent,
                                      SharingAccount *account,
                                      ConIcConnection *con,
                                      gboolean *dead_mans_switch)
{
  return fb_sharing_plugin_edit_account(parent, account, con, dead_mans_switch);
}

static gboolean
fb_sharing_plugin_transfer_progress(gdouble ulnow, PluginDataTransfer *userdata)
{
  gboolean rv = FALSE;
  SharingTransfer *transfer = userdata->transfer;

  DEBUG_LOG(__func__);

  if (transfer)
  {
    rv = (sharing_transfer_continue(transfer) == FALSE);

    if (userdata->upload_sum_size)
    {
      sharing_transfer_set_progress(
            transfer,CLAMP((gdouble)(userdata->upload_done + ulnow) /
                           (gdouble)userdata->upload_sum_size, 0.0, 1.0));
    }
    else
      sharing_transfer_set_progress(transfer, 0.0);
  }

  if (userdata->dead_mans_switch)
    *userdata->dead_mans_switch = FALSE;

  return rv;
}

static int
fb_sharing_plugin_send_video(facebook_graph_request *request,
                             const gchar *access_token,
                             const SharingEntryMedia *media, GArray *response,
                             ConIcConnection *con, HttpProgress *progress,
                             GError **error)
{
  /* TODO - add places support */
  const gchar* localpath;
  gchar *path;
  GString *url;
  const gchar *desc = sharing_entry_media_get_desc(media);
  gchar *mime;
  gchar *title;
  gchar* place_id;
  int rv;

  DEBUG_LOG(__func__);

  facebook_graph_request_reset(request);

  if (desc && *desc)
    g_hash_table_insert(request->query_params, "description", g_strdup(desc));

  title = sharing_entry_media_get_title(media);
  if (title && *title)
    g_hash_table_insert(request->query_params, "title", g_strdup(title));
  g_free(title);

  mime = sharing_entry_media_get_mime(media);
  localpath = sharing_entry_media_get_localpath(media);
  path = g_strjoin(".", localpath, mime + 6, NULL);
  g_free(mime);

  place_id = fb_sharing_plugin_get_place_id(media, request, access_token,
                                            localpath, con, TRUE);

  url = g_string_new("https://graph-video.facebook.com/me/videos");
  rename(localpath, path);

  if (place_id)
  {
    g_hash_table_insert(request->query_params, "place",
                        g_strdup(place_id));
  }

  g_hash_table_insert(request->query_params, "access_token",
                      g_strdup(access_token));

  rv = network_utils_post_multipart_with_progress(url, path,
                                                  request->query_params,
                                                  response, con, progress,
                                                  error);

  rename(path, localpath);
  g_free(place_id);
  g_string_free(url, FALSE);

  return rv;
}



static int
fb_sharing_plugin_send_photo(facebook_graph_request *request,
                             const gchar *access_token, const gchar *album,
                             const SharingEntryMedia *media, GArray *response,
                             ConIcConnection *con, HttpProgress *progress,
                             GError **error)
{
  const gchar* localpath;
  gchar* place_id;
  gchar *s;
  GString *url;
  const gchar *desc = sharing_entry_media_get_desc(media);
  gchar *title = sharing_entry_media_get_title(media);
  GString *message = g_string_new(NULL);
  int rv;

  DEBUG_LOG(__func__);

  localpath = sharing_entry_media_get_localpath(media);
  place_id = fb_sharing_plugin_get_place_id(media, request, access_token,
                                            localpath, con, FALSE);

  facebook_graph_request_reset(request);

  if (title && *title)
    g_string_append(message, title);

  if (desc && *desc)
  {
    /* FIXME */
    int len = message->len;
    if (len)
    {
      int len_plus_1 = len + 1;
      if (len + 1 >= message->allocated_len)
      {
        g_string_insert_c(message, -1, ',');
      }
      else
      {
        gchar *str;
        message->str[len] = ',';
        str = message->str;
        message->len = len_plus_1;
        str[len_plus_1] = 0;
      }
    }
    g_string_append(message, desc);
  }

  if (message->len)
    g_hash_table_insert(request->query_params, "message",
                        g_strdup(message->str));

  g_string_free(message, TRUE);
  g_free(title);

  if (!g_strcmp0(album, "Mobile Uploads"))
    s = g_strdup_printf("https://graph.facebook.com/me/photos");
  else
    s = g_strdup_printf("https://graph.facebook.com/%s/photos", album);

  url = g_string_new(s);

  if (place_id)
  {
    g_hash_table_insert(request->query_params, "place",
                        g_strdup(place_id));
  }

  g_hash_table_insert(request->query_params, "access_token",
                      g_strdup(access_token));

  rv = network_utils_post_multipart_with_progress(url,
                                                  localpath,
                                                  request->query_params,
                                                  response,
                                                  con,
                                                  progress,
                                                  error);
  g_free(place_id);
  g_string_free(url, TRUE);

  return rv;
}

static SharingPluginInterfaceSendResult
fb_sharing_plugin_send(SharingTransfer *transfer, ConIcConnection *con,
                       gboolean *dead_mans_switch)
{
  gchar *access_token;
  SharingPluginInterfaceSendResult rv = SHARING_SEND_ERROR_UNKNOWN;
  facebook_graph_request *request;

  gchar *mime;
  GArray *response;
  int http_response;

  SharingEntry *entry;
  SharingAccount *account;
  GSList *media_list;
  PluginDataTransfer data_transfer;
  HttpProgress progress;
  GError *error = NULL;

  DEBUG_LOG(__func__);

  *dead_mans_switch = FALSE;

  if (!(entry = sharing_transfer_get_entry(transfer)))
    return SHARING_SEND_ERROR_UNKNOWN;

  if (!(account = sharing_entry_get_account(entry)))
    return SHARING_SEND_ERROR_ACCOUNT_NOT_FOUND;

  sharing_account_ref(account);

  access_token = sharing_account_get_param(account, "access_token");

  if (!access_token || !*access_token || *access_token == ' ')
  {
    g_free(access_token);

    return SHARING_SEND_ERROR_AUTH;
  }

  request = facebook_graph_request_new();

  fb_sharing_plugin_reset_dead_mans_switch(dead_mans_switch);

  data_transfer.transfer = transfer;
  data_transfer.upload_done = 0LL;
  data_transfer.dead_mans_switch = dead_mans_switch;
  data_transfer.upload_sum_size = sharing_entry_get_size(entry);

  progress.user_data = &data_transfer;
  progress.callback = (HttpProgressCallback)fb_sharing_plugin_transfer_progress;

  for (media_list = sharing_entry_get_media(entry); media_list;
       media_list = media_list->next)
  {
    SharingEntryMedia *media = (SharingEntryMedia *)media_list->data;

    if (sharing_entry_media_get_sent(media))
      continue;

    mime = sharing_entry_media_get_mime(media);

    response = g_array_new(1, 1, 1);

    if (g_strcmp0(mime, "image/jpeg") && g_strcmp0(mime, "image/png"))
    {
      http_response = fb_sharing_plugin_send_video(request, access_token,
                                                   media, response, con,
                                                   &progress, &error);
    }
    else
    {
      const gchar *album = sharing_entry_get_option(entry, "album");

      http_response = fb_sharing_plugin_send_photo(request, access_token, album,
                                                   media, response, con,
                                                   &progress, &error);
    }

    g_free(mime);

    if (http_response == 200)
    {
      sharing_entry_media_set_sent(media, TRUE);
      data_transfer.upload_done += sharing_entry_media_get_size(media);
      sharing_entry_sync_filesystem(entry);
      rv = SHARING_SEND_SUCCESS;
    }
    else
    {
      g_array_free(response, 1);

      if (error)
      {
        g_warning("%s\n", error->message);
        switch (error->code)
        {
          case -1022:
            rv = SHARING_SEND_ERROR_CONNECTION;
            break;
          case -1021:
            rv = SHARING_SEND_CANCELLED;
            break;
          default:
            rv = SHARING_SEND_ERROR_UNKNOWN;
            break;
        }

        g_error_free(error);
      }
      else
        rv = SHARING_SEND_ERROR_UNKNOWN;

      break;
    }
  }

  sharing_account_unref(account);
  facebook_graph_request_free(request);
  g_free(access_token);

  return rv;
}

SharingPluginInterfaceSendResult
sharing_plugin_interface_send(SharingTransfer *transfer, ConIcConnection *con,
                              gboolean *dead_mans_switch)
{
  return fb_sharing_plugin_send(transfer, con, dead_mans_switch);
}
