#include "libhomeassistant.h"
#include "homeassistant_connection.h"
#include "homeassistant_message.h"
#include "homeassistant_websocket.h"
#include <json-glib/json-glib.h>

//static PurpleCmdId cmd_on_id;
//static PurpleCmdId cmd_off_id;
//static PurpleCmdId cmd_toggle_id;
static PurpleCmdId cmd_subscribe_id;
static PurpleCmdId cmd_unsubscribe_id;

static const char *
ha_list_icon(PurpleAccount *account, PurpleBuddy *buddy)
{
    return "homeassistant";
}

static const char *
ha_list_emblem(PurpleBuddy *buddy)
{
    PurpleAccount *account = purple_buddy_get_account(buddy);
    if (!account) return NULL;
    
    PurpleConnection *pc = purple_account_get_connection(account);
    if (!pc) return NULL;
    
    HAAccount *ha = purple_connection_get_protocol_data(pc);
    if (!ha || !ha->subscriptions) return NULL;
    
    const gchar *entity_id = purple_buddy_get_name(buddy);
    if (g_hash_table_lookup(ha->subscriptions, entity_id) != NULL) {
        return "voice";
    }
    
    return NULL;
}

static gchar *
ha_status_text(PurpleBuddy *buddy)
{
    PurplePresence *presence = purple_buddy_get_presence(buddy);
    PurpleStatus *status = purple_presence_get_active_status(presence);
    return g_strdup(purple_status_get_name(status));
}

static void
ha_tooltip_text(PurpleBuddy *buddy, PurpleNotifyUserInfo *user_info, gboolean full)
{
    PurplePresence *presence = purple_buddy_get_presence(buddy);
    PurpleStatus *status = purple_presence_get_active_status(presence);
    
    const gchar *message = purple_status_get_attr_string(status, "message");
    if (message && *message) {
        gchar *escaped = g_markup_printf_escaped("%s", message);
        purple_notify_user_info_add_pair_html(user_info, _("Message"), escaped);
        g_free(escaped);
    }
}

static GList *
ha_status_types(PurpleAccount *account)
{
    GList *types = NULL;

#if PURPLE_VERSION_CHECK(3, 0, 0)
    types = g_list_append(types, purple_status_type_new(PURPLE_STATUS_AVAILABLE, "online", "Online", TRUE));
    types = g_list_append(types, purple_status_type_new(PURPLE_STATUS_AWAY, "away", "Away", TRUE));
    types = g_list_append(types, purple_status_type_new(PURPLE_STATUS_OFFLINE, "offline", "Offline", TRUE));
#else
    types = g_list_append(types, purple_status_type_new_with_attrs(
        PURPLE_STATUS_AVAILABLE, "online", "Online", TRUE, TRUE, FALSE,
        "message", "Message", purple_value_new(PURPLE_TYPE_STRING), NULL));
    types = g_list_append(types, purple_status_type_new_with_attrs(
        PURPLE_STATUS_AWAY, "away", "Away", TRUE, TRUE, FALSE,
        "message", "Message", purple_value_new(PURPLE_TYPE_STRING), NULL));
    types = g_list_append(types, purple_status_type_new_with_attrs(
        PURPLE_STATUS_OFFLINE, "offline", "Offline", TRUE, TRUE, FALSE,
        "message", "Message", purple_value_new(PURPLE_TYPE_STRING), NULL));
#endif

    return types;
}

static void
ha_login(PurpleAccount *account)
{
    PurpleConnection *pc = purple_account_get_connection(account);
    HAAccount *ha;

    ha = g_new0(HAAccount, 1);
    ha->account = account;
    ha->pc = pc;
    ha->server_url = g_strdup(purple_account_get_string(account, "server_url", ""));
    ha->api_key = g_strdup(purple_account_get_string(account, "api_key", ""));
    ha->entities = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)json_object_unref);
    ha->areas = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    ha->entity_areas = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    ha->subscriptions = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    ha->devices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    ha->device_areas = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    ha->entity_devices = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    ha->device_contacts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    const gchar *subs_str = purple_account_get_string(account, "subscriptions", "");
    if (subs_str && *subs_str) {
        gchar **subs = g_strsplit(subs_str, ",", -1);
        int i;
        for (i = 0; subs[i] != NULL; i++) {
            g_strstrip(subs[i]);
            if (subs[i][0] != '\0') {
                g_hash_table_replace(ha->subscriptions, g_strdup(subs[i]), GINT_TO_POINTER(1));
            }
        }
        g_strfreev(subs);
    }

    purple_connection_set_protocol_data(pc, ha);
    
    // Set connection state to connected for now
    purple_connection_set_state(pc, PURPLE_CONNECTED);
    
    // Fetch initial states
    // ha_fetch_states(ha);
    ha_websocket_connect(ha);
}

