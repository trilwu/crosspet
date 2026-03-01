# Phase 3: Settings & Persistence

## Context

- [Phase 1](./phase-01-nimble-halgpio-integration.md) — HalGPIO injection
- [CrossPointSettings pattern](../../src/CrossPointSettings.h) — singleton, JSON serialization
- [SettingsList pattern](../../src/SettingsList.h) — shared settings list for device + web UI

## Overview

- **Priority:** P1
- **Status:** Pending
- **Description:** Add BLE settings to CrossPointSettings (enable toggle, bonded device address). Extend JSON serialization. Add settings list entries for device UI.

## Key Insights

- Settings stored as JSON on SD card at `/.crosspoint/settings.json`
- `CrossPointSettings` is a singleton (`SETTINGS` macro)
- `JsonSettingsIO::saveSettings/loadSettings` handles serialization — must add new fields
- `SettingsList.h` `getSettingsList()` defines all settings for both device UI and web API
- Settings categories: Display, Reader, Controls, System. BLE fits in **Controls** or **System**
- Bonded address: NimBLE has internal bonding database, but we also store address in settings for directed reconnect on wake
- Keep it simple: `bleEnabled` toggle + `bleBondedDeviceAddr` string. No key mapping UI in V1 (hardcoded defaults).

## Requirements

### Functional
- `bleEnabled` toggle (default: off) — controls whether BLE initializes on boot
- `bleBondedDeviceAddr` string — MAC address of last paired remote (for auto-reconnect)
- `bleBondedDeviceName` string — display name of paired remote (for settings UI)
- Settings persist across reboots via JSON
- New settings load gracefully on first boot (defaults applied if missing from JSON)

### Non-Functional
- Zero RAM when BLE disabled (settings struct cost is negligible)
- Backward compatible: old settings.json without BLE fields loads fine

## Related Code Files

### Modify
- `src/CrossPointSettings.h` — add `bleEnabled`, `bleBondedDeviceAddr`, `bleBondedDeviceName`
- `src/JsonSettingsIO.cpp` — add JSON save/load for new fields
- `src/SettingsList.h` — add BLE toggle to Controls category
- `src/activities/settings/SettingsActivity.h` — add `SettingAction::BleRemote`
- `src/activities/settings/SettingsActivity.cpp` — handle BLE action in `toggleCurrentSetting()`
- `lib/I18n/translations/english.yaml` — add BLE string keys

### Reference (read-only)
- `src/activities/settings/ButtonRemapActivity.cpp` — pattern for sub-activity from settings

## Implementation Steps

1. **Add BLE fields to CrossPointSettings.h**

   ```cpp
   // BLE Remote settings
   uint8_t bleEnabled = 0;  // 0=disabled, 1=enabled
   char bleBondedDeviceAddr[18] = "";   // "XX:XX:XX:XX:XX:XX" format
   char bleBondedDeviceName[32] = "";   // Display name of bonded remote
   ```

2. **Add JSON serialization in JsonSettingsIO.cpp**

   In `saveSettings()`:
   ```cpp
   doc["bleEnabled"] = s.bleEnabled;
   doc["bleBondedDeviceAddr"] = s.bleBondedDeviceAddr;
   doc["bleBondedDeviceName"] = s.bleBondedDeviceName;
   ```

   In `loadSettings()`:
   ```cpp
   s.bleEnabled = doc["bleEnabled"] | (uint8_t)0;

   const char* bleAddr = doc["bleBondedDeviceAddr"] | "";
   strncpy(s.bleBondedDeviceAddr, bleAddr, sizeof(s.bleBondedDeviceAddr) - 1);
   s.bleBondedDeviceAddr[sizeof(s.bleBondedDeviceAddr) - 1] = '\0';

   const char* bleName = doc["bleBondedDeviceName"] | "";
   strncpy(s.bleBondedDeviceName, bleName, sizeof(s.bleBondedDeviceName) - 1);
   s.bleBondedDeviceName[sizeof(s.bleBondedDeviceName) - 1] = '\0';
   ```

3. **Add I18n strings in english.yaml**

   ```yaml
   STR_BLE_REMOTE: "Bluetooth Remote"
   STR_BLE_ENABLED: "Bluetooth"
   STR_BLE_PAIR_DEVICE: "Pair Remote"
   STR_BLE_SCANNING: "Scanning..."
   STR_BLE_CONNECTING: "Connecting..."
   STR_BLE_CONNECTED: "Connected"
   STR_BLE_DISCONNECTED: "Not connected"
   STR_BLE_NO_DEVICES: "No devices found"
   STR_BLE_PAIRED_WITH: "Paired: %s"
   STR_BLE_FORGET_DEVICE: "Forget Device"
   ```

4. **Add settings list entry in SettingsList.h**

   In Controls section:
   ```cpp
   // --- Controls ---
   SettingInfo::Toggle(StrId::STR_BLE_ENABLED, &CrossPointSettings::bleEnabled,
                       "bleEnabled", StrId::STR_CAT_CONTROLS),
   ```

5. **Add SettingAction for BLE pairing**

   In `SettingsActivity.h`:
   ```cpp
   enum class SettingAction {
     None,
     RemapFrontButtons,
     // ...existing...
     BleRemote,  // NEW
   };
   ```

6. **Add BLE action entry in SettingsActivity.cpp onEnter()**

   After the RemapFrontButtons insert:
   ```cpp
   controlsSettings.push_back(
       SettingInfo::Action(StrId::STR_BLE_PAIR_DEVICE, SettingAction::BleRemote));
   ```

7. **Handle BLE action in toggleCurrentSetting()**

   ```cpp
   case SettingAction::BleRemote:
     startActivityForResult(
         std::make_unique<BleRemotePairingActivity>(renderer, mappedInput),
         resultHandler);
     break;
   ```

8. **Run gen_i18n.py to regenerate I18n headers**

   ```sh
   python3 scripts/gen_i18n.py lib/I18n/translations lib/I18n/
   ```

9. **Compile and verify**

   ```sh
   pio run --environment default
   ```

## Todo List

- [ ] Add `bleEnabled`, `bleBondedDeviceAddr`, `bleBondedDeviceName` to CrossPointSettings.h
- [ ] Add JSON save/load for BLE fields in JsonSettingsIO.cpp
- [ ] Add BLE I18n string keys to english.yaml
- [ ] Run gen_i18n.py to regenerate headers
- [ ] Add BLE toggle to Controls category in SettingsList.h
- [ ] Add `SettingAction::BleRemote` enum value
- [ ] Add BLE action entry in SettingsActivity.cpp
- [ ] Handle BleRemote action in toggleCurrentSetting() (forward declare activity)
- [ ] Compile and verify no errors

## Success Criteria

- BLE toggle appears in Settings → Controls
- Toggle persists across reboots
- Old settings.json (without BLE fields) loads without error
- "Pair Remote" action visible in Controls (navigates to pairing activity in Phase 4)

## Risk Assessment

| Risk | Mitigation |
|------|------------|
| JSON doc size increase | Minimal: 3 small fields. ArduinoJson handles gracefully. |
| Bonded address format mismatch | Validate MAC format on load. Clear if invalid. |
| I18n generation breaks | Run gen_i18n.py and verify generated files compile. |

## Next Steps

→ Phase 4: BleRemotePairingActivity (scan UI, device list, connect)
