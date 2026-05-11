# HomeAssistant Plugin for libpurple (Pidgin)

A `libpurple` protocol plugin to connect to HomeAssistant. It shows devices as buddies in your buddy list and groups them by room (Area). You can send messages like "on", "off", or "toggle" to buddies to control them.

## Installation

### Windows
1. Download the latest `libhomeassistant.dll` from the [Releases](https://github.com/eionrobb/purple-homeassistant/releases) page.
2. Copy `libhomeassistant.dll` to your Pidgin plugins directory (usually `C:\Program Files (x86)\Pidgin\plugins`).
3. Copy the icons (`homeassistant16.png`, etc.) to `C:\Program Files (x86)\Pidgin\pixmaps\pidgin\protocols\...` in their respective size folders (16, 22, 48).

## How to Create an API Key in Home Assistant

To use this plugin, you need a **Long-Lived Access Token** from Home Assistant:

1. Open your Home Assistant web interface.
2. Click on your **Username** at the bottom of the left sidebar to open your **Profile**.
3. Scroll all the way to the bottom of the page to the **Long-Lived Access Tokens** section.
4. Click the **Create Token** button.
5. Enter a name for the token (e.g., `Pidgin`) and click **OK**.
6. A long string of characters will be displayed. **Copy this token immediately** and save it somewhere safe, as it will never be displayed again.

## Configuration in Pidgin

1. In Pidgin, go to **Accounts** -> **Manage Accounts** -> **Add**.
2. Select **HomeAssistant** from the Protocol dropdown.
3. In the **Username** field, you can put anything (it is not used for authentication).
4. In the **Server URL** field, enter the full URL of your Home Assistant instance (e.g., `http://homeassistant.local:8123` or `https://your-ha-domain.com`).
5. In the **API Key** (or Token) field, paste the Long-Lived Access Token you created above.
6. Click **Save**.

## Building from Source

This plugin requires `libpurple` development headers and `json-glib-1.0`.

### Linux
```sh
make
sudo make install
```

### Windows (Cross-compiling)
The `Makefile` is set up for cross-compiling from Linux/Cygwin targeting Windows, assuming the standard Pidgin `win32-dev` environment is present in parent directories.
```sh
make
make install
```