static void
ha_close(PurpleConnection *pc)
{
    HAAccount *ha = purple_connection_get_protocol_data(pc);

    if (ha) {
        ha_websocket_close(ha);
        g_free(ha->server_url);
        g_free(ha->api_key);
        g_hash_table_destroy(ha->entities);
        g_hash_table_destroy(ha->areas);
        g_hash_table_destroy(ha->entity_areas);
        g_hash_table_destroy(ha->subscriptions);
        g_hash_table_destroy(ha->devices);
        g_hash_table_destroy(ha->device_areas);
        g_hash_table_destroy(ha->entity_devices);
        g_hash_table_destroy(ha->device_contacts);
        g_free(ha);
    }
    purple_connection_set_protocol_data(pc, NULL);
}

static int
ha_im_send(PurpleConnection *pc, const char *who, const char *message, PurpleMessageFlags flags)
{
    HAAccount *ha = purple_connection_get_protocol_data(pc);
    return ha_send_command(ha, who, message);
}

static void
ha_unsubscribe_from_node(PurpleBlistNode *node, gpointer userdata)
{
	if(PURPLE_IS_BUDDY(node)) {
        PurpleBuddy *buddy = PURPLE_BUDDY(node);
        PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
        HAAccount *ha = purple_connection_get_protocol_data(pc);

        if (ha != NULL) {
            ha_unsubscribe(ha, purple_buddy_get_name(buddy));
        }
    }
}

static void
ha_subscribe_from_node(PurpleBlistNode *node, gpointer userdata)
{
	if(PURPLE_IS_BUDDY(node)) {
        PurpleBuddy *buddy = PURPLE_BUDDY(node);
        PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
        HAAccount *ha = purple_connection_get_protocol_data(pc);

        if (ha != NULL) {
            ha_subscribe(ha, purple_buddy_get_name(buddy));
        }
    }
}

static GList *
ha_node_menu(PurpleBlistNode *node)
{
	GList *m = NULL;
	PurpleMenuAction *act;
	PurpleBuddy *buddy;
	
	if(PURPLE_IS_BUDDY(node))
	{
		buddy = PURPLE_BUDDY(node);
        PurpleConnection *pc = purple_account_get_connection(purple_buddy_get_account(buddy));
        HAAccount *ha = purple_connection_get_protocol_data(pc);
		
		if (ha != NULL) {
            if (g_hash_table_lookup(ha->subscriptions, purple_buddy_get_name(buddy)) != NULL) {
                act = purple_menu_action_new(_("Unsubscribe"),
                                PURPLE_CALLBACK(ha_unsubscribe_from_node),
                                ha, NULL);
                m = g_list_append(m, act);
            } else {
                act = purple_menu_action_new(_("Subscribe"),
                                PURPLE_CALLBACK(ha_subscribe_from_node),
                                ha, NULL);
                m = g_list_append(m, act);
            }
		}
	}
	
	return m;
}

