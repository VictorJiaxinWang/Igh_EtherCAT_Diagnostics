# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/).

## [1.1.0] - 2026-09-03

### Added

- 1 Hz monitoring of IgH EtherCAT master and slave status.
- Unified `NetworkSnapshot` data model.
- Master-link, slave-count, lost-slave, and AL-state change events.
- Lost-slave boundary location.
- In-memory snapshot ring buffer and JSONL fault black box.
- Burst reads of ESC DL Status, AL Status, and AL Status Code registers.
- Ten-second Cooldown for repeated active-diagnosis bursts.
- One-shot network recovery events with duration and restored slave positions.
- Graceful shutdown on `SIGINT` and `SIGTERM`.
- `esc-diagnostic-probe` command-line utility.

### Verified

- Built and tested on Debian GNU/Linux running on RK3588 (AArch64).
- Passed 22 automated tests and a physical EtherCAT cable removal/recovery test.
