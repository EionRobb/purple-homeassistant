#ifndef _HOMEASSISTANT_WEBSOCKET_H_
#define _HOMEASSISTANT_WEBSOCKET_H_

#include "libhomeassistant.h"

void ha_websocket_connect(HAAccount *ha);
void ha_websocket_close(HAAccount *ha);
void ha_websocket_call_service(HAAccount *ha, const gchar *domain, const gchar *service, const gchar *entity_id);

#endif /* _HOMEASSISTANT_WEBSOCKET_H_ */
