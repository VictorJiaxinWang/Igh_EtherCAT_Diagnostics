#include "ethercat_diag/recording/blackbox.h"
#include <stdexcept>
#include <fstream>


namespace
{

const char* eventTypeToString(EventType type)
{
    switch (type)
    {
    case EventType::MASTER_LINK_DOWN:
        return "MASTER_LINK_DOWN";

    case EventType::MASTER_LINK_UP:
        return "MASTER_LINK_UP";

    case EventType::SLAVE_COUNT_CHANGED:
        return "SLAVE_COUNT_CHANGED";

    case EventType::SLAVE_LOST:
        return "SLAVE_LOST";

    case EventType::SLAVE_STATE_CHANGED:
        return "SLAVE_STATE_CHANGED";

    case EventType::PORT_INVALID_FRAME_INCREASED:
        return "PORT_INVALID_FRAME_INCREASED";

    case EventType::PORT_RX_ERROR_INCREASED:
        return "PORT_RX_ERROR_INCREASED";

    case EventType::PORT_FORWARDED_RX_ERROR_INCREASED:
        return "PORT_FORWARDED_RX_ERROR_INCREASED";

    case EventType::PORT_LOST_LINK_INCREASED:
        return "PORT_LOST_LINK_INCREASED";
    }

    return "UNKNOWN_EVENT";
}

std::string escapeJsonString(
    const std::string& input)
{
    static constexpr char hex_digits[] =
        "0123456789abcdef";

    std::string escaped;

    for (const unsigned char character : input)
    {
        switch (character)
        {
        case '"':
            escaped += "\\\"";
            break;

        case '\\':
            escaped += "\\\\";
            break;

        case '\b':
            escaped += "\\b";
            break;

        case '\f':
            escaped += "\\f";
            break;

        case '\n':
            escaped += "\\n";
            break;

        case '\r':
            escaped += "\\r";
            break;

        case '\t':
            escaped += "\\t";
            break;

        default:
            if (character < 0x20)
            {
                escaped += "\\u00";
                escaped +=
                    hex_digits[
                        (character >> 4) & 0x0f];
                escaped +=
                    hex_digits[
                        character & 0x0f];
            }
            else
            {
                escaped +=
                    static_cast<char>(character);
            }

            break;
        }
    }

    return escaped;
}

const char* alStateToString(AlState state)
{
    switch (state)
    {
    case AlState::INIT:
        return "INIT";

    case AlState::PREOP:
        return "PREOP";

    case AlState::SAFEOP:
        return "SAFEOP";

    case AlState::OP:
        return "OP";

    case AlState::UNKNOWN:
        return "UNKNOWN";
    }

    return "UNKNOWN";
}

}

Blackbox::Blackbox(
    std::size_t capacity,
    std::size_t post_fault_capacity)
    : capacity_(capacity),
      post_fault_capacity_(post_fault_capacity)
{
    if (capacity_ == 0)
    {
        throw std::invalid_argument(
            "Blackbox capacity must be greater than zero");
    }

    if (post_fault_capacity_ == 0)
    {
        throw std::invalid_argument(
            "Post-fault capacity must be greater than zero");
    }
}

void Blackbox::push(
    const NetworkSnapshot& snapshot)
{
    if (state_ == BlackboxState::SAVING)
    {
        return;
    }

    buffer_.push_back(snapshot);

    if (state_ == BlackboxState::RECORDING)
    {
        if (buffer_.size() > capacity_)
        {
            buffer_.pop_front();
        }

        return;
    }

    ++post_fault_recorded_;

    if (post_fault_recorded_ >=
        post_fault_capacity_)
    {
        state_ = BlackboxState::SAVING;
    }
}

std::size_t Blackbox::size() const
{
    return buffer_.size();
}

const std::deque<NetworkSnapshot>&
Blackbox::snapshots() const
{
    return buffer_;
}

void Blackbox::trigger(
    const FaultEvent& event)
{
    if (state_ != BlackboxState::RECORDING)
    {
        return;
    }

    trigger_event_ = event;

    state_ =
        BlackboxState::POST_FAULT_RECORDING;
}

BlackboxState Blackbox::state() const
{
    return state_;
}

const std::optional<FaultEvent>&
Blackbox::triggerEvent() const
{
    return trigger_event_;
}

bool Blackbox::saveToFile(
    const std::string& file_path) const
{
    if (state_ != BlackboxState::SAVING)
    {
        return false;
    }

    std::ofstream output(file_path);

    if (!output.is_open())
    {
        return false;
    }

    for (const NetworkSnapshot& snapshot :
     buffer_)
    {
        const MasterSnapshot& master =
            snapshot.master;

        output
            << "{\"record\":\"snapshot\","
            << "\"timestamp_ms\":"
            << master.timestamp_ms
            << ",\"master_index\":"
            << master.master_index
            << ",\"phase\":\""
            << escapeJsonString(master.phase)
            << "\",\"active\":"
            << (master.active ? "true" : "false")
            << ",\"link_up\":"
            << (master.link_up ? "true" : "false")
            << ",\"slave_count\":"
            << master.slave_count
            << ",\"slaves\":[";

        bool first_slave = true;

        for (const SlaveSnapshot& slave :
            snapshot.slaves)
        {
            if (!first_slave)
            {
                output << ',';
            }

            first_slave = false;

            output
                << "{\"position\":"
                << slave.position
                << ",\"alias\":"
                << slave.alias
                << ",\"relative_position\":"
                << slave.relative_position
                << ",\"state\":\""
                << alStateToString(slave.state)
                << "\",\"has_error\":"
                << (slave.has_error ?
                    "true" : "false")
                << ",\"online\":"
                << (slave.online ?
                    "true" : "false")
                << ",\"name\":\""
                << escapeJsonString(slave.name)
                << "\"}";
        }

        output << "]}\n";
    }

    // output.flush();

    if (trigger_event_)
    {
        const FaultEvent& event =
            *trigger_event_;

        output
            << "{\"record\":\"event\","
            << "\"timestamp_ms\":"
            << event.timestamp_ms
            << ",\"type\":\""
            << eventTypeToString(event.type)
            << "\",\"slave_position\":"
            << event.slave_position
            ;

        if (event.master_index >= 0)
        {
            output << ",\"master_index\":" << event.master_index;
        }
        if (event.slave_alias > 0)
        {
            output << ",\"slave_alias\":" << event.slave_alias
                   << ",\"slave_relative_position\":"
                   << event.slave_relative_position;
        }

        if (event.port_position >= 0)
        {
            output
                << ",\"port_position\":"
                << event.port_position;
        }

        output
            << ",\"old_value\":"
            << event.old_value
            << ",\"new_value\":"
            << event.new_value
            << ",\"description\":\""
            << escapeJsonString(event.description)
            << "\"}\n";
    }

    output.close();

    return !output.fail();
}
