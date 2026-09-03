#pragma once

#include "ethercat_diag/common/diag_types.h"

#include <cstddef>
#include <deque>
#include <optional>
#include <string>

enum class BlackboxState
{
    RECORDING,
    POST_FAULT_RECORDING,
    SAVING
};

class Blackbox
{
public:
    // explicit Blackbox(std::size_t capacity);
    Blackbox(std::size_t capacity, std::size_t post_fault_capacity);

    void push(const NetworkSnapshot& snapshot);

    std::size_t size() const;

    const std::deque<NetworkSnapshot>&
    snapshots() const;

    void trigger(const FaultEvent& event);

    BlackboxState state() const;

    const std::optional<FaultEvent>& triggerEvent() const;

    bool saveToFile(const std::string& file_path) const;

private:
    std::size_t capacity_;
    std::deque<NetworkSnapshot> buffer_;

    BlackboxState state_{
    	BlackboxState::RECORDING
    };
    
    std::optional<FaultEvent> trigger_event_;

    std::size_t post_fault_capacity_;
    std::size_t post_fault_recorded_{0};
};
