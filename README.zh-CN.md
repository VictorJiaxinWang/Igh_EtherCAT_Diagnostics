# IgH EtherCAT Diagnostics

简体中文 | [English](README.md)

这是一个面向 IgH EtherCAT Master 系统的独立 EtherCAT 监控与故障诊断工具。

版本：**v3.1.0**  
作者：**Victor-Jiaxin Wang**  
许可证：**MIT**

## 项目作用

诊断进程运行在现有 EtherCAT 应用旁边，不进入 PDO 实时循环，也不请求占用 Master。它以 1 Hz 采集网络状态，检测拓扑和 AL 状态变化，保存故障前后的快照，定位可能的断点，读取 ESC 诊断寄存器，跟踪端口错误增量，识别网络恢复，并根据多种证据给出带置信度和解释信息的根因判断。

程序提供两个稳定文件，并通过面向测试人员的 Web UI 展示：

- `logs/latest_status.json`：当前状态，通过原子替换更新。
- `logs/events.jsonl`：故障、恢复和根因事件，只追加写入。

独立的 `igh-ethercat-diagnostics-web` 在 8080 端口提供只读页面。测试人员可以直接看到网络状态、带红色断点的从站拓扑、通俗根因、置信度、判断依据、恢复步骤和最近事件。Web 流量不会阻塞 EtherCAT 采集。

## 适用情况

- 调试和维护使用 IgH EtherCAT Master 的设备。
- 捕获偶发的网线、接头、从站供电、下游掉站和 AL 状态故障。
- 不修改运动控制实时程序，为现有系统增加旁路诊断。

本项目是诊断辅助工具，不是安全功能，也不会自动修复网络。

## 主要功能

- 生产采集路径直接使用 IgH ioctl，不启动 shell 子进程。
- 统一 `NetworkSnapshot` 数据模型，采集后端可替换、可注入测试。
- 检测掉站、状态变化、链路变化、恢复和端口错误事件。
- 环形历史与 JSONL 黑匣子，保存故障前后证据。
- 使用 Burst 和 Cooldown 控制故障后的 ESC 主动读取。
- 定位故障边界，融合证据并输出可解释的根因排序。
- 原子更新当前状态，追加记录历史事件。
- 零第三方依赖的 C++ HTTP 服务和响应式中文诊断页面。
- 支持 systemd、`SIGINT`/`SIGTERM` 优雅退出和自动化回归测试。
- 可配置监测 Master 0、Master 1 或同时监测二者，各 Master 的诊断状态完全隔离。
- 以 EEPROM Alias（`alias:relative_position`）识别从站；Position 只用于当前 ioctl 寻址，`rescan` 后不会误判身份。

## 软件结构

```text
/dev/EtherCAT0,1 -> ioctl 后端 -> 每个 Master 独立的 NetworkSnapshot -> 1 Hz Monitor
                                            |
       +--------------------+---------------+------------------+
       |                    |                                  |
   事件/恢复检测         环形历史                        端口错误增量
       |                    |                                  |
       +------------ DiagnosisCoordinator --------------------+
                             |
                  边界 + ESC 主动读取 + 根因
                             |
                 黑匣子 + 当前状态 + 事件流
                             |
                             v
                    C++ Web 服务 -> 浏览器
```

完整模块关系见[软件架构](docs/ARCHITECTURE.zh-CN.md)，Web 字段契约见 [Web 数据格式](docs/WEB_DATA_SCHEMA.md)。

## 依赖环境

- Linux 和可以正常工作的 IgH EtherCAT Master。
- 能够读取 IgH 设备节点，通常是 `/dev/EtherCAT0`。
- 支持 C++17 的编译器、CMake 3.18 或更高版本、Make 或 Ninja。
- 工程自带的 IgH 1.6.3 ABI 头文件必须与板卡正在运行的 Master 匹配。如果版本不同，应替换头文件并重新完成全部测试。

### 已验证参考环境

| 项目 | 环境 |
| --- | --- |
| 开发板 | Rockchip RK3588 TOYBRICK X10 Board |
| 系统 | Debian GNU/Linux 11 (bullseye) |
| 架构 | AArch64 (`aarch64`) |
| 内核 | Linux 5.10.161 |
| 编译器 | GCC/G++ 10.2.1 |
| CMake | 3.18.4 |
| IgH Master | 1.6.3，运行时 ioctl magic 32 |
| 内核模块 | `ec_master`、`ec_generic` |
| 设备节点 | `/dev/EtherCAT0`，组 `ethercat`，权限 `0660` |

