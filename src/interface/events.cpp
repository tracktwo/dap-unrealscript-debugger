
#include "service.h"

namespace unreal_debugger::interface
{

using namespace unreal_debugger::serialization::events;

void debugger_service::show_dll_form()
{
    send_event(events::show_dll_form{});
}

void debugger_service::build_hierarchy()
{
    send_event(events::build_hierarchy{});
}

void debugger_service::clear_hierarchy()
{
    send_event(events::clear_hierarchy{});
}

void debugger_service::add_class_to_hierarchy(const char* class_name)
{
    send_event(events::add_class_to_hierarchy{ class_name });
}

void debugger_service::clear_a_watch(int watch_kind)
{
    // Reset the watch index for this kind. See the comment on
    // add_a_watch for more details of the watch indices.
    watch_indices_[watch_kind] = 1;

    if (!send_watch_info_)
        return;

    if (pending_unlocks_[watch_kind])
    {
        pending_unlocks_[watch_kind]->watch_info_.clear();
    }

    send_event(events::clear_a_watch{ watch_kind });
}

// AddAWatch is special : it's the only entry point from unreal that accepts a return value.
// The return value is used to assign an index to each variable sent to us. This is used to build the
// tree structure of watch variables: when unreal sends an AddAWatch for some child, it will
// set the 'parent' argument to the index we provided for that parent variable. Root variables are
// indicated with a parent of -1.
//
// This debugger interface does not really maintain debugger state. We don't build the variable trees
// here, but rather let the debugger client do it. But we must return a value right away to Unreal,
// we can't just block here until the command is sent to the  client and the client responds with a
// result, as that may take far too long.
//
// The numbers for indices are just arbitrary so we can assign them, and tell the debugger client
// what values we have assigned each particular watch. The client can maintain whatever sort of data
// structure it likes for the watches, it just needs to know that the indices given for 'parent'
// will match the 'assigned_index' variables we send for each watch.
//
// Unreal has three different watch types and 'ClearAWatch' can occur independently for each watch
// kind. So, we maintain three watch indices. On 'ClearAWatch' the appropriate index is reset to 1.
// Each 'AddAWatch' call will assign the current watch index for that watch kind and increment it.
int debugger_service::add_a_watch(int watch_kind, int parent, const char* name, const char* value)
{
    // Assign this variable the next available watch number in the given list.
    int idx = watch_indices_[watch_kind]++;

    if (!send_watch_info_)
        return idx;

    assert(pending_unlocks_[watch_kind]);

    pending_unlocks_[watch_kind]->watch_info_.emplace_back(parent, idx, name, value);
    return idx;
}

void debugger_service::lock_list(int watch_kind)
{
    if (!send_watch_info_)
        return;

    // Create a pending unlock_list message. All watches we receive will be queued up into
    // this message to be sent when we unlock.
    assert(!pending_unlocks_[watch_kind]);
    pending_unlocks_[watch_kind].emplace(watch_kind);

    send_event(events::lock_list{ watch_kind });
}

void debugger_service::unlock_list(int watch_kind)
{
    if (!send_watch_info_)
        return;

    assert(pending_unlocks_[watch_kind]);

    events::unlock_list unlock = std::move(*pending_unlocks_[watch_kind]);
    pending_unlocks_[watch_kind].reset();
    send_event(unlock);
}

void debugger_service::add_breakpoint(const char* class_name, int line_number)
{
    send_event(events::add_breakpoint{ class_name, line_number });
}

void debugger_service::remove_breakpoint(const char* class_name, int line_number)
{
    send_event(events::remove_breakpoint{ class_name, line_number });
}

void debugger_service::editor_load_class(const char* class_name)
{
    send_event(events::editor_load_class{ class_name });
}

void debugger_service::editor_goto_line(int line_number, int highlight)
{
    send_event(events::editor_goto_line{ line_number, static_cast<bool>(highlight) });
}

void debugger_service::add_line_to_log(const char* text)
{
    send_event(events::add_line_to_log{ text });
}

void debugger_service::call_stack_clear()
{
    send_event(events::call_stack_clear{});
}

void debugger_service::call_stack_add(const char* entry)
{
    send_event(events::call_stack_add{ entry });
}

void debugger_service::set_current_object_name(const char* object_name)
{
    send_event(events::set_current_object_name{ object_name });
}

}
