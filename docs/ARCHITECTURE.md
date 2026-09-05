# Software architecture

[简体中文](ARCHITECTURE.zh-CN.md) | English

IgH EtherCAT Diagnostics v3.0.0 separates acquisition, detection, diagnosis, persistence, and presentation. The diagnostic daemon observes an existing IgH Master through its character device; it does not join the PDO real-time loop or call `ecrt_request_master()`.

```text
/dev/EtherCAT0
      |
      v
IgH ioctl backend ----> NetworkSnapshot ----> Monitor (1 Hz)
                                                |
                  +-----------------------------+------------------+
                  |                             |                  |
                  v                             v                  v
            EventDetector                rolling history    port-error deltas
                  |                             |                  |
                  +----------> DiagnosisCoordinator <-------------+
                                                |
                            +-------------------+------------------+
                            |                   |                  |
                            v                   v                  v
                    BoundaryLocator      ActiveDiagnosis    RootCauseAnalyzer
                                                |
                    +---------------------------+-------------------------+
                    |                           |                         |
                    v                           v                         v
              fault JSONL             latest_status.json           events.jsonl
                                           atomic                    append-only
                                                |
                                                v
                              igh-ethercat-diagnostics-web
                                                |
                                                v
                                              browser
```

## Runtime data flow

1. `IoctlSnapshotReader` and `IghMasterDevice` read Master and Slave state directly from `/dev/EtherCATN`. The production monitoring path does not launch a shell process.
2. `Monitor` produces one `NetworkSnapshot` per second. Downstream modules depend on this typed snapshot rather than the acquisition mechanism.
3. `EventDetector`, `RecoveryTracker`, and `PortErrorTracker` compare snapshots and emit topology, AL-state, recovery, and counter-delta events.
4. The rolling black box retains pre-fault history. A fault starts post-trigger capture and produces a JSONL evidence file.
5. `DiagnosisCoordinator` groups a fault episode, locates its likely boundary, performs controlled ESC reads, and applies Burst/Cooldown limits.
6. `RootCauseAnalyzer` ranks candidate causes from topology, ESC, and port-counter evidence. Reports include confidence, supporting and contradicting evidence, and recommended actions.
7. `WebDataPublisher` atomically replaces `latest_status.json` and appends significant events to `events.jsonl`.
8. The separate Web process reads those files and serves a read-only HTTP API and dashboard. It never opens the EtherCAT device.

## Module responsibilities

| Directory | Responsibility |
| --- | --- |
| `common` | Shared snapshots, events, and result data types |
| `infrastructure` | IgH device access and process/runtime utilities |
| `monitoring` | ioctl snapshots, monitoring loop, and port counters |
| `detection` | Change, boundary, recovery, and error-delta detection |
| `esc` | ESC register access, decoding, and formatting |
| `diagnosis` | Active diagnosis and fault-episode coordination |
| `root_cause` | Evidence windows, scoring, confidence, and explanations |
| `recording` | Rolling history and JSONL black-box persistence |
| `publishing` | Stable status and event files for external consumers |
| `web` | Read-only data store, HTTP routing, and socket server |
| `apps` | Composition roots for the daemon, Web service, and tools |
| `packaging/systemd` | Linux service units for production deployment |

## Isolation and failure handling

- The monitoring daemon and Web server are separate processes. Browser traffic cannot block EtherCAT collection.
- Status publication uses write-to-temporary-file plus `rename`, so readers do not observe partial JSON.
- Event history is JSONL and append-only; each line is independently parseable.
- Collection and publication errors are reported without replacing the last valid snapshot.
- ABI validation runs before interpreting ioctl structures. An incompatible IgH ABI is treated as a hard acquisition error.
- Active register reads are bounded by Burst and Cooldown controls to avoid repeated diagnostic traffic during one fault.

## Security boundary

The diagnostic service needs access to `/dev/EtherCATN` and runs as root in the supplied unit. The Web service runs as `nobody:nogroup`, receives no device access, and is hardened with systemd restrictions. The built-in HTTP server has no authentication or TLS, so it must remain on a trusted network or sit behind an authenticated HTTPS reverse proxy.

The Web UI is read-only. It does not reset slaves, change AL states, restart the Master, or repair the network automatically.