这是已经验证的参考组合，不是平台限制。其他 Linux 板卡只要架构、IgH ABI 和设备权限正确，也可以移植。

## 编译与测试

```bash
git clone <你的仓库地址>
cd igh-ethercat-diagnostics

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
cd build && ctest --output-on-failure
cd ..
```

生成的主要程序：

- `build/igh-ethercat-diagnostics`：持续运行的诊断守护进程。
- `build/igh-ethercat-diagnostics-web`：只读 HTTP API 与诊断页面服务。
- `build/esc-diagnostic-probe`：单次 ESC 寄存器探针。
- `build/root_cause_matrix_demo`：根因规则标定矩阵。
- `build/snapshot_backend_compare`：ioctl 与旧 shell 后端迁移对比工具。

单元测试使用注入的假数据，通常不要求 EtherCAT 硬件在线；真实探针需要 Master 与设备节点。

## 直接运行

先检查 IgH 服务和权限：

```bash
systemctl status ethercat --no-pager
ls -l /dev/EtherCAT0
ethercat master -m 0
ethercat slaves -m 0
```

程序会在当前工作目录下面创建 `logs`，因此应从可写目录启动：

```bash
mkdir -p "$HOME/igh-ethercat-diagnostics-runtime"
cd "$HOME/igh-ethercat-diagnostics-runtime"
/你的仓库路径/build/igh-ethercat-diagnostics --masters 0,1
```

如果当前用户无法打开 `/dev/EtherCAT0`，可把用户加入设备所属组并重新登录，或临时使用 root 运行探针。不要把设备节点改成所有用户可写。

```bash
sudo usermod -aG ethercat "$USER"
# 退出 SSH 并重新连接，然后用 id 检查组是否生效

/你的仓库路径/build/esc-diagnostic-probe 0 3
```

## 安装和 systemd 部署

安装后在 `/etc/default/igh-ethercat-diagnostics` 选择监测范围：

```bash
# 仅 Master 0：IGH_DIAG_MASTERS=0
# 仅 Master 1：IGH_DIAG_MASTERS=1
# 同时监测：
IGH_DIAG_MASTERS=0,1
```

可复制工程中的 `packaging/igh-ethercat-diagnostics.default` 作为配置模板；修改配置后重启诊断服务。

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

这里显式执行 `restart` 是为了保证升级生效：`enable --now` 不会重新加载已经运行的旧进程。重启后，新安装的诊断程序和 Web 服务才会真正投入运行。

默认把程序安装到 `/usr/local/bin`，页面资源安装到 `/usr/local/share/igh-ethercat-diagnostics/web`，服务文件安装到 `/usr/local/lib/systemd/system`。诊断服务以 root 写入 `/var/lib/igh-ethercat-diagnostics/logs`，Web 服务以 `nobody:nogroup` 只读访问。

在受信任测试局域网的电脑打开 `http://<RK3588-IP>:8080`。板卡运行 `hostname -I` 可以查看地址。

随工程提供的服务依赖 `ethercat.service`。如果板卡上的 IgH 服务名称不同，请修改 [`packaging/systemd/igh-ethercat-diagnostics.service`](packaging/systemd/igh-ethercat-diagnostics.service)，重新编译安装，再执行 `systemctl daemon-reload`。

常用管理命令：

```bash
sudo systemctl restart igh-ethercat-diagnostics.service
sudo systemctl restart igh-ethercat-diagnostics-web.service
sudo systemctl stop igh-ethercat-diagnostics.service
sudo systemctl disable igh-ethercat-diagnostics.service
```

## 运行数据

```text
logs/
├── latest_status.json
├── events.jsonl
├── master0/fault_<timestamp_ms>.jsonl
└── master1/fault_<timestamp_ms>.jsonl
```

`latest_status.json` 可以反复读取。持续消费 `events.jsonl` 时，应按“一行一个 JSON 对象”解析，并保存上次读到的文件偏移量。

## 当前边界

- 默认根因权重是基线值，不同拓扑和从站应使用真实故障样本继续标定。
- 监控周期和部分运行参数目前在程序中定义。
- ioctl ABI 必须与板卡安装的 IgH 版本一致。
- Web UI 不内置登录和 HTTPS。8080 端口只应开放在受信任局域网；跨不可信网络访问时应使用带认证和 HTTPS 的反向代理。
- 页面只读，不会复位从站、切换 AL 状态或自动修复网络。

发布自己的仓库或版本前，请检查[发布清单](docs/RELEASE_CHECKLIST.md)。

## 许可证

MIT License。Copyright (c) 2026 Victor-Jiaxin Wang。
