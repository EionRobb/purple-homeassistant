# HomeAssistant Plugin for libpurple (Pidgin)

A `libpurple` protocol plugin to connect to HomeAssistant. It shows devices as buddies in your buddy list and groups them by room (Area). You can send messages to buddies to control them or interrogate their valid options.

## Usage

You can control your devices by sending Instant Messages to them. 

- **Basic Commands:** Send `on`, `off`, or `toggle` to turn devices on or off.
- **Help / Device Interrogation:** Send `help` or `?` to any device. The plugin will interrogate the device's capabilities and reply with a list of valid commands and options for that specific device (e.g. supported HVAC modes or select options).
- **Dynamic Options:** If a device has specific options (like a "Coffee Machine Program" that takes `Espresso` or `Latte`), you can send the exact option name in the chat to select it.
- **Cover Positions:** Send a number between `0` and `100` to cover devices to set their position.
- **Light Controls:** For lights, send a number between `0` and `100` (or with a `%` suffix) to set the brightness percentage. You can also send a color name (e.g. `red`, `blue`, `green`, `purple`, `warm_white`) to change the color.
- **Thermostat (Climate) Controls:** For thermostats, send a number (e.g. `21` or `21.5`) to set the target temperature. You can also send any supported HVAC mode (e.g. `heat`, `cool`, `auto`, `off`) or preset mode to change the mode.

## Enhanced Buddy Statuses

The plugin automatically registers status messages for your buddies in Pidgin, so you can see their real-time state at a glance in your buddy list:

- **Sensors:** Displays the state and unit of measurement (e.g., `21.5 °C` or `303.46 L`).
- **Covers:** Displays the state and position (e.g., `open (50%)`).
- **Lights:** Displays the brightness level when on (e.g., `On (75%)`).
- **Thermostats:** Displays the HVAC mode and target/current temperatures (e.g., `cool (Target: 21.0°C, Current: 20.5°C)`).
- **Selects:** Displays the currently selected option (e.g., `Espresso`).

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
