#pragma once

#include "vm.hpp"
#include <unordered_set>

namespace ooey_station::vm {

class VMDebugger {
public:
    VMDebugger(BooeyVM& vm);
    ~VMDebugger() = default;

    void add_breakpoint(uint32_t addr);
    void remove_breakpoint(uint32_t addr);
    void clear_breakpoints();
    
    bool step();
    bool continue_execution();

private:
    BooeyVM& vm_;
    std::unordered_set<uint32_t> breakpoints_;
};

} // namespace ooey_station::vm
