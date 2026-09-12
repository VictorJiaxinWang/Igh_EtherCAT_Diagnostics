# 软件架构

简体中文 | [English](ARCHITECTURE.md)

IgH EtherCAT Diagnostics v3.1.0 按“采集—检测—诊断—持久化—展示”划分职责。诊断守护进程通过字符设备观察现有 IgH Master，不进入 PDO 实时循环，也不调用 `ecrt_request_master()`。

```text
/dev/EtherCAT0
      |
      v
IgH ioctl 后端 ----> NetworkSnapshot ----> Monitor（1 Hz）
                                             |
                +----------------------------+------------------+
                |                            |                  |
                v                            v                  v
          EventDetector                  环形历史          端口错误增量
                |                            |                  |
                +---------> DiagnosisCoordinator <-------------+
                                             |
                          +------------------+------------------+
                          |                  |                  |
                          v                  v                  v
                  BoundaryLocator     ActiveDiagnosis    RootCauseAnalyzer
                                             |
                  +--------------------------+--------------------------+
                  |                          |                          |
                  v                          v                          v
              故障 JSONL             latest_status.json          events.jsonl
                                           原子替换                  只追加
                                             |
                                             v
                           igh-ethercat-diagnostics-web
                                             |
                                             v
                                           浏览器
```

## 运行时数据流

1. `IoctlSnapshotReader` 和 `IghMasterDevice` 直接从配置的 `/dev/EtherCATN` 读取状态。每个 Master 拥有独立的检测、历史、恢复和根因上下文。EEPROM Alias 与 relative position 构成稳定身份，Position 只用于当前寄存器访问。
2. `Monitor` 每秒生成一个 `NetworkSnapshot`。后续模块只依赖这个强类型快照，不依赖具体采集方式。
3. `EventDetector`、`RecoveryTracker` 和 `PortErrorTracker` 比较快照，生成拓扑、AL 状态、恢复和计数器增量事件。
4. 环形黑匣子保留故障前历史。故障触发后继续采集，并生成 JSONL 证据文件。
5. `DiagnosisCoordinator` 管理一次故障过程，定位疑似边界，执行受控 ESC 读取，并实施 Burst 和 Cooldown 限制。
6. `RootCauseAnalyzer` 融合拓扑、ESC 和端口计数器证据，对候选根因排序。报告包含置信度、支持证据、矛盾证据和恢复建议。
7. `WebDataPublisher` 原子替换 `latest_status.json`，并把重要事件追加到 `events.jsonl`。
8. 独立 Web 进程读取这些文件并提供只读 HTTP API 和页面，不访问 EtherCAT 设备。

## 模块职责

| 目录 | 职责 |
| --- | --- |
| `common` | 公共快照、事件和诊断结果数据结构 |
| `infrastructure` | IgH 设备访问与进程运行基础设施 |
| `monitoring` | ioctl 快照、监控循环与端口计数器 |
| `detection` | 状态变化、边界、恢复和错误增量检测 |
| `esc` | ESC 寄存器读取、解码和格式化 |
| `diagnosis` | 主动诊断与故障过程协调 |
| `root_cause` | 证据窗口、评分、置信度和解释 |
| `recording` | 环形历史与 JSONL 黑匣子持久化 |
| `publishing` | 面向外部消费者的状态和事件发布 |
| `web` | 只读数据访问、HTTP 路由与 socket 服务 |
| `apps` | 守护进程、Web 服务和工具的组合入口 |
| `packaging/systemd` | 生产部署使用的 Linux 服务单元 |

## 隔离与失败处理

- 诊断守护进程和 Web 服务是两个独立进程，浏览器流量不会阻塞 EtherCAT 采集。
- 当前状态先写临时文件，再通过 `rename` 原子替换，读取方不会看到不完整 JSON。
- 历史事件采用只追加 JSONL，每一行都可以独立解析。
- 采集或发布失败会记录错误，但不会用无效数据覆盖上一份有效快照。
- 解释 ioctl 数据结构前先检查 ABI，不兼容的 IgH ABI 会作为严重采集错误处理。
- 主动寄存器读取受到 Burst 和 Cooldown 限制，避免一次故障产生重复诊断流量。

## 安全边界

诊断服务需要访问 `/dev/EtherCATN`，随项目提供的 unit 使用 root 运行。Web 服务以 `nobody:nogroup` 运行，不能访问 EtherCAT 设备，并启用了 systemd 安全限制。内置 HTTP 服务没有认证和 TLS，只能部署在受信任网络，或者放在带认证和 HTTPS 的反向代理后面。

Web UI 是只读工具，不会复位从站、切换 AL 状态、重启 Master 或自动修复网络。
