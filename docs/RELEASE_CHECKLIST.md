# GitHub 发布清单

## 发布前检查

- 确认 README 中的版本、功能和限制与代码一致。
- 在目标架构上完成 Release 构建和全部 CTest。
- 确认仓库中没有 `build`、`Testing`、`logs`、ELF 可执行文件或现场数据。
- 确认没有密码、私钥、设备序列号和内部网络信息。
- 用真实板卡完成一次启动、拔线、恢复和停止测试。
- 在另一台局域网电脑打开页面，检查正常、故障、恢复和数据过期四种状态。
- 对照板卡 IgH 版本检查 `third_party/igh-ethercat-1.6.3` ABI 头文件。

## 首次推送

在 GitHub 网站创建空仓库后，在板卡运行：

```bash
cd /path/to/igh-ethercat-diagnostics
git init
git add .
git commit -m "Release v3.1.0"
git branch -M main
git remote add origin <你的 GitHub 仓库地址>
git push -u origin main
```

建议仓库名使用 `igh-ethercat-diagnostics`，发布标签使用 `v3.1.0`。

## 后续版本

```bash
git tag -a v3.1.0 -m "IgH EtherCAT Diagnostics v3.1.0"
git push origin v3.1.0
```

以后每次发布先更新 `CHANGELOG.md`、README 版本和测试结果，再创建新标签。
