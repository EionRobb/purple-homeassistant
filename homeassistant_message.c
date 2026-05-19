#include "homeassistant_message.h"
#include "homeassistant_websocket.h"
#include <json-glib/json-glib.h>
#include <stdlib.h>

static void
ha_send_help(HAAccount *ha, const char *who, JsonObject *attributes, const gchar *domain)
{
    GString *help = g_string_new("Valid commands:\n- on\n- off\n- toggle");
    
    if (g_strcmp0(domain, "cover") == 0) {
        g_string_append(help, "\n- <number> (0-100) to set position");
    } else if (g_strcmp0(domain, "light") == 0) {
        g_string_append(help, "\n- <number> (0-100) or <number>% to set brightness\n- <color name> (e.g. red, blue, green) to set color");
    } else if (g_strcmp0(domain, "climate") == 0) {
        g_string_append(help, "\n- <number> to set target temperature");
        if (attributes) {
            if (json_object_has_member(attributes, "hvac_modes")) {
                JsonNode *modes_node = json_object_get_member(attributes, "hvac_modes");
                if (json_node_get_node_type(modes_node) == JSON_NODE_ARRAY) {
                    g_string_append(help, "\n\nHVAC Modes:");
                    JsonArray *modes = json_node_get_array(modes_node);
                    guint i, len = json_array_get_length(modes);
                    for (i = 0; i < len; i++) {
                        g_string_append_printf(help, "\n- %s", json_array_get_string_element(modes, i));
                    }
                }
            }
        }
    }
    
    if (attributes) {
        if (json_object_has_member(attributes, "options")) {
            JsonNode *options_node = json_object_get_member(attributes, "options");
            if (json_node_get_node_type(options_node) == JSON_NODE_ARRAY) {
                g_string_append(help, "\n\nOptions:");
                JsonArray *options = json_node_get_array(options_node);
                guint i, len = json_array_get_length(options);
                for (i = 0; i < len; i++) {
                    g_string_append_printf(help, "\n- %s", json_array_get_string_element(options, i));
                }
            }
        }
        
        if (json_object_has_member(attributes, "preset_modes")) {
            JsonNode *preset_node = json_object_get_member(attributes, "preset_modes");
            if (json_node_get_node_type(preset_node) == JSON_NODE_ARRAY) {
                g_string_append(help, "\n\nPreset Modes:");
                JsonArray *presets = json_node_get_array(preset_node);
                guint i, len = json_array_get_length(presets);
                for (i = 0; i < len; i++) {
                    g_string_append_printf(help, "\n- %s", json_array_get_string_element(presets, i));
                }
            }
        }
    }
    
    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, ha->account);
    if (conv) {
        purple_conversation_write(conv, who, help->str, PURPLE_MESSAGE_SYSTEM, time(NULL));
    }
    g_string_free(help, TRUE);
}

