#include "ethercat_diag/monitoring/monitor.h"

#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

Monitor::ContinuePredicate runOnce()
{
    return [remaining = 1]() mutable {
        if (remaining == 0) {
            return false;
        }

        --remaining;
        return true;
    };
}

void testRunsThreeFixedIntervalCycles()
{
    MonitorConfig config;

    config.master_index = 2;
    config.interval =
        std::chrono::milliseconds(1000);

    int read_count = 0;

    std::vector<int> handled_counts;

    std::vector<Monitor::Clock::time_point>
        deadlines;

    Monitor monitor(
        config,

        [&](int master_index,
            NetworkSnapshot& snapshot)
        {
            assert(master_index == 2);

            ++read_count;

            snapshot.master.slave_count =
                read_count;

            return true;
        },

        [&](const NetworkSnapshot& snapshot)
        {
            handled_counts.push_back(
                snapshot.master.slave_count);
        },

        [&](Monitor::Clock::time_point deadline)
        {
            deadlines.push_back(deadline);
        });

    int remaining_cycles = 3;

    const bool success =
        monitor.run(
            [&]()
            {
                if (remaining_cycles == 0)
                {
                    return false;
                }

                --remaining_cycles;
                return true;
            });

    assert(success);
    assert(read_count == 3);

    assert(handled_counts.size() == 3);
    assert(handled_counts[0] == 1);
    assert(handled_counts[1] == 2);
    assert(handled_counts[2] == 3);

    assert(deadlines.size() == 3);

    const auto first_interval =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                deadlines[1] - deadlines[0]);

    const auto second_interval =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
                deadlines[2] - deadlines[1]);

    assert(first_interval ==
        std::chrono::milliseconds(1000));

    assert(second_interval ==
        std::chrono::milliseconds(1000));
}

void testReadFailureSkipsHandlerAndContinues()
{
    MonitorConfig config;

    config.master_index = 0;
    config.interval =
        std::chrono::milliseconds(1000);

    int read_count = 0;
    int handler_count = 0;
    int wait_count = 0;

    Monitor monitor(
        config,

        [&](int,
            NetworkSnapshot& snapshot)
        {
            ++read_count;

            if (read_count == 2)
            {
                snapshot.master.slave_count = 4;
                return true;
            }

            return false;
        },

        [&](const NetworkSnapshot& snapshot)
        {
            ++handler_count;

            assert(
                snapshot.master.slave_count
                == 4);
        },

        [&](Monitor::Clock::time_point)
        {
            ++wait_count;
        });

    int remaining_cycles = 3;

    const bool success =
        monitor.run(
            [&]()
            {
                if (remaining_cycles == 0)
                {
                    return false;
                }

                --remaining_cycles;
                return true;
            });

    assert(success);

    assert(read_count == 3);
    assert(handler_count == 1);
    assert(wait_count == 3);
}

void testRejectsNegativeMasterIndex()
{
    MonitorConfig config;
    config.master_index = -1;

    int reader_calls = 0;
    int handler_calls = 0;
    int wait_calls = 0;

    Monitor monitor(
        config,
        [&](int, NetworkSnapshot&) {
            ++reader_calls;
            return true;
        },
        [&](const NetworkSnapshot&) {
            ++handler_calls;
        },
        [&](Monitor::Clock::time_point) {
            ++wait_calls;
        });

    const bool success = monitor.run(runOnce());

    assert(!success);
    assert(reader_calls == 0);
    assert(handler_calls == 0);
    assert(wait_calls == 0);
}

void testRejectsNonPositiveInterval()
{
    for (const auto interval :
         {std::chrono::milliseconds(0),
          std::chrono::milliseconds(-100)}) {
        MonitorConfig config;
        config.interval = interval;

        Monitor monitor(
            config,
            [](int, NetworkSnapshot&) {
                return true;
            },
            [](const NetworkSnapshot&) {},
            [](Monitor::Clock::time_point) {});

        const bool success = monitor.run(runOnce());
        assert(!success);
    }
}

void testRejectsMissingCallbacks()
{
    const Monitor::SnapshotReader reader =
        [](int, NetworkSnapshot&) {
            return true;
        };

    const Monitor::SnapshotHandler handler =
        [](const NetworkSnapshot&) {};

    const Monitor::WaitUntil wait_until =
        [](Monitor::Clock::time_point) {};

    const Monitor::ContinuePredicate stop_immediately =
        []() {
            return false;
        };

    MonitorConfig config;

    {
        Monitor monitor(
            config,
            Monitor::SnapshotReader{},
            handler,
            wait_until);

        assert(!monitor.run(stop_immediately));
    }

    {
        Monitor monitor(
            config,
            reader,
            Monitor::SnapshotHandler{},
            wait_until);

        assert(!monitor.run(stop_immediately));
    }

    {
        Monitor monitor(
            config,
            reader,
            handler,
            Monitor::WaitUntil{});

        assert(!monitor.run(stop_immediately));
    }

    {
        Monitor monitor(
            config,
            reader,
            handler,
            wait_until);

        assert(!monitor.run(
            Monitor::ContinuePredicate{}));
    }
}

int main()
{
    testRunsThreeFixedIntervalCycles();
    testReadFailureSkipsHandlerAndContinues();
    testRejectsNegativeMasterIndex();
    testRejectsNonPositiveInterval();
    testRejectsMissingCallbacks();

    std::cout
        << "testRunsThreeFixedIntervalCycles passed\n";

    return 0;
}
