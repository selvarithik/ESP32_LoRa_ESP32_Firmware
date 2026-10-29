# ESP32 red/white header update — 2026-10-28

Appearance-only update to the ESP32 dashboard/configuration, MQTT and login/account pages.

- Reuses the exact NASA-style SVG wordmark from the existing Gateway global header.
- White wordmark on a fixed #FF0000 header, with a desktop page-title divider.
- Removes company branding from the navigation sidebar.
- White navigation with red labels, active red edge/underline and soft red selection background.
- Responsive desktop/sidebar and mobile/wrapped navigation; no external fonts or CDNs.
- Existing session, authentication, handlers, topics, settings and role behavior remain intact.
- Gateway source/design was not modified in this appearance update.

Changed: src/web/local_web_server.cpp, src/web/mqtt_page.h, src/web/access_page.h,
and generated src/web/web_assets_gzip.h.

Checks passed: local mocked UI at 1280/375 px; MQTT and login/account mocked UI at
1280/360 px; gzip source equality; shared contract check; Git whitespace check.
Firmware build and artifact results are recorded in docs/esp32dev_BUILD_EVIDENCE.json.

Preview images use simulated data and do not prove a live connection. No board upload
or flash/partition change was performed in this appearance update.