int
ha_send_command(HAAccount *ha, const char *who, const char *message)
{
    // 'who' is the entity_id
    gchar **parts = g_strsplit(who, ".", 2);
    if (!parts || !parts[0] || !parts[1]) {
        g_strfreev(parts);
        return -1;
    }
    
    const gchar *domain = parts[0];
    const gchar *service = NULL;
    
    gchar *cmd = g_ascii_strdown(message, -1);
    g_strstrip(cmd);
    
    JsonObject *entity_obj = g_hash_table_lookup(ha->entities, who);
    JsonObject *attributes = NULL;
    if (entity_obj && json_object_has_member(entity_obj, "attributes")) {
        attributes = json_object_get_object_member(entity_obj, "attributes");
    }
    
    if (g_strcmp0(cmd, "on") == 0) {
        service = "turn_on";
    } else if (g_strcmp0(cmd, "off") == 0) {
        service = "turn_off";
    } else if (g_strcmp0(cmd, "toggle") == 0) {
        service = "toggle";
    } else if (g_strcmp0(cmd, "help") == 0 || g_strcmp0(cmd, "?") == 0) {
        ha_send_help(ha, who, attributes, domain);
        g_free(cmd);
        g_strfreev(parts);
        return 1;
    } else if (purple_strequal(cmd, "subscribe")) {
        ha_subscribe(ha, who);
        return 1;
    } else if (purple_strequal(cmd, "unsubscribe")) {
        ha_unsubscribe(ha, who);
        return 1;
    } else {
        gboolean handled = FALSE;
        
        if (g_strcmp0(domain, "cover") == 0) {
            char *endptr;
            long val = strtol(cmd, &endptr, 10);
            if (*endptr == '\0' && val >= 0 && val <= 100) {
                JsonObject *service_data = json_object_new();
                json_object_set_string_member(service_data, "entity_id", who);
                json_object_set_int_member(service_data, "position", val);
                ha_websocket_call_service_with_data(ha, domain, "set_cover_position", service_data);
                handled = TRUE;
            }
        }
        
        if (g_strcmp0(domain, "light") == 0) {
            char *endptr;
            gchar *clean_cmd = g_strdup(cmd);
            if (g_str_has_suffix(clean_cmd, "%")) {
                clean_cmd[strlen(clean_cmd) - 1] = '\0';
            }
            long val = strtol(clean_cmd, &endptr, 10);
            if (*endptr == '\0' && val >= 0 && val <= 100) {
                JsonObject *service_data = json_object_new();
                json_object_set_string_member(service_data, "entity_id", who);
                json_object_set_int_member(service_data, "brightness_pct", val);
                ha_websocket_call_service_with_data(ha, domain, "turn_on", service_data);
                handled = TRUE;
            }
            g_free(clean_cmd);
            
            if (!handled && g_ascii_isalpha(cmd[0])) {
                JsonObject *service_data = json_object_new();
                json_object_set_string_member(service_data, "entity_id", who);
                json_object_set_string_member(service_data, "color_name", cmd);
                ha_websocket_call_service_with_data(ha, domain, "turn_on", service_data);
                handled = TRUE;
            }
        }
        
        if (g_strcmp0(domain, "climate") == 0) {
            char *endptr;
            double val = g_ascii_strtod(cmd, &endptr);
            if (*endptr == '\0') {
                JsonObject *service_data = json_object_new();
                json_object_set_string_member(service_data, "entity_id", who);
                json_object_set_double_member(service_data, "temperature", val);
                ha_websocket_call_service_with_data(ha, domain, "set_temperature", service_data);
                handled = TRUE;
            }
            
            if (!handled && attributes) {
                if (json_object_has_member(attributes, "hvac_modes")) {
                    JsonNode *modes_node = json_object_get_member(attributes, "hvac_modes");
                    if (json_node_get_node_type(modes_node) == JSON_NODE_ARRAY) {
                        JsonArray *modes = json_node_get_array(modes_node);
                        guint i, len = json_array_get_length(modes);
                        for (i = 0; i < len; i++) {
                            const gchar *mode = json_array_get_string_element(modes, i);
                            gchar *mode_lower = g_ascii_strdown(mode, -1);
                            if (g_strcmp0(cmd, mode_lower) == 0) {
                                JsonObject *service_data = json_object_new();
                                json_object_set_string_member(service_data, "entity_id", who);
                                json_object_set_string_member(service_data, "hvac_mode", mode);
                                ha_websocket_call_service_with_data(ha, domain, "set_hvac_mode", service_data);
                                handled = TRUE;
                                g_free(mode_lower);
                                break;
                            }
                            g_free(mode_lower);
                        }
                    }
                }
                
                if (!handled && json_object_has_member(attributes, "preset_modes")) {
                    JsonNode *presets_node = json_object_get_member(attributes, "preset_modes");
                    if (json_node_get_node_type(presets_node) == JSON_NODE_ARRAY) {
                        JsonArray *presets = json_node_get_array(presets_node);
                        guint i, len = json_array_get_length(presets);
                        for (i = 0; i < len; i++) {
                            const gchar *preset = json_array_get_string_element(presets, i);
                            gchar *preset_lower = g_ascii_strdown(preset, -1);
                            if (g_strcmp0(cmd, preset_lower) == 0) {
                                JsonObject *service_data = json_object_new();
                                json_object_set_string_member(service_data, "entity_id", who);
                                json_object_set_string_member(service_data, "preset_mode", preset);
                                ha_websocket_call_service_with_data(ha, domain, "set_preset_mode", service_data);
                                handled = TRUE;
                                g_free(preset_lower);
                                break;
                            }
                            g_free(preset_lower);
                        }
                    }
                }
            }
        }
        
        if (!handled && attributes && json_object_has_member(attributes, "options")) {
            JsonNode *options_node = json_object_get_member(attributes, "options");
            if (json_node_get_node_type(options_node) == JSON_NODE_ARRAY) {
                JsonArray *options = json_node_get_array(options_node);
                guint i, len = json_array_get_length(options);
                for (i = 0; i < len; i++) {
                    const gchar *opt = json_array_get_string_element(options, i);
                    gchar *opt_lower = g_ascii_strdown(opt, -1);
                    if (g_strcmp0(cmd, opt_lower) == 0) {
                        JsonObject *service_data = json_object_new();
                        json_object_set_string_member(service_data, "entity_id", who);
                        json_object_set_string_member(service_data, "option", opt);
                        ha_websocket_call_service_with_data(ha, "select", "select_option", service_data);
                        handled = TRUE;
                        g_free(opt_lower);
                        break;
                    }
                    g_free(opt_lower);
                }
            }
        }
        
        if (!handled) {
            ha_send_help(ha, who, attributes, domain);
        }
        
        g_free(cmd);
        g_strfreev(parts);
        return 1;
    }
    
    g_free(cmd);
    
    if (service) {
        ha_websocket_call_service(ha, domain, service, who);
    }
    
    g_strfreev(parts);
    
    return 1;
}

