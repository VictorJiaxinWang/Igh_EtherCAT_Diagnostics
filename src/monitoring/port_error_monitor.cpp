#include "ethercat_diag/monitoring/port_error_monitor.h"

#include <utility>

PortErrorMonitor::PortErrorMonitor(int master_index)
    : PortErrorMonitor(master_index, PortErrorReader{})
{
}

PortErrorMonitor::PortErrorMonitor(
    int master_index,
    PortErrorReader reader)
    : PortErrorMonitor(
          master_index,
          [reader = std::move(reader)](
              int current_master,
              int slave_position) {
              return reader.read(
                  current_master,
                  slave_position);
          })
{
}

PortErrorMonitor::PortErrorMonitor(
    int master_index,
    ReadFunction read)
    : master_index_(master_index),
      read_(std::move(read))
{
}

PortErrorMonitorResult PortErrorMonitor::process(
    const NetworkSnapshot& snapshot)
{
    std::vector<const SlaveSnapshot*> online_slaves;

    for (const SlaveSnapshot& slave : snapshot.slaves)
    {
        if (slave.online && slave.position >= 0)
        {
            online_slaves.push_back(&slave);
        }
    }

    if (online_slaves.empty())
    {
        next_online_index_ = 0U;
        return {};
    }

    next_online_index_ %= online_slaves.size();
    const SlaveSnapshot& slave =
        *online_slaves[next_online_index_];
    next_online_index_ =
        (next_online_index_ + 1U) % online_slaves.size();

    PortErrorMonitorResult result;
    result.attempted = true;
    result.slave_position = slave.position;

    if (!read_)
    {
        result.error = "port error read function is not configured";
        return result;
    }

    const PortErrorReadResult read_result =
        read_(master_index_, slave.position);

    if (!read_result.success)
    {
        result.error = read_result.error;
        return result;
    }

    result.events = tracker_.process(
        snapshot.master.timestamp_ms,
        slave.position,
        slave.name,
        read_result.counters);
    return result;
}
