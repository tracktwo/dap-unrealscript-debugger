
// debuggerinterface.cpp
//
// This implements the debugger interface API specified by the UDK docs:
// https://docs.unrealengine.com/udk/Three/DebuggerInterface.html
//
// This interface is compiled into a DLL that is controlled by Unreal itself.
// It contains very little actual debug logic, this interface spins up a small
// TCP server that accepts incoming connections from the debugger itself and just
// acts as an intermediary between Unreal and that external debugger.
//
// Calls from Unreal into the debugger interface entry points are serialized into
// "events" and sent over the network to the debugger. Communication from the
// debugger to Unreal are sent over the network to this interface and deserialized
// as "commands" to send to Unreal through the unreal supplied callback function.
//
// With the exception of watches, commands and events are passed through between the
// debugger and Unreal effectively without any inspection: this layer does nothing
// but manage the connections and ferry data back and forth between Unreal and the
// debugger.

#include <stdio.h>
#include "service.h"

// The callback function provided by unreal.
UnrealCallback callback_function = nullptr;

// Unreal's debugging API doesn't have an explicit 'stop' command. See AddLineToLog for more details
// on the use of this special string.
static const char* magic_debugger_stopped_log_entry = "Log: Detaching UnrealScript Debugger (currently detached)";

extern "C"
{
    // Set the callback function. This is called by Unreal when the debugger starts.
    __declspec(dllexport) void SetCallback(void* cb)
    {
        callback_function = static_cast<UnrealCallback>(cb);
    }

    // ShowDllForm is called at multiple points by unreal, but has no parameters to tell us why.
    // It seems to be invoked during the very first startup (after clearing watches and breakpoints)
    // and then again each time the debugger breaks. The first time is *not* necessarily an actual
    // break - if the debugger is enabled via \toggledebugger it does not initially break (despite the docs
    // saying it does). An automatic break is done when the debugger is launched via -autodebug.
    //
    // TODO Handle autodebug support
    __declspec(dllexport) void ShowDllForm()
    {
        static bool is_break = false;

        if (check_service())
        {
            if (!is_break)
            {
                is_break = true;
            }
            else
            {
                service->show_dll_form();
            }
        }
    }

    // We are about to begin building the class hierarchy
    __declspec(dllexport) void BuildHierarchy()
    {
        if (check_service())
            service->build_hierarchy();
    }

    // Zero out the class hierarchy
    __declspec(dllexport) void ClearHierarchy()
    {
        if (check_service())
            service->clear_hierarchy();
    }

    // Add a class to the class hierarchy
    __declspec(dllexport) void AddClassToHierarchy(const char* class_name)
    {
        if (check_service())
            service->add_class_to_hierarchy(class_name);
    }

    // Clear all watches of the given kind (legacy, no longer used)
    __declspec(dllexport) void ClearWatch(int watch_kind)
    {
        if (check_service())
            service->clear_a_watch(watch_kind);
    }

    // Clear all watches of the given kind
    __declspec(dllexport) void ClearAWatch(int watch_kind)
    {
        if (check_service())
            service->clear_a_watch(watch_kind);
    }

    // Add a watch.
    __declspec(dllexport) int AddAWatch(int kind, int parent, const char* name, const char* value)
    {
        if (check_service())
            return service->add_a_watch(kind, parent, name, value);
        return 0;
    }

    // Lock a watch list - updates will come.
    __declspec(dllexport) void LockList(int watch_kind)
    {
        if (check_service())
            return service->lock_list(watch_kind);
    }

    // Unlock a watch list - updates are finished.
    __declspec(dllexport) void UnlockList(int watch_kind)
    {
        if (check_service())
            return service->unlock_list(watch_kind);
    }

    // A breakpoint has been added at the given class and line.
    __declspec(dllexport) void AddBreakpoint(const char* class_name, int line_number)
    {
        if (check_service())
            service->add_breakpoint(class_name, line_number);
    }

    // A breakpoint has been removed at the given class and line.
    __declspec(dllexport) void RemoveBreakpoint(const char* class_name, int line_number)
    {
        if (check_service())
            service->remove_breakpoint(class_name, line_number);
    }

    // Show the source file for the given class. Typically called before ShowDllForm() when
    // the debugger breaks.
    __declspec(dllexport) void EditorLoadClass(const char* class_name)
    {
        if (check_service())
            service->editor_load_class(class_name);
    }

    // Set the line number for the class provided by EditorLoadClass. Called before ShowDllForm()
    // when the debugger breaks.
    __declspec(dllexport) void EditorGotoLine(int line_number, int highlight)
    {
        if (check_service())
            service->editor_goto_line(line_number, highlight);
    }

    // A line has been added to the log
    __declspec(dllexport) void AddLineToLog(const char* text)
    {
        if (check_service())
        {
            service->add_line_to_log(text);

            // Unreal doesn't provide an entry point to indicate that the debugger should be stopped, e.g.
            // when the "toggledebugger" command is used when the debugger is running. The one and only entry
            // we get into the Debugger Interface DLL when this happens is a log entry. The existing debugger
            // checks for this special log entry and uses it to initiate a clean shutdown. Extremely gross,
            // but we have to do the same or we can't tell the debugger client that things are shutting down.
            // Even worse, we otherwise can't halt the IO thread, which would prevent Unreal from shutting down
            // cleanly when the game is closed.
            //
            // Note that we get this log entry for both when unreal has initiated the stop and when the client
            // has requested a stop via the "stopdebugger" command. This will only be hit for the former case,
            // as when we process "stopdebugger" we have already toggled the state to 'shutdown' and check_service
            // will not return true.
            if (strcmp(text, magic_debugger_stopped_log_entry) == 0)
            {
                service->shutdown();
                // Run the check_service utility to initiate the shutdown, since unreal won't be calling us again.
                check_service();
            }
        }
    }

    // Clear the call stack
    __declspec(dllexport) void CallStackClear()
    {
        if (check_service())
            service->call_stack_clear();
    }

    // Add an entry to the call stack
    __declspec(dllexport) void CallStackAdd(const char* entry)
    {
        if (check_service())
            service->call_stack_add(entry);
    }

    // Set the current object name. Typically called before ShowDllForm() when the debugger breaks.
    __declspec(dllexport) void SetCurrentObjectName(const char* object_name)
    {
        if (check_service())
            service->set_current_object_name(object_name);
    }

    // This API is documented as being unused. No action.
    __declspec(dllexport) void DebugWindowState(int)
    {
    }
}