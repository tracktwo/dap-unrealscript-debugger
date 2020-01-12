
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
        printf("Got callback\n");
        callback_function = static_cast<UnrealCallback>(cb);
    }

    __declspec(dllexport) void ShowDllForm()
    {
        printf("Show DLL frame\n");
        if (!service)
        {
            service = std::make_unique<DebuggerService>();
            service->start();
        }

        service->show_dll_form();
    }

    __declspec(dllexport) void BuildHierarchy()
    {
        printf("BuildHierarchy\n");
        if (service)
            service->build_hierarchy();
    }

    __declspec(dllexport) void ClearHierarchy()
    {
        printf("ClearHierarchy\n");
    }

    __declspec(dllexport) void AddClassToHierarchy(const char* classname)
    {
        printf("AddClassToHierarchy %s\n", classname);
    }

    __declspec(dllexport) void ClearWatch(int)
    {
        printf("ClearWatch\n");
    }

    __declspec(dllexport) void ClearAWatch(int kind)
    {
        printf("ClearAWatch\n");
       // return debugger.clear_watch(static_cast<UnrealDebugger::WatchKind>(kind));
    }

    __declspec(dllexport) int AddAWatch(int kind, int parent, const char* name, const char* value)
    {
        printf("AddAWatch\n");
        return 1;
      //  return debugger.add_watch(static_cast<UnrealDebugger::WatchKind>(kind), parent, name, value);
    }

    __declspec(dllexport) void LockList(int)
    {
        printf("LockList\n");
    }

    __declspec(dllexport) void UnlockList(int)
    {
        printf("UnlockList\n");
    }

    __declspec(dllexport) void AddBreakpoint(const char*, int)
    {
        printf("AddBreakpoint\n");
    }

    __declspec(dllexport) void RemoveBreakpoint(const char*, int)
    {
        printf("RemoveBreakpoint\n");
    }

    __declspec(dllexport) void EditorLoadClass(const char* name)
    {
        printf("EditorLoadClass\n");
      //  debugger.load_class(name);
    }

    __declspec(dllexport) void EditorGotoLine(int linenumber, int)
    {
        printf("EditorGotoLine\n");
     //   debugger.goto_line(linenumber);
    }

    __declspec(dllexport) void AddLineToLog(const char* )
    {
       // printf("AddLineToLog: %s\n", log);
    }

    __declspec(dllexport) void CallStackClear()
    {
        printf("ClearCallStack\n");
      //  debugger.clear_callstack();
    }

    __declspec(dllexport) void CallStackAdd(const char* name)
    {
        printf("AddCallStack\n");
     //   debugger.add_callstack(name);
    }

    __declspec(dllexport) void SetCurrentObjectName(const char*)
    {
        printf("SetCurrentObjectName\n");
    }

    __declspec(dllexport) void DebugWindowState(int)
    {
        printf("DebugWindowState\n");
    }
}