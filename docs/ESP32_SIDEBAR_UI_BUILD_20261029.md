# ESP32 sidebar UI — 29 October 2026

Implemented only the embedded ESP32 pages. No upload or physical device test was performed. Existing unrelated working-tree modifications were preserved.

## Changes
- Removed full-width top header on Dashboard, Network, Device, Radio, Nodes, MQTT and System pages.
- Added the existing NASA-style SELVARITHIK A&D vector above the white sidebar; red menu labels, active marker, consistent inline SVG icons, horizontal mobile navigation.
- Moved the existing live clock and authenticated connection indicators into page content; retained truthful pending/off/error states and reduced-motion support.
- Header-free login/account layout, compact red brand panel and existing authentication handlers, forced-password-change flow, error messages and HTTP warning.
- Subtle offline CSS grid outside white panels. No remote fonts, CDNs or image dependencies.
- Per the final scope correction, all sensor-reading tiles and sensor health/readings in the expanded status view were removed. Device details use a communication-only allowlist. Sensor collection, configuration and Gateway telemetry remain unchanged.
- Retained all existing forms, mutation protections, MQTT staging workflow, routes and configuration semantics.

## Files changed in this task
- src/web/local_web_server.cpp — embedded HTML/CSS/JS only; C++ outside HTML verified unchanged.
- src/web/mqtt_page.h
- src/web/access_page.h
- src/web/web_assets_gzip.h — regenerated deterministically.
- test/local_page.cjs, test/mqtt_page.cjs, test/access_page.cjs — responsive shell, navigation, sensor error and existing interaction coverage.
- This report, JSON build evidence, and mocked browser screenshots.

## Verification
- Local UI: 1280/375 px; all existing interaction checks, absence of sensor readings from Dashboard/details, sidebar clearance and no horizontal overflow.
- MQTT UI: 1280/360 px; staging/cancel, required TLS, secret masking, mismatches, existing forms.
- Access UI: 1280/360 px; login error, password visibility, forced first-use change, sign-in and logout.
- Network, Device, Radio, Nodes and System route navigation checked at both local UI widths.
- Contract check passed, unchanged SHA-256 6f463f36075885baca81aa1e7cefa44d23d0d11817d860c6e4a6324a59f51248.
- Native C++ command rules and exact 21-command registry parity passed.
- Gzip generator/decompression and --check passed.
- PlatformIO esp32dev build passed: application 1,344,105 / 3,145,728 bytes (42.7%), static RAM 75,556 / 327,680 bytes (23.1%). Binary 1,350,816 bytes.
- Existing huge_app partition configuration retained; no partition changes, flashing, erase or filesystem operation.
- Configuration/defaults, sensor implementation, canonical MQTT source and all C++ outside the embedded HTML match the pre-task SHA-256 baseline.

## Limits
All browser tests used mock HTTP responses, not hardware. Gateway source, profiles, Wi-Fi and sensor settings were not changed. Physical rendering, connection indicators, sensor readings and board memory behavior must be checked later when the ESP32 is available. No Gateway regression run was needed for an ESP32-only change. The approved preview remains a visual reference; live data is displayed only where supported by the existing device API.
