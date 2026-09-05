# Web UI 部署与现场使用

## 安装

```bash
cd igh-ethercat-diagnostics
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
cd build && ctest --output-on-failure && cd ..
sudo cmake --install build
sudo systemctl daemon-reload
sudo systemctl enable igh-ethercat-diagnostics.service
sudo systemctl enable igh-ethercat-diagnostics-web.service
sudo systemctl restart igh-ethercat-diagnostics.service
sudo systemctl restart igh-ethercat-diagnostics-web.service
```

安装结果：

```text
/usr/local/bin/igh-ethercat-diagnostics
/usr/local/bin/igh-ethercat-diagnostics-web
/usr/local/bin/esc-diagnostic-probe
/usr/local/share/igh-ethercat-diagnostics/web/
/usr/local/lib/systemd/system/igh-ethercat-diagnostics.service
/usr/local/lib/systemd/system/igh-ethercat-diagnostics-web.service
```

## 验证服务

```bash
systemctl status igh-ethercat-diagnostics.service --no-pager
systemctl status igh-ethercat-diagnostics-web.service --no-pager
curl --fail http://127.0.0.1:8080/healthz
curl --fail http://127.0.0.1:8080/api/status
ss -ltnp | grep ':8080'
```

页面地址为 `http://<RK3588-IP>:8080`。`hostname -I` 可以查看板卡地址。

## 数据和权限

诊断服务以 root 运行并写入：

```text
/var/lib/igh-ethercat-diagnostics/logs
```

Web 服务以 `nobody:nogroup` 运行，只需要目录遍历和文件读取权限。默认 `StateDirectory`、`std::filesystem::create_directories` 和普通输出文件权限能够满足读取。如果 `/api/status` 返回 503，检查：

```bash
namei -l /var/lib/igh-ethercat-diagnostics/logs/latest_status.json
sudo -u nobody test -r /var/lib/igh-ethercat-diagnostics/logs/latest_status.json
```

不要为了省事把目录改成 `0777`。

## 常见问题

### 板卡 curl 正常，其他电脑打不开

检查 Web 服务是否使用 `--bind 0.0.0.0`，再检查防火墙、电脑与板卡是否在可互通网段：

```bash
sudo journalctl -u igh-ethercat-diagnostics-web.service -n 50 --no-pager
ss -ltn | grep ':8080'
```

### 页面显示“诊断数据已经停止更新”

Web 服务仍在，但 `latest_status.json` 的时间戳连续 5 秒没有变化。检查诊断主服务：

```bash
systemctl status igh-ethercat-diagnostics.service --no-pager
sudo journalctl -u igh-ethercat-diagnostics.service -n 50 --no-pager
```

### 页面一直显示 503

确认主服务已生成文件，并确认低权限 Web 用户可读。主服务刚启动时短暂 503 属于正常现象。

### 端口已被占用

```bash
sudo ss -ltnp | grep ':8080'
```

可以修改 Web unit 中的 `--port`，执行 `sudo systemctl daemon-reload` 后重启服务。浏览器地址也要使用新端口。

## 现场恢复流程

1. 截图保存当前页面，记录红色断点和诊断结论。
2. 只处理页面指出的链路或从站，避免同时改变多个条件。
3. 重新连接线缆或恢复供电后等待至少两个采样周期。
4. 确认顶部变绿、Slave 数量恢复、红色断点消失。
5. 确认最近事件出现恢复记录。
6. 如果错误重复发生，保存 `events.jsonl` 和 `fault_*.jsonl` 交给工程师。

## 网络安全

8080 只应开放给受信任测试网段。公网或跨厂区访问必须使用反向代理增加 HTTPS、认证和访问控制。本服务没有远程控制接口，但状态、设备名称和故障拓扑仍属于需要保护的设备信息。