static PurplePluginProtocolInfo prpl_info = {
    .options            = OPT_PROTO_NO_PASSWORD,
    .icon_spec          = { "png", 16, 16, 16, 16, 0, PURPLE_ICON_SCALE_DISPLAY },
    .list_icon          = ha_list_icon,
    .list_emblem        = ha_list_emblem,
    .status_text        = ha_status_text,
    .tooltip_text       = ha_tooltip_text,
    .status_types       = ha_status_types,
    .blist_node_menu    = ha_node_menu,
    .chat_info          = NULL,
    .chat_info_defaults = NULL,
    .login              = ha_login,
    .close              = ha_close,
    .send_im            = ha_im_send,
    .set_info           = NULL,
    .send_typing        = NULL,
    .get_info           = NULL,
    .set_status         = NULL,
    .set_idle           = NULL,
    .change_passwd      = NULL,
    .add_buddy          = NULL,
    .add_buddies        = NULL,
    .remove_buddy       = NULL,
    .remove_buddies     = NULL,
    .add_permit         = NULL,
    .add_deny           = NULL,
    .rem_permit         = NULL,
    .rem_deny           = NULL,
    .set_permit_deny    = NULL,
    .join_chat          = NULL, // TODO: Group chat support
    .reject_chat        = NULL,
    .get_chat_name      = NULL,
    .chat_invite        = NULL,
    .chat_leave         = NULL,
    .chat_whisper       = NULL,
    .chat_send          = NULL,
    .keepalive          = NULL,
    .register_user      = NULL,
    .get_cb_info        = NULL,
    .get_cb_away        = NULL,
    .alias_buddy        = NULL,
    .group_buddy        = NULL,
    .rename_group       = NULL,
    .buddy_free         = NULL,
    .convo_closed       = NULL,
    .normalize          = NULL,
    .set_buddy_icon     = NULL,
    .remove_group       = NULL,
    .get_cb_real_name   = NULL,
    .set_chat_topic     = NULL,
    .find_blist_chat    = NULL,
    .roomlist_get_list  = NULL,
    .roomlist_cancel    = NULL,
    .roomlist_expand_category = NULL,
    .can_receive_file   = NULL,
    .send_file          = NULL,
    .new_xfer           = NULL,
    .offline_message    = NULL,
    .whiteboard_prpl_ops = NULL,
    .send_raw           = NULL,
    .roomlist_room_serialize = NULL,
    .unregister_user    = NULL,
    .send_attention     = NULL,
    .get_attention_types = NULL,
    .struct_size        = sizeof(PurplePluginProtocolInfo)
};

static gboolean
plugin_load(PurplePlugin *plugin)
{
    cmd_subscribe_id = purple_cmd_register(
        "subscribe", 
        "s", 
        PURPLE_CMD_P_DEFAULT, 
        PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, 
        HOMEASSISTANT_PLUGIN_ID, 
        ha_cmd_subscribe, 
        "subscribe [device_id]: Subscribe to notifications for a device", 
        NULL
    );
    cmd_unsubscribe_id = purple_cmd_register(
        "unsubscribe", 
        "s", 
        PURPLE_CMD_P_DEFAULT, 
        PURPLE_CMD_FLAG_IM | PURPLE_CMD_FLAG_PRPL_ONLY | PURPLE_CMD_FLAG_ALLOW_WRONG_ARGS, 
        HOMEASSISTANT_PLUGIN_ID, 
        ha_cmd_unsubscribe, 
        "unsubscribe [device_id]: Unsubscribe from notifications for a device", 
        NULL
    );
    return TRUE;
}

static gboolean
plugin_unload(PurplePlugin *plugin)
{
    purple_cmd_unregister(cmd_subscribe_id);
    purple_cmd_unregister(cmd_unsubscribe_id);
    return TRUE;
}

static void
init_plugin(PurplePlugin *plugin)
{
    PurpleAccountOption *option;

    option = purple_account_option_string_new("Server URL", "server_url", "https://homeassistant.local:8123");
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);

    option = purple_account_option_string_new("Long-Lived Access Token", "api_key", "");
    prpl_info.protocol_options = g_list_append(prpl_info.protocol_options, option);
}

static PurplePluginInfo info = {
    .magic          = PURPLE_PLUGIN_MAGIC,
    .major_version  = PURPLE_MAJOR_VERSION,
    .minor_version  = PURPLE_MINOR_VERSION,
    .type           = PURPLE_PLUGIN_PROTOCOL,
    .ui_requirement = NULL,
    .flags          = 0,
    .dependencies   = NULL,
    .priority       = PURPLE_PRIORITY_DEFAULT,
    .id             = HOMEASSISTANT_PLUGIN_ID,
    .name           = "HomeAssistant",
    .version        = HOMEASSISTANT_PLUGIN_VERSION,
    .summary        = "HomeAssistant Protocol Plugin",
    .description    = "Connects to HomeAssistant API to control devices.",
    .author         = "Antigravity",
    .homepage       = "",
    .load           = plugin_load,
    .unload         = plugin_unload,
    .destroy        = NULL,
    .ui_info        = NULL,
    .extra_info     = &prpl_info,
    .prefs_info     = NULL,
    .actions        = NULL,
    //.padding        = { NULL, NULL, NULL, NULL }
};

PURPLE_INIT_PLUGIN(homeassistant, init_plugin, info);
