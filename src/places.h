#ifndef PLACES_H
#define PLACES_H

#include <conicconnection.h>
#include <facebook/feedserviceutils2.h>
#include <facebook/common.h>
#include <sharing-plugin-interface.h>

gchar *
fb_sharing_plugin_get_place_id(const SharingEntryMedia *media,
                               facebook_graph_request *request,
                               const gchar *access_token,
                               const gchar *path, ConIcConnection *con,
                               gboolean is_video);

/* #define DEBUG_LOG(msg) g_debug(msg) */
#define DEBUG_LOG(msg)

#endif // PLACES_H
