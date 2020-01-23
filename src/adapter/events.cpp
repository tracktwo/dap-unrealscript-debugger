
#include "client.h"
#include "debugger.h"
#include "adapter.h"
#include "signals.h"

namespace unreal_debugger::client
{

namespace serialization = unreal_debugger::serialization;
namespace events = serialization::events;

void show_dll_form(const events::show_dll_form& ev)
{
 
    debugger.finalize_callstack();
    // Tell the debugger we've hit a breakpoint.
    breakpoint_hit();
}

void build_hierarchy(const events::build_hierarchy& ev)
{
}

void clear_hierarchy(const events::clear_hierarchy& ev)
{
}

void add_class_to_hierarchy(const events::add_class_to_hierarchy& ev)
{
}

void clear_a_watch(const events::clear_a_watch& ev)
{
    debugger.clear_watch(static_cast<Debugger::WatchKind>(ev.watch_type_));
}

void lock_list(const events::lock_list& ev)
{
    debugger.lock_list(static_cast<Debugger::WatchKind>(ev.watch_type_));
}

void unlock_list(const events::unlock_list& ev)
{
    for (const events::watch& w : ev.watch_info_)
    {
        debugger.add_watch(static_cast<Debugger::WatchKind>(ev.watch_type_), w.assigned_index_, w.parent_index_, w.name_, w.value_);
    }

    debugger.unlock_list(static_cast<Debugger::WatchKind>(ev.watch_type_));
}

void add_breakpoint(const events::add_breakpoint& ev)
{
}

void remove_breakpoint(const events::remove_breakpoint& ev)
{
}

void editor_load_class(const events::editor_load_class& ev)
{
    debugger.get_current_stack_frame().class_name = ev.class_name_;
}

void editor_goto_line(const events::editor_goto_line& ev)
{
    debugger.get_current_stack_frame().line_number = ev.line_number_;
}

void add_line_to_log(const events::add_line_to_log& ev)
{
    console_message(ev.text_);
}

void call_stack_clear(const events::call_stack_clear& ev)
{
    debugger.clear_callstack();
}

void call_stack_add(const events::call_stack_add& ev)
{
    debugger.add_callstack(ev.entry_);
}

void set_current_object_name(const events::set_current_object_name& ev)
{
    // When changing frames for the purposes of fetching line info for the call stack 'current object name'
    // is the last event we will receive from Unreal, so we can use this to signal that the change is complete.
    // This is because we've disabled watch info for this change.
    if (debugger.get_state() == Debugger::State::waiting_for_frame_line)
    {
        signals::line_received.fire();
    }
}

// FIXME This is a terminated event from the interface and needs to close down the adapter. It should be in
// a better place.
void terminated(const events::terminated& ev)
{
    debugger_terminated();
}

void dispatch_event(const serialization::message& msg)
{
    char* buf = msg.buf_.get();
    events::event_kind k = serialization::deserialize_event_kind(buf);

    switch (k)
    {
    case events::event_kind::show_dll_form: show_dll_form(events::show_dll_form{ msg }); return;
    case events::event_kind::build_hierarchy: build_hierarchy(events::build_hierarchy{ msg }); return;
    case events::event_kind::clear_hierarchy: clear_hierarchy(events::clear_hierarchy{ msg }); return;
    case events::event_kind::add_class_to_hierarchy: add_class_to_hierarchy(events::add_class_to_hierarchy{ msg }); return;
    case events::event_kind::clear_a_watch: clear_a_watch(events::clear_a_watch{ msg }); return;
    case events::event_kind::lock_list: lock_list(events::lock_list{ msg }); return;
    case events::event_kind::unlock_list: unlock_list(events::unlock_list{ msg }); return;
    case events::event_kind::add_breakpoint: add_breakpoint(events::add_breakpoint{ msg }); return;
    case events::event_kind::remove_breakpoint: remove_breakpoint(events::remove_breakpoint{ msg }); return;
    case events::event_kind::editor_load_class: editor_load_class(events::editor_load_class{ msg }); return;
    case events::event_kind::editor_goto_line: editor_goto_line(events::editor_goto_line{ msg }); return;
    case events::event_kind::add_line_to_log: add_line_to_log(events::add_line_to_log{ msg }); return;
    case events::event_kind::call_stack_clear: call_stack_clear(events::call_stack_clear{ msg }); return;
    case events::event_kind::call_stack_add: call_stack_add(events::call_stack_add{ msg }); return;
    case events::event_kind::set_current_object_name: set_current_object_name(events::set_current_object_name{ msg }); return;
    case events::event_kind::terminated: terminated(events::terminated{ msg }); return;
    }

    throw std::runtime_error("Unexpected event type");
}

}
