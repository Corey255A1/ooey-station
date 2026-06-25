#include "debug.hpp"

namespace ooey_station::vm {

VMDebugger::VMDebugger(BooeyVM& vm) : vm_(vm) {}

void VMDebugger::add_breakpoint(uint32_t addr) {
    breakpoints_.insert(addr);
}

void VMDebugger::remove_breakpoint(uint32_t addr) {
    breakpoints_.erase(addr);
}

void VMDebugger::clear_breakpoints() {
    breakpoints_.clear();
}

bool VMDebugger::step() {
    if (vm_.is_halted()) return false;
    // Single step is not directly exposed in BooeyVM currently, 
    // but we could run a single iteration. Let's stub this for now.
    return false;
}

bool VMDebugger::continue_execution() {
    if (vm_.is_halted()) return false;
    vm_.run_frame();
    return breakpoints_.count(vm_.get_pc()) > 0;
}

} // namespace ooey_station::vm
