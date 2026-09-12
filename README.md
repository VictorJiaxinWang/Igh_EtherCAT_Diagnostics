# IgH EtherCAT Diagnostics

[简体中文](README.zh-CN.md) | English

An independent EtherCAT diagnostic daemon for systems built on the IgH EtherCAT Master.

Version: **v3.1.0**  
Author: **Victor-Jiaxin Wang**  
License: **MIT**

## What it does

The daemon observes an existing IgH EtherCAT network without joining the PDO real-time loop or requesting ownership of the Master. It samples the network at 1 Hz, detects topology and AL-state changes, preserves snapshots around a fault, locates the likely break boundary, reads ESC diagnostic registers, tracks port-error deltas, reports recovery, and produces an explainable root-cause assessment.

It publishes two files and presents them through an operator-oriented Web UI:

- `logs/latest_status.json`: the current status, replaced atomically.
- `logs/events.jsonl`: append-only fault, recovery, and root-cause events.

The standalone `igh-ethercat-diagnostics-web` process serves a read-only dashboard at port 8080. Operators see network health, a slave-chain topology with the likely break highlighted, a plain-language root cause, confidence, evidence, recovery steps, and recent events. Keeping it separate means Web traffic cannot block EtherCAT collection.

## When to use it

- Commissioning and troubleshooting systems that use IgH EtherCAT Master.
- Capturing intermittent cable, connector, power, downstream-slave, and AL-state failures.
- Adding diagnostics beside an existing motion-control application without changing its real-time code.

This software is a diagnostic aid, not a safety function and not an automatic repair system.

## Highlights

- Direct IgH ioctl production backend; no shell process in the monitoring path.
- Unified `NetworkSnapshot` model and injectable acquisition interfaces.
- Lost-slave, state-change, link, recovery, and port-error events.
- Rolling history and JSONL black-box captures.
- Active ESC reads with Burst and Cooldown control.
- Fault-boundary location and evidence-based root-cause ranking.
- Atomic Web status output and append-only event output.
- Zero-dependency C++ HTTP service and responsive Chinese operator dashboard.
- systemd unit, graceful `SIGINT`/`SIGTERM` shutdown, and automated tests.
- Configurable monitoring of Master 0, Master 1, or both with isolated per-Master diagnostic state.
- Stable EEPROM Alias identity (`alias:relative_position`); Position is retained only for current ioctl addressing after a rescan.

## Architecture

```text
/dev/EtherCAT0,1 -> ioctl backend -> isolated per-Master snapshots -> 1 Hz Monitor
                                             |
        +--------------------+---------------+-------------------+
        |                    |                                   |
   event/recovery       rolling history                 port-error deltas
        |                    |                                   |
        +------------ DiagnosisCoordinator ---------------------+
                              |
              boundary + active ESC + root cause
                              |
             black box + latest_status + events
                              |
                              v
                  C++ Web service -> browser
```

Detailed responsibilities and data flow are documented in [Software Architecture](docs/ARCHITECTURE.md). The Web output contract is in [Web Data Schema](docs/WEB_DATA_SCHEMA.md).

## Prerequisites

- Linux and a working IgH EtherCAT Master installation.
- A readable IgH device node, normally `/dev/EtherCAT0`.
- C++17 compiler, CMake 3.18 or newer, and Make or Ninja.
- The vendored IgH 1.6.3 ABI headers must match the running Master ABI. If your installation differs, replace the headers and run the tests before using the daemon.

### Verified reference environment

| Component | Verified value |
| --- | --- |
| Board | Rockchip RK3588 TOYBRICK X10 Board |
| Operating system | Debian GNU/Linux 11 (bullseye) |
| Architecture | AArch64 (`aarch64`) |
| Kernel | Linux 5.10.161 |
| Compiler | GCC/G++ 10.2.1 |
| CMake | 3.18.4 |
| IgH EtherCAT Master | 1.6.3, runtime ioctl magic 32 |
| Kernel modules | `ec_master`, `ec_generic` |
| Device node | `/dev/EtherCAT0`, group `ethercat`, mode `0660` |

This is a reference, not a platform restriction. Other Linux boards are suitable when the architecture, IgH ABI, and permissions are correct.

## Build and test

```bash
git clone <your-repository-url>
cd igh-ethercat-diagnostics

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
cd build && ctest --output-on-failure
cd ..
```

