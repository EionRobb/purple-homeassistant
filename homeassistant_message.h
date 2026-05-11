#ifndef _HOMEASSISTANT_MESSAGE_H_
#define _HOMEASSISTANT_MESSAGE_H_

#include "libhomeassistant.h"

int ha_send_command(HAAccount *ha, const char *who, const char *message);

#endif /* _HOMEASSISTANT_MESSAGE_H_ */
