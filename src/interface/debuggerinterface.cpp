
// debuggerinterface.cpp
//
// This implements the debugger interface API specified by the UDK docs:
// https://docs.unrealengine.com/udk/Three/DebuggerInterface.html
//
// This interface is compiled into a DLL that is controlled by Unreal itself.
// It contains very little actual debug logic: the sole purpose of this interface
// is to spin up a small RPC server awaiting connections from the out-of-process
// debugger.
//
// Calls from Unreal into the debugger interface entry point are packaged into
// RPC messages and queued for delivery to the debugger, and commands from the
// debugger are sent as RPC messages to this server to be sent to Unreal via
// the Unreal-supplied callback function.

#include <stdio.h>
#include "service.h"

UnrealCallback callback_function = nullptr;

extern "C"
{
    __declspec(dllexport) void SetCallback(void* cb)
    {
        callback_function = static_cast<UnrealCallback>(cb);
    }

    __declspec(dllexport) void ShowDllForm()
    {
        // ShowDllForm is called at multiple points by unreal, but has no parameters to tell us why.
        // It seems to be invoked during the very first startup (after clearing watches and breakpoints)
        // and then again each time the debugger breaks. The first time is *not* necessarily an actual
        // break, though.
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

    __declspec(dllexport) void BuildHierarchy()
    {
        if (check_service())
            service->build_hierarchy();
    }

    __declspec(dllexport) void ClearHierarchy()
    {
        if (check_service())
            service->clear_hierarchy();
    }

    __declspec(dllexport) void AddClassToHierarchy(const char* class_name)
    {
        if (check_service())
            service->add_class_to_hierarchy(class_name);
    }

    __declspec(dllexport) void ClearWatch(int watch_kind)
    {
        if (check_service())
            service->clear_a_watch(watch_kind);
    }

    __declspec(dllexport) void ClearAWatch(int watch_kind)
    {
        if (check_service())
            service->clear_a_watch(watch_kind);
    }

    __declspec(dllexport) int AddAWatch(int kind, int parent, const char* name, const char* value)
    {
        if (check_service())
            return service->add_a_watch(kind, parent, name, value);
        return 0;
    }

    __declspec(dllexport) void LockList(int watch_kind)
    {
        if (check_service())
            return service->lock_list(watch_kind);
    }

    __declspec(dllexport) void UnlockList(int watch_kind)
    {
        if (check_service())
            return service->unlock_list(watch_kind);
    }

    __declspec(dllexport) void AddBreakpoint(const char* class_name, int line_number)
    {
        if (check_service())
            service->add_breakpoint(class_name, line_number);
    }

    __declspec(dllexport) void RemoveBreakpoint(const char* class_name, int line_number)
    {
        if (check_service())
            service->remove_breakpoint(class_name, line_number);
    }

    __declspec(dllexport) void EditorLoadClass(const char* class_name)
    {
        if (check_service())
            service->editor_load_class(class_name);
    }

    __declspec(dllexport) void EditorGotoLine(int line_number, int highlight)
    {
        if (check_service())
            service->editor_goto_line(line_number, highlight);
    }

    __declspec(dllexport) void AddLineToLog(const char* text)
    {
        if (check_service())
            service->add_line_to_log(text);
    }

    __declspec(dllexport) void CallStackClear()
    {
        if (check_service())
            service->call_stack_clear();
    }

    __declspec(dllexport) void CallStackAdd(const char* entry)
    {
        if (check_service())
            service->call_stack_add(entry);
    }

    __declspec(dllexport) void SetCurrentObjectName(const char* object_name)
    {
        if (check_service())
            service->set_current_object_name(object_name);
    }

    // This API is documented as being unused. No action.
    __declspec(dllexport) void DebugWindowState(int)
    {
        printf("DebugWindowState\n");
    }
}