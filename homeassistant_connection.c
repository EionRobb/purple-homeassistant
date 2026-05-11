#include "homeassistant_connection.h"
#include "purple2compat/http.h"
#include <json-glib/json-glib.h>

static void
ha_update_buddy_status(HAAccount *ha, PurpleBuddy *buddy, const gchar *state)
{
    const gchar *status_id = "offline";
    
    if (g_strcmp0(state, "on") == 0 || g_strcmp0(state, "home") == 0) {
        status_id = "online";
    } else if (g_strcmp0(state, "off") == 0 || g_strcmp0(state, "not_home") == 0) {
        status_id = "away";
    } else if (g_strcmp0(state, "unavailable") == 0) {
        status_id = "offline";
    } else {
        // For other states (e.g. numbers, sensor values), we just put them as online 
        // and show the state in the status message.
        status_id = "online";
    }

    purple_prpl_got_user_status(ha->account, purple_buddy_get_name(buddy), status_id, NULL);
}

void
ha_process_entities(HAAccount *ha, JsonArray *array)
{
    guint i, length = json_array_get_length(array);
    for (i = 0; i < length; i++) {
        JsonObject *obj = json_array_get_object_element(array, i);
        const gchar *entity_id = json_object_get_string_member(obj, "entity_id");
        const gchar *state = json_object_get_string_member(obj, "state");
        JsonObject *attributes = json_object_has_member(obj, "attributes") ? json_object_get_object_member(obj, "attributes") : NULL;
        
        const gchar *friendly_name = entity_id;
        if (attributes && json_object_has_member(attributes, "friendly_name")) {
            friendly_name = json_object_get_string_member(attributes, "friendly_name");
        }
        
        gchar **parts = g_strsplit(entity_id, ".", 2);
        const gchar *domain = parts[0];
        const gchar *group_name = domain;
        
        const gchar *area_id = g_hash_table_lookup(ha->entity_areas, entity_id);
        if (area_id) {
            const gchar *area_name = g_hash_table_lookup(ha->areas, area_id);
            if (area_name) {
                group_name = area_name;
            }
        }
        
        // Find or create group
        PurpleGroup *group = purple_find_group(group_name);
        if (!group) {
            group = purple_group_new(group_name);
            purple_blist_add_group(group, NULL);
        }
        
        // Find or create buddy
        PurpleBuddy *buddy = purple_find_buddy(ha->account, entity_id);
        if (!buddy) {
            buddy = purple_buddy_new(ha->account, entity_id, friendly_name);
            purple_blist_add_buddy(buddy, NULL, group, NULL);
        } else {
            purple_blist_alias_buddy(buddy, friendly_name);
        }
        
        g_strfreev(parts);
        
        ha_update_buddy_status(ha, buddy, state);
    }
}

void
ha_process_state_change_event(HAAccount *ha, JsonObject *new_state)
{
    const gchar *entity_id = json_object_get_string_member(new_state, "entity_id");
    const gchar *state = json_object_get_string_member(new_state, "state");
    
    if (entity_id && state) {
        PurpleBuddy *buddy = purple_find_buddy(ha->account, entity_id);
        if (buddy) {
            ha_update_buddy_status(ha, buddy, state);
        } else {
            // New entity discovered that we didn't have during initial states fetch
            JsonArray *array = json_array_new();
            json_array_add_object_element(array, json_object_ref(new_state));
            ha_process_entities(ha, array);
            json_array_unref(array);
        }
    }
}


