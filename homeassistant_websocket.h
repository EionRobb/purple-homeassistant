#ifndef _HOMEASSISTANT_WEBSOCKET_H_
#define _HOMEASSISTANT_WEBSOCKET_H_

#include "libhomeassistant.h"
#include <json-glib/json-glib.h>

void ha_websocket_connect(HAAccount *ha);
void ha_websocket_close(HAAccount *ha);
void ha_websocket_call_service(HAAccount *ha, const gchar *domain, const gchar *service, const gchar *entity_id);
void ha_websocket_call_service_with_data(HAAccount *ha, const gchar *domain, const gchar *service, JsonObject *service_data);

#endif /* _HOMEASSISTANT_WEBSOCKET_H_ */
