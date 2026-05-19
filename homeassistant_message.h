#ifndef _HOMEASSISTANT_MESSAGE_H_
#define _HOMEASSISTANT_MESSAGE_H_

#include "libhomeassistant.h"

int ha_send_command(HAAccount *ha, const char *who, const char *message);
PurpleCmdRet ha_cmd_subscribe(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
PurpleCmdRet ha_cmd_unsubscribe(PurpleConversation *conv, const gchar *cmd, gchar **args, gchar **error, void *data);
void ha_subscribe(HAAccount *ha, const gchar *entity_id);
void ha_unsubscribe(HAAccount *ha, const gchar *entity_id);

#endif /* _HOMEASSISTANT_MESSAGE_H_ */
