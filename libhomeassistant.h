#ifndef _LIBHOMEASSISTANT_H_
#define _LIBHOMEASSISTANT_H_

#include <purple.h>
#include "purplecompat.h"
#include <glib.h>

#define HOMEASSISTANT_PLUGIN_ID "prpl-homeassistant"
#define HOMEASSISTANT_PLUGIN_VERSION "0.1"

typedef struct _HAAccount {
    PurpleAccount *account;
    PurpleConnection *pc;
    gchar *server_url;
    gchar *api_key;
    GHashTable *entities; // entity_id -> HABuddy mapping or state
    GHashTable *areas; // area_id -> area_name
    GHashTable *entity_areas; // entity_id -> area_id
    
    // WebSocket
    PurpleSslConnection *websocket;
    PurpleProxyConnectData *websocket_conn_data;
    int websocket_fd;
    guint websocket_inpa;
    gboolean is_ssl;
    
    gboolean websocket_header_received;
    guint message_id;
    guint get_areas_msg_id;
    guint get_entities_msg_id;
    guint get_states_msg_id;
    gchar *frame;
    guint64 frame_len;
    guint64 frame_len_progress;
    guchar packet_code;
    gint frames_since_reconnect;
} HAAccount;

#endif /* _LIBHOMEASSISTANT_H_ */
