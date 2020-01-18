
#include "service.h"

using namespace unreal_debugger::events;

void DebuggerService::show_dll_form()
{
    Event ev;
    ev.set_kind(Event_Kind_ShowDllForm);
    ev.mutable_show_dll_form();
    send_event(ev);
}

void DebuggerService::build_hierarchy()
{
    Event ev;
    ev.set_kind(Event_Kind_BuildHierarchy);
    ev.mutable_build_hierarchy();
    send_event(ev);
}

void DebuggerService::clear_hierarchy()
{
    Event ev;
    ev.set_kind(Event_Kind_ClearHierarchy);
    ev.mutable_clear_hierarchy();
    send_event(ev);
}

void DebuggerService::add_class_to_hierarchy(const char* class_name)
{
    Event ev;
    ev.set_kind(Event_Kind_AddClassToHierarchy);
    ev.mutable_add_class_to_hierarchy()->set_class_name(class_name);
    send_event(ev);
}

void DebuggerService::clear_a_watch(int watch_kind)
{
    // Reset the watch index for this kind. See the comment on
    // add_a_watch for more details of the watch indices.
    watch_indices_[watch_kind] = 1;

    if (!send_watch_info_)
        return;

    if (pending_unlocks_[watch_kind])
    {
        pending_unlocks_[watch_kind]->clear_watch_info();
    }

    Event ev;
    ev.set_kind(Event_Kind_ClearAWatch);
    ev.mutable_clear_a_watch()->set_watch_type(watch_kind);
    send_event(ev);
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
int DebuggerService::add_a_watch(int watch_kind, int parent, const char* name, const char* value)
{
    // Assign this variable the next available watch number in the given list.
    int idx = watch_indices_[watch_kind]++;

    if (!send_watch_info_)
        return idx;

    assert(pending_unlocks_[watch_kind]);

    AddAWatch *new_watch = pending_unlocks_[watch_kind]->add_watch_info();
    new_watch->set_watch_type(watch_kind);
    new_watch->set_parent_index(parent);
    new_watch->set_name(name);
    new_watch->set_value(value);
    new_watch->set_assigned_index(idx);

    return idx;
}

void DebuggerService::lock_list(int watch_kind)
{
    if (!send_watch_info_)
        return;

    // Create a pending unlock_list message. All watches we receive will be queued up into
    // this message to be sent when we unlock.
    assert(!pending_unlocks_[watch_kind]);
    unreal_debugger::events::UnlockList unlock;
    unlock.set_watch_type(watch_kind);
    pending_unlocks_[watch_kind] = std::move(unlock);

    Event ev;
    ev.set_kind(Event_Kind_LockList);
    ev.mutable_lock_list()->set_watch_type(watch_kind);
    send_event(ev);
}

void DebuggerService::unlock_list(int watch_kind)
{
    if (!send_watch_info_)
        return;

    assert(pending_unlocks_[watch_kind]);

    Event ev;
    ev.set_kind(Event_Kind_UnlockList);
    *ev.mutable_unlock_list() = *pending_unlocks_[watch_kind];
    pending_unlocks_[watch_kind] = {};
    send_event(ev);
}

void DebuggerService::add_breakpoint(const char* class_name, int line_number)
{
    Event ev;
    ev.set_kind(Event_Kind_AddBreakpoint);
    AddBreakpoint* payload = ev.mutable_add_breakpoint();
    payload->set_class_name(class_name);
    payload->set_line_number(line_number);
    send_event(ev);
}

void DebuggerService::remove_breakpoint(const char* class_name, int line_number)
{
    Event ev;
    ev.set_kind(Event_Kind_RemoveBreakpoint);
    RemoveBreakpoint* payload = ev.mutable_remove_breakpoint();
    payload->set_class_name(class_name);
    payload->set_line_number(line_number);
    send_event(ev);
}

void DebuggerService::editor_load_class(const char* class_name)
{
    Event ev;
    ev.set_kind(Event_Kind_EditorLoadClass);
    ev.mutable_editor_load_class()->set_class_name(class_name);
    send_event(ev);
}

void DebuggerService::editor_goto_line(int line_number, int highlight)
{
    Event ev;
    ev.set_kind(Event_Kind_EditorGotoLine);
    EditorGotoLine* payload = ev.mutable_editor_goto_line();
    payload->set_line_number(line_number);
    payload->set_highlight(highlight);
    send_event(ev);
}

void DebuggerService::add_line_to_log(const char* text)
{
    Event ev;
    ev.set_kind(Event_Kind_AddLineToLog);
    ev.mutable_add_line_to_log()->set_text(text);
    send_event(ev);
}

void DebuggerService::call_stack_clear()
{
    // Reset the tracking for our call stack size.
    call_stack_size_ = 0;
    Event ev;
    ev.set_kind(Event_Kind_CallStackClear);
    ev.mutable_call_stack_clear();
    send_event(ev);
}

void DebuggerService::call_stack_add(const char* entry)
{
    // Before we can send this event we need to ask unreal to switch the frame so we can get the line number.
    Event ev;
    ev.set_kind(Event_Kind_CallStackAdd);
    ev.mutable_call_stack_add()->set_entry(entry);
    send_event(ev);
}

void DebuggerService::set_current_object_name(const char* object_name)
{
    Event ev;
    ev.set_kind(Event_Kind_SetCurrentObjectName);
    ev.mutable_set_current_object_name()->set_object_name(object_name);
    send_event(ev);
}