Main executables:

- `build/igh-ethercat-diagnostics`: continuous diagnostic daemon.
- `build/igh-ethercat-diagnostics-web`: read-only HTTP API and dashboard server.
- `build/esc-diagnostic-probe`: one-shot ESC register probe.
- `build/root_cause_matrix_demo`: root-cause calibration matrix.
- `build/snapshot_backend_compare`: ioctl/legacy-shell migration checker.

Unit tests use injected data and normally need no EtherCAT hardware. Hardware probes require the Master and device node.

## Run

First confirm the Master and permissions:

```bash
systemctl status ethercat --no-pager
ls -l /dev/EtherCAT0
ethercat master -m 0
ethercat slaves -m 0
```

Run from a writable directory because runtime data is stored below `./logs`:

```bash
mkdir -p "$HOME/igh-ethercat-diagnostics-runtime"
cd "$HOME/igh-ethercat-diagnostics-runtime"
/path/to/repository/build/igh-ethercat-diagnostics --masters 0,1
```

If the current user cannot open `/dev/EtherCAT0`, add the user to the device's group and reconnect the login session, or run the probe as root. Avoid making the device world-writable.

```bash
sudo usermod -aG ethercat "$USER"
# Log out and back in, then verify with: id

/path/to/repository/build/esc-diagnostic-probe 0 3
```

## Install and deploy with systemd

After installation, select the monitored Masters in `/etc/default/igh-ethercat-diagnostics`:

```bash
# 0, 1, or both:
IGH_DIAG_MASTERS=0,1
```

Use `packaging/igh-ethercat-diagnostics.default` as the template and restart the diagnostic service after changing it.

```bash
sudo cmake --install build
sudo systemctl daemon-reload
sudo systemctl enable igh-ethercat-diagnostics.service
sudo systemctl enable igh-ethercat-diagnostics-web.service
sudo systemctl restart igh-ethercat-diagnostics.service
sudo systemctl restart igh-ethercat-diagnostics-web.service
systemctl status igh-ethercat-diagnostics.service --no-pager
systemctl status igh-ethercat-diagnostics-web.service --no-pager
sudo journalctl -u igh-ethercat-diagnostics.service -f
```

The explicit `restart` commands are intentional: `enable --now` does not reload an already-running process after an upgrade. Restarting ensures the newly installed diagnostic binary and Web service are active.

The default install places binaries in `/usr/local/bin`, Web assets in `/usr/local/share/igh-ethercat-diagnostics/web`, and units in `/usr/local/lib/systemd/system`. The diagnostic service runs as root and writes under `/var/lib/igh-ethercat-diagnostics/logs`; the Web service runs as `nobody:nogroup` and only reads those files.

Open `http://<RK3588-IP>:8080` from a computer on the trusted test LAN. Use `hostname -I` on the board to find its address.

The supplied unit depends on `ethercat.service`. If your IgH service has another unit name, edit [`packaging/systemd/igh-ethercat-diagnostics.service`](packaging/systemd/igh-ethercat-diagnostics.service), then rebuild/install and run `systemctl daemon-reload`.

Common service commands:

```bash
sudo systemctl restart igh-ethercat-diagnostics.service
sudo systemctl restart igh-ethercat-diagnostics-web.service
sudo systemctl stop igh-ethercat-diagnostics.service
sudo systemctl disable igh-ethercat-diagnostics.service
```

## Runtime data

```text
logs/
├── latest_status.json
├── events.jsonl
├── master0/fault_<timestamp_ms>.jsonl
└── master1/fault_<timestamp_ms>.jsonl
```

`latest_status.json` may be read repeatedly. For `events.jsonl`, process one JSON object per line and persist your last consumed offset when building a long-running integration.

## Known boundaries

- Default root-cause weights are a baseline and should be calibrated with real fault samples from each topology.
- The monitor interval and operational defaults are currently compiled into the application.
- The ioctl ABI must match the installed IgH version.
- The Web UI has no built-in login or TLS. Keep port 8080 on a trusted LAN, or place it behind an authenticated HTTPS reverse proxy.
- The Web UI is read-only and does not reset slaves, change AL states, or repair the network automatically.

See [Release Checklist](docs/RELEASE_CHECKLIST.md) before publishing a fork or release.

## License

MIT License. Copyright (c) 2026 Victor-Jiaxin Wang.
