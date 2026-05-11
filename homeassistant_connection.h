#ifndef _HOMEASSISTANT_CONNECTION_H_
#define _HOMEASSISTANT_CONNECTION_H_

#include "libhomeassistant.h"
#include <json-glib/json-glib.h>

void ha_process_state_change_event(HAAccount *ha, JsonObject *new_state);
void ha_process_entities(HAAccount *ha, JsonArray *array);

#endif /* _HOMEASSISTANT_CONNECTION_H_ */
