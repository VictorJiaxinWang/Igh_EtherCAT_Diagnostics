# Web 数据文件约定

## latest_status.json

必须是单个合法 JSON 对象，UTF-8 编码，以换行结束。

| 字段 | 类型 | 含义 |
| --- | --- | --- |
| `updated_ms` | integer | 最近快照时间戳 |
| `status` | string | HEALTHY、DEGRADED 或 FAULT |
| `master_index` | integer | IgH Master 编号 |
| `phase` | string | Master phase |
| `active` | boolean | Master active 状态 |
| `link_up` | boolean | 主站物理链路 |
| `slave_count` | integer | 当前响应从站数量 |
| `last_fault_timestamp_ms` | integer/null | 最近故障时间 |
| `slaves` | array | 当前从站列表 |
| `root_cause` | object/null | 当前活动根因 |

消费者必须允许以后增加字段，不应依赖 JSON 字段顺序。

## events.jsonl

每行一个独立 JSON 对象。`record` 当前可能为：

```text
event
root_cause
recovery
```

读取到最后一行尚未完整时，消费者可以稍后重试，但 Publisher 每次写入都会生成完整一行。

## 时间戳

当前使用板卡系统时钟毫秒值。没有 RTC 电池的设备重启后时间可能倒退，因此消费者不应仅凭跨重启时间戳推导持续时长。单次故障的 `duration_ms` 由进程内相对差值产生。
