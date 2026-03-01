---
title: "BLE Page-Turn Remote Support"
description: "Add BLE HID keyboard remote support for hands-free page turning via NimBLE Central on ESP32-C3"
status: done
priority: P2
effort: 16h
branch: feat/ble-page-turn-remote
tags: [feature, hardware, ble, controls]
created: 2026-02-28
---

# BLE Page-Turn Remote Support

## Overview

Add Bluetooth Low Energy page-turn remote support to CrossPoint Reader (personal fork). Uses NimBLE-Arduino as BLE Central to connect to standard HID keyboard remotes. BLE button events inject at HalGPIO level — all downstream code works unchanged.

## Context

- [Brainstorm Report](../reports/brainstorm-260228-0034-ble-page-turn-remote.md)
- [BLE Capabilities Research](../reports/researcher-260225-esp32c3-ble-capabilities.md)
- [BLE Quick Reference](../reports/QUICK-REFERENCE-esp32c3-ble.md)

## Phases

| # | Phase | Status | Effort | Deps | Link |
|---|-------|--------|--------|------|------|
| 1 | NimBLE integration & HalGPIO injection | Done | 3h | - | [phase-01](./phase-01-nimble-halgpio-integration.md) |
| 2 | BLE HID client & report parsing | Done | 4h | P1 | [phase-02](./phase-02-ble-hid-client.md) |
| 3 | Settings & persistence | Done | 3h | P1 | [phase-03](./phase-03-settings-persistence.md) |
| 4 | Pairing UI activity | Done | 4h | P2,P3 | [phase-04](./phase-04-pairing-ui-activity.md) |
| 5 | Auto-reconnect & WiFi mutual exclusion | Done | 2h | P2,P3 | [phase-05](./phase-05-auto-reconnect-wifi-exclusion.md) |

## Dependencies

- NimBLE-Arduino library (`h2zero/NimBLE-Arduino`)
- BLE HID keyboard remote (hardware for testing)
- Personal fork (not upstream — conflicts with SCOPE.md)

## Key Constraints

- ~50-60KB RAM for NimBLE Central stack
- WiFi and BLE mutually exclusive (shared 2.4GHz RF)
- Deep sleep kills BLE radio — reconnect on wake (~1-3s)
- Single-core ESP32-C3 — NimBLE runs as FreeRTOS task