static void
ha_save_subscriptions(HAAccount *ha)
{
    GList *keys = g_hash_table_get_keys(ha->subscriptions);
    GString *subs_str = g_string_new("");
    GList *l;
    for (l = keys; l != NULL; l = l->next) {
        if (subs_str->len > 0) {
            g_string_append_c(subs_str, ',');
        }
        g_string_append(subs_str, (const gchar *)l->data);
    }
    g_list_free(keys);
    
    purple_account_set_string(ha->account, "subscriptions", subs_str->str);
    g_string_free(subs_str, TRUE);
}

void
ha_subscribe(HAAccount *ha, const gchar *entity_id)
{
    g_hash_table_replace(ha->subscriptions, g_strdup(entity_id), GINT_TO_POINTER(1));
    ha_save_subscriptions(ha);

    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, entity_id, ha->account);
    if (!conv) {
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, ha->account, entity_id);
    }
    if (conv) {
        gchar *msg = g_strdup_printf("Subscribed to notifications for %s.", entity_id);
        purple_conversation_write(conv, NULL, msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
    }
}

void
ha_unsubscribe(HAAccount *ha, const gchar *entity_id)
{
    g_hash_table_remove(ha->subscriptions, entity_id);
    ha_save_subscriptions(ha);

    PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, entity_id, ha->account);
    if (!conv) {
        conv = purple_conversation_new(PURPLE_CONV_TYPE_IM, ha->account, entity_id);
    }
    if (conv) {
        gchar *msg;
        if (g_hash_table_lookup(ha->subscriptions, entity_id) == NULL) {
            msg = g_strdup_printf("You are not subscribed to %s.", entity_id);
        } else {
            msg = g_strdup_printf("Unsubscribed from notifications for %s.", entity_id);
        }
        purple_conversation_write(conv, NULL, msg, PURPLE_MESSAGE_SYSTEM, time(NULL));
        g_free(msg);
    }
}

PurpleCmdRet
ha_cmd_subscribe(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
    PurpleConnection *pc = purple_conversation_get_connection(conv);
    if (!pc) {
        return PURPLE_CMD_RET_CONTINUE;
    }
    
    PurpleAccount *account = purple_connection_get_account(pc);
    if (g_strcmp0(purple_account_get_protocol_id(account), HOMEASSISTANT_PLUGIN_ID) != 0) {
        return PURPLE_CMD_RET_CONTINUE;
    }
    
    HAAccount *ha = purple_connection_get_protocol_data(pc);
    if (!ha) {
        return PURPLE_CMD_RET_FAILED;
    }
    
    const gchar *entity_id = NULL;
    if (args && args[0] && args[0][0] != '\0') {
        entity_id = args[0];
    } else {
        entity_id = purple_conversation_get_name(conv);
    }
    
    if (!entity_id || !g_hash_table_lookup(ha->entities, entity_id)) {
        purple_conversation_write(conv, NULL, "Please specify a valid device entity ID to subscribe to, or run this command in a device conversation window.", PURPLE_MESSAGE_SYSTEM, time(NULL));
        return PURPLE_CMD_RET_OK;
    }
    
    ha_subscribe(ha, entity_id);
    return PURPLE_CMD_RET_OK;
}

PurpleCmdRet
ha_cmd_unsubscribe(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data)
{
    PurpleConnection *pc = purple_conversation_get_connection(conv);
    if (!pc) {
        return PURPLE_CMD_RET_CONTINUE;
    }
    
    PurpleAccount *account = purple_connection_get_account(pc);
    if (g_strcmp0(purple_account_get_protocol_id(account), HOMEASSISTANT_PLUGIN_ID) != 0) {
        return PURPLE_CMD_RET_CONTINUE;
    }
    
    HAAccount *ha = purple_connection_get_protocol_data(pc);
    if (!ha) {
        return PURPLE_CMD_RET_FAILED;
    }
    
    const gchar *entity_id = NULL;
    if (args && args[0] && args[0][0] != '\0') {
        entity_id = args[0];
    } else {
        entity_id = purple_conversation_get_name(conv);
    }
    
    if (!entity_id) {
        purple_conversation_write(conv, NULL, "Please specify a device entity ID to unsubscribe from, or run this command in a device conversation window.", PURPLE_MESSAGE_SYSTEM, time(NULL));
        return PURPLE_CMD_RET_OK;
    }

    ha_unsubscribe(ha, entity_id);
    return PURPLE_CMD_RET_OK;
}
