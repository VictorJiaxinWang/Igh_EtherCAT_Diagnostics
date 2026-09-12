# Changelog

## 3.1.0 - 2026-09-12

- Add configurable monitoring for Master 0, Master 1, or both.
- Use EEPROM Alias identity for loss, recovery, and fault-boundary detection across rescans.
- Publish multi-Master status and identify Master/Alias in events and the Web UI.

## 3.0.0 - 2026-09-05

- Add the standalone `igh-ethercat-diagnostics-web` C++ HTTP service.
- Add a responsive Chinese dashboard for health, topology, root cause, evidence, recovery steps, and recent events.
- Highlight the Master entry failure or the last-alive/first-lost slave boundary.
- Detect stale diagnostic data without requiring synchronized board and browser clocks.
- Add read-only status/events APIs, hardened systemd deployment, Web asset installation, and loopback HTTP tests.
- Complete the V3.0 operator Web UI and deployment path.

## 2.2.0 - 2026-09-05

- Deliver the V2.2 evidence-based root-cause diagnosis pipeline.
- Use direct IgH ioctl access in the production monitoring path.
- Add ESC port-error counter decoding and delta events.
- Add evidence windows, configurable root-cause weights, confidence, explanations, and calibration matrix.
- Publish atomic `latest_status.json` and append-only `events.jsonl` outputs.
- Include systemd deployment, graceful shutdown, black-box recording, recovery tracking, and active ESC diagnosis.

## 1.1.0

- Detect one-shot recovery events after a fault episode.
- Add active ESC register Burst and Cooldown coordination.
- Preserve fault context in JSONL black-box captures.
