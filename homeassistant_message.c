#include "homeassistant_message.h"
#include "homeassistant_websocket.h"
#include <json-glib/json-glib.h>



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
    
    // Simplistic command parsing
    gchar *cmd = g_ascii_strdown(message, -1);
    g_strstrip(cmd);
    
    if (g_strcmp0(cmd, "on") == 0) {
        service = "turn_on";
    } else if (g_strcmp0(cmd, "off") == 0) {
        service = "turn_off";
    } else if (g_strcmp0(cmd, "toggle") == 0) {
        service = "toggle";
    } else {
        // unsupported command for now
        g_free(cmd);
        g_strfreev(parts);
        
        // Return 1 meaning message handled/displayed, but we might want to tell the user it failed
        PurpleConversation *conv = purple_find_conversation_with_account(PURPLE_CONV_TYPE_IM, who, ha->account);
        if (conv) {
            purple_conversation_write(conv, who, "Unsupported command. Try 'on', 'off', or 'toggle'.", PURPLE_MESSAGE_SYSTEM, time(NULL));
        }
        return 1;
    }
    g_free(cmd);
    
    ha_websocket_call_service(ha, domain, service, who);
    
    g_strfreev(parts);
    
    return 1; // 1 means message was sent successfully
}
