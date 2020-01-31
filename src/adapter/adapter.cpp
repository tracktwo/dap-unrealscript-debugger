
// A DAP adapter for unreal

#include <exception>
#include <filesystem>
#include <sstream>
#include <optional>
#include <map>

#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include "adapter.h"
#include "client.h"
#include "debugger.h"
#include "signals.h"

// Define a custom "launch" request type so we can receive specific launch parameters from
// vscode.
namespace dap
{
    struct UnrealLaunchRequest : LaunchRequest
    {
        using Response = LaunchResponse;

        UnrealLaunchRequest() = default;
        ~UnrealLaunchRequest() = default;

        // A vector of strings for the list of source roots.
        optional<array<string>> sourceRoots;
    };

    struct UnrealAttachRequest : AttachRequest
    {
        using Response = AttachResponse;

        UnrealAttachRequest() = default;
        ~UnrealAttachRequest() = default;

        // A vector of strings for the list of source roots.
        optional<array<string>> sourceRoots;
    };


    DAP_DECLARE_STRUCT_TYPEINFO(UnrealLaunchRequest);
    DAP_DECLARE_STRUCT_TYPEINFO(UnrealAttachRequest);

    DAP_IMPLEMENT_STRUCT_TYPEINFO(UnrealLaunchRequest,
        "launch",
        DAP_FIELD(restart, "__restart"),
        DAP_FIELD(noDebug, "noDebug"),
        DAP_FIELD(sourceRoots, "sourceRoots"));

    DAP_IMPLEMENT_STRUCT_TYPEINFO(UnrealAttachRequest,
        "attach",
        DAP_FIELD(restart, "__restart"),
        DAP_FIELD(sourceRoots, "sourceRoots"));
}

namespace unreal_debugger::adapter
{
using namespace unreal_debugger::client;

std::unique_ptr<dap::Session> session;
std::unique_ptr<dap::net::Server> server;

// Unrealscript debugger does not expose separate threads. We arbitrarily name the sole
// thread we can access '1'.
static const int unreal_thread_id = 1;

namespace util
{
    // Given a source reference, return the unreal class name, qualified with package name.
    std::string source_to_class(const dap::Source& source)
    {
        if (source.sourceReference.value(0) != 0)
        {
            // We don't handle source references, just paths.
            throw std::runtime_error("Received source reference instead of source path.\n");
        }

        std::filesystem::path path{ *source.path };

        // The class name should be the last component of the path, minus extension.
        auto class_name = path.stem();

        // Get the parent path. This should be something like 'Classes'
        if (!path.has_parent_path())
        {
            throw std::runtime_error(std::string("Unexpected source path format: ") + *source.path );
        }

        // Get the next parent path. This is expected to be the name of the package.
        path = path.parent_path();
        if (!path.has_parent_path())
        {
            throw std::runtime_error(std::string("Unexpected source path format: ") + *source.path);
        }

        // The package name should be the current stem
        auto package_name = path.parent_path().stem();

        // Build the "Package.Class" fully qualified name.
        return package_name.string() + "." + class_name.string();
    }

    // Normalize a source file path to the true path name on disk. The path that we have built by gluing a user-provided
    // source root to the package and class name that Unreal provided may not exactly match the true file name of the file
    // on disk due to casing differences. E.g. the source-root may not have the correct casing, and while fs::exists() ignores
    // the case differences VS Code currently doesn't do a great job at detecting two different casings of the same file name
    // as being the same. If the cases don't match and you have opened the file in VS Code (which uses the true file path as it
    // appears on disk) the debugger may open another copy of this same file when a breakpoint within it is hit but the source
    // path returned from the debugger doesn't match exactly.
    //
    // To help reduce this annoyance the file name is canonicalized to the true path recorded on disk before returning. This
    // is not simple to do on Windows, we need to actually open the file to query it, and it needs to use gross Win32 APIs.
    std::string normalize_path(const std::string& path)
    {
        HANDLE hnd;
        char buf[MAX_PATH];

        // Open the file to get a handle
        hnd = CreateFile(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hnd == INVALID_HANDLE_VALUE)
        {
            log("normalize_path: Could not open file (error %d\n)", GetLastError());
            return path;
        }

        // Get the 'final path name' from the handle. Try with a reasonable buffer first, and if that fails allocate
        // one large enough to hold the result.
        unsigned long sz = GetFinalPathNameByHandle(hnd, buf, MAX_PATH, 0);
        std::string str;
        if (sz < MAX_PATH)
        {
            str = buf;
        }
        else
        {
            auto large_buf = std::make_unique<char[]>(sz + 1);
            GetFinalPathNameByHandle(hnd, large_buf.get(), sz + 1, 0);
            str = large_buf.get();
        }

        // We're now done with the handle
        CloseHandle(hnd);

        // The returned string may be prefixed with the \\?\ long path prefix. Strip it, cause VS Code doesn't want to
        // see it.
        if (str.find(R"(\\?\)") == 0)
            str = str.substr(4);
        return str;
    }

    // Normalizing the source file paths is expensive, so keep a cache of known mappings from class names to source file names.
    std::map<std::string, std::string> file_name_cache;

    // Given a class name, return a source name by attempting to apply each of the configured source-roots in order.
    std::string class_to_source(const std::string& class_name)
    {
        // Try to find a cached version of the file first.
        auto  it = file_name_cache.find(class_name);
        if (it != file_name_cache.end())
        {
            return it->second;
        }

        // No dice. Split the name into package and file name and search the source roots until we find a match
        // (or don't).
        auto idx = class_name.find('.');
        std::string package = class_name.substr(0, idx);
        std::string file = class_name.substr(idx + 1);

        for (fs::path path : source_roots)
        {
            path = path / package / "Classes" / (file + ".uc");
            if (fs::exists(path))
            {
                std::string normalized = normalize_path(path.string());
                file_name_cache.insert({ class_name, normalized } );
                return normalized;
            }
        }
        log("Error: Cannot find source path for %s\n", class_name.c_str());
        return class_name;
    }

    // DAP uses 'variableReferences' to identify scopes and variables within those scopes. These are integer
    // values and must be unique per variable but otherwise have no real meaning to the debugger. We encode
    // the position in the stack frame and watch list in the returned variable reference to make them easy to
    // find in the future. Of the 32-bit integer we encode the reference as follows:
    //
    //  [bit 31] xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx  [bit 0]
    //
    //  Bit 31: always 0
    //  Bit 30: 0 if set, this is a user watch and bit 29 is unset.
    //  Bit 29: 0 for local watch, 1 for global watch
    //  Bits 28-22: Frame index: 7 bits = 128 possible frames (always 0 for user watches)
    //  Bits 21-0: Variable index in watch list within frame, + 1: 20 bits, but the 0 value is not used = 4,194,303 possible variables per frame.
    //             The variable index is always offset by 1 from the true index within the debugger watch vector. This is
    //             because the value 0 is special to DAP and so we cannot use it to represent the 0th local variable of the 0th stack frame.
    //             Instead simply shift all variable indices to be 1-indexed instead of 0-indexed. This wastes 1 potential variable slot per
    //             frame when we really only need to special case the first frame, but 4 million variables is a whole lot anyway.

    constexpr int variable_encoding_user_bit = 0x4000'0000;
    constexpr int variable_encoding_global_bit = 0x2000'0000;
    constexpr int variable_encoding_frame_shift = 22;
    constexpr int variable_encoding_max_frame = 1 << 7;
    constexpr int variable_encoding_max_var = (1 << 22) - 1;

    int encode_variable_reference(int frame_index, int variable_index, watch_kind kind)
    {
        if (frame_index >= variable_encoding_max_frame)
        {
            std::stringstream stream;
            stream << "encode_variable_reference: frame index " << frame_index << " exceeds maximum value " << variable_encoding_max_frame;
            throw std::runtime_error(stream.str());
        }

        if (variable_index > variable_encoding_max_var)
        {
            std::stringstream stream;
            stream << "encode_variable_reference: variable index " << variable_index << " exceeds maximum value " << variable_encoding_max_var;
            throw std::runtime_error(stream.str());
        }

        // Offset the internal variable index in the watch list by 1 to avoid the 0 issue for the first frame.
        int encoding = variable_index + 1;

        // Add in the frame reference
        encoding |= (frame_index << variable_encoding_frame_shift);

        // Add the user/global bits
        switch (kind)
        {
        case watch_kind::local:
            // Nothing to do for locals
            break;
        case watch_kind::global:
            encoding |= variable_encoding_global_bit;
            break;
        case watch_kind::user:
            encoding |= variable_encoding_user_bit;

        }

        return encoding;
    }

    watch_kind decode_watch_kind(int variable_reference)
    {
        if (variable_reference & variable_encoding_user_bit)
            return watch_kind::user;

        if (variable_reference & variable_encoding_global_bit)
            return watch_kind::global;

        return watch_kind::local;
    }

    std::tuple<int, int, watch_kind> decode_variable_reference(int variable_reference)
    {
        // The mask to apply to isolate the variable index: shift 1 by the frame shift amount and subtract 1 to set all bits below
        // the first bit of the frame.
        constexpr int variable_mask = (1 << variable_encoding_frame_shift) - 1;

        // Record and then unset the global or user watch flag.
        watch_kind kind = decode_watch_kind(variable_reference);
        variable_reference &= ~(variable_encoding_global_bit | variable_encoding_user_bit);

        // Extract the variable portion.
        int variable_index = (variable_reference & variable_mask) - 1;
        
        // Shift the variable portion off, and what's left is the frame.
        int frame_index = variable_reference >> variable_encoding_frame_shift;

        return { frame_index, variable_index, kind };
    }

    // Initialize the list of source roots provided as launch arguments. Returns a list of bad roots (if any).
    std::vector<std::string> init_source_roots(const std::vector<std::string>& in_roots)
    {
        std::vector<std::string> bad_roots;

        for (const std::string& r : in_roots)
        {
            fs::path root_path{ r };
            if (!fs::exists(root_path))
            {
                bad_roots.push_back(r);
            }
            // Insert the path exactly as the user wrote it.
            source_roots.push_back(root_path);
        }

        return bad_roots;
    }
}

namespace handlers
{
    void error_handler(const char* msg)
    {
        log("Session error: %s\n", msg);
        stop_debugger();
    }

    // Handle an initialize request. This returns debugger capabilities.
    dap::InitializeResponse initialize_handler(const dap::InitializeRequest&)
    {
        dap::InitializeResponse response;
        response.supportsDelayedStackTraceLoading = true;
        response.supportsValueFormattingOptions = true;
        return response;
    }

    // Handle a 'launch' request.
    dap::ResponseOrError<dap::LaunchResponse> launch_handler(const dap::UnrealLaunchRequest& req)
    {
        if (req.sourceRoots)
        {
            auto bad_roots = util::init_source_roots(*req.sourceRoots);
            if (!bad_roots.empty())
            {
                std::stringstream msg;
                msg << "Error: Bad source roots:" << std::endl;
                for (const auto& r : bad_roots)
                {
                    msg << r << std::endl;
                }

                dap::Error err;
                err.message = msg.str();
                return err;
            }
        }
        return dap::LaunchResponse{};
    }

    // Handle an 'attach' request.
    dap::ResponseOrError<dap::AttachResponse> attach_handler(const dap::UnrealAttachRequest& req)
    {
        if (req.sourceRoots)
        {
            auto bad_roots = util::init_source_roots(*req.sourceRoots);
            if (!bad_roots.empty())
            {
                std::stringstream msg;
                msg << "Error: Bad source roots:" << std::endl;
                for (const auto& r : bad_roots)
                {
                    msg << r << std::endl;
                }

                dap::Error err;
                err.message = msg.str();
                return err;
            }
        }
        return dap::AttachResponse{};
    }

    dap::DisconnectResponse disconnect(const dap::DisconnectRequest& request)
    {
        stop_debugging();
        return {};
    }

    // Handle a 'set breakpoints' request
    dap::ResponseOrError<dap::SetBreakpointsResponse> set_breakpoints_handler(const dap::SetBreakpointsRequest& request)
    {
        dap::SetBreakpointsResponse response;

        if (debugger.get_state() == debugger_state::state::busy)
        {
            signals::breakpoint_hit.wait();
        }

        auto breakpoints = request.breakpoints.value({});

        // Source references are not supported, we need a source path.
        if (request.source.sourceReference.has_value())
        {
            return dap::Error("add breakpoint: Source references are not supported.");
        }

        std::string class_name = util::source_to_class(request.source);

        // Clear any existing breakpoints in the file
        if (const std::vector<int>* existing_breakpoints = debugger.get_breakpoints(class_name))
        {
            for (int line : *existing_breakpoints)
            {
                remove_breakpoint(class_name, line);
            }
        }
        
        response.breakpoints.resize(breakpoints.size());
        for (size_t i = 0; i < breakpoints.size(); i++)
        {
            debugger.set_state(debugger_state::state::waiting_for_add_breakpoint);
            add_breakpoint(class_name, breakpoints[i].line);
            signals::breakpoint_added.wait();
            signals::breakpoint_added.reset();
            debugger.set_state(debugger_state::state::normal);

            if (const std::vector<int>* list = debugger.get_breakpoints(class_name))
            {
                response.breakpoints[i].line = list->back();
                response.breakpoints[i].verified = true;
            }
            else
            {
                response.breakpoints[i].verified = false;
            }
        }

        return response;
    }

    // Handle 'set exception breakpoints'. Unreal does not have exception breakpoints except for the special
    // case of breaking on access of 'none'. This is not yet implemented.
    dap::SetExceptionBreakpointsResponse set_exception_breakpoints_handler(const dap::SetExceptionBreakpointsRequest& request)
    {
        return {};
    }

    // Handle the 'threads' response. Unreal only exposes a single thread to the debugger for unrealscript, so we can return
    // a fixed value for it.
    dap::ThreadsResponse threads_handler(const dap::ThreadsRequest& request)
    {
        dap::ThreadsResponse response;
        dap::Thread thread;

        thread.id = unreal_thread_id;
        thread.name = "UnrealScript";
        response.threads.push_back(thread);
        return response;
    }

    // Change the debugger frame, blocking until the frame has changed. Optionally requests watch info for the new frame.
    void change_frame_and_wait(int frame, bool with_watches)
    {
        debugger.set_current_frame_index(frame);
        change_stack(frame);
        if (with_watches)
        {
            debugger.set_state(debugger_state::state::waiting_for_frame_watches);
            signals::watches_received.wait();
            signals::watches_received.reset();
        }
        else
        {
            debugger.set_state(debugger_state::state::waiting_for_frame_line);
            signals::line_received.wait();
            signals::line_received.reset();
        }

        debugger.set_state(debugger_state::state::normal);
    }

    // Handle a stack trace request.
    dap::ResponseOrError<dap::StackTraceResponse> stack_trace_handler(const dap::StackTraceRequest& request)
    {
        if (request.threadId != unreal_thread_id)
        {
            int id = request.threadId;
            return dap::Error("Unknown thread id :%d", id);
        }

        int count = 0;
        dap::StackTraceResponse response;

        if (debugger.get_state() == debugger_state::state::busy)
        {
            signals::breakpoint_hit.wait();
        }

        // Remember what frame we are currently looking at so we can restore it if we need to change it to
        // fetch information here.
        int previous_frame = debugger.get_current_frame_index();
        bool disabled_watch_info = false;

        // Loop over frames requested by the client. The request may start at a frame > 0, and may not request all frames.
        for (int frame_index = *request.startFrame; frame_index < client::debugger.callstack_size(); ++frame_index)
        {
            dap::StackFrame dap_frame;
            const stack_frame& debugger_frame = debugger.get_stack_frame(frame_index);

            if (debugger_frame.line_number == 0)
            {
                // We have not yet fetched this frame's line number. Request it now.
                debugger.set_current_frame_index(frame_index);
                debugger.set_state(debugger_state::state::waiting_for_frame_line);

                // Tell the debugger interface not to bother sending watch info: we only want line numbers
                // when swapping frames to build a call stack.
                if (!disabled_watch_info)
                {
                    toggle_watch_info(false);
                    disabled_watch_info = true;
                }

                // Request a stack change and wait for the line number to be received.
                change_stack(frame_index);
                signals::line_received.wait();
                signals::line_received.reset();
                debugger.set_state(debugger_state::state::normal);
            }

            dap_frame.id = frame_index;
            dap_frame.line = debugger_frame.line_number;

            dap::Source source;
            source.path = util::class_to_source(debugger_frame.class_name);
            source.name = debugger_frame.class_name;

            dap_frame.source = source;
            dap_frame.column = 0;
            if (request.format)
            {
                std::string format_str;
                if (request.format->includeAll || request.format->module)
                {
                    format_str += debugger_frame.class_name;
                    format_str += ".";
                }
                format_str += debugger_frame.function_name;

                if (request.format->includeAll || request.format->line)
                {
                    format_str += " Line ";
                    format_str += std::to_string(debugger_frame.line_number);
                }
                dap_frame.name = format_str;
            }
            else
            {
                dap_frame.name = debugger_frame.function_name;
            }
            response.stackFrames.push_back(dap_frame);

            // If we have reached the number of frames requested by the client we can stop.
            if (++count >= *request.levels)
            {
                break;
            }
        }

        // Restore the frame index to our original value.
        if (previous_frame != debugger.get_current_frame_index())
        {
            change_frame_and_wait(previous_frame, false);
        }

        // If we asked the debugger to stop sending watch info, turn it back on now
        if (disabled_watch_info)
        {
            toggle_watch_info(true);
        }

        response.totalFrames = static_cast<int>(debugger.callstack_size());

        return response;
    }

    // Handle a request for scope information
    dap::ResponseOrError<dap::ScopesResponse> scopes_handler(const dap::ScopesRequest& request)
    {
        if (debugger.get_state() == debugger_state::state::busy)
        {
            signals::breakpoint_hit.wait();
        }

        dap::Scope scope;
        scope.name = "Locals";
        scope.presentationHint = "locals";
        scope.variablesReference = util::encode_variable_reference(request.frameId, 0, watch_kind::local);
        if (debugger.get_stack_frame(request.frameId).fetched_watches)
        {
            scope.namedVariables = static_cast<int>(debugger.get_stack_frame(request.frameId).local_watches[0].children.size());
        }
        dap::ScopesResponse response;
        response.scopes.push_back(scope);

        scope.name = "Globals";
        scope.presentationHint = {};
        scope.variablesReference = util::encode_variable_reference(request.frameId, 0, watch_kind::global);
        if (debugger.get_stack_frame(request.frameId).fetched_watches)
        {
            scope.namedVariables = static_cast<int>(debugger.get_stack_frame(request.frameId).global_watches[0].children.size());
        }
        response.scopes.push_back(scope);
        return response;
    }

    void fetch_watches(int frame_index)
    {
        int saved_frame_index = debugger.get_current_frame_index();
        change_frame_and_wait(frame_index, true);
        debugger.get_current_stack_frame().fetched_watches = true;

        if (debugger.get_current_frame_index() != saved_frame_index)
        {
            // Reset the debugger's internal state to the original callstack.
            // We don't need var information for this (we already have the previous
            // frame), so turn it off.
            change_frame_and_wait(saved_frame_index, false);
        }
    }

    dap::ResponseOrError<dap::VariablesResponse> variables_handler(const dap::VariablesRequest& request)
    {
        if (debugger.get_state() == debugger_state::state::busy)
        {
            signals::breakpoint_hit.wait();
        }

        auto [frame_index, variable_index, watch_kind] = util::decode_variable_reference(request.variablesReference);

        // If we don't have watch info for this frame yet we need to collect it now.
        if (!debugger.get_stack_frame(frame_index).fetched_watches)
        {
            fetch_watches(frame_index);
        }

        const watch_list& watch_list = debugger.get_stack_frame(frame_index).get_watches(watch_kind);

        if (request.start.value(0) != 0 || request.count.value(0) != 0)
        {
            // TODO Implement chunked responses.
            return dap::Error("debugger_state does not support chunked variable requests");
        }

        dap::VariablesResponse response;

        // The watch list can be empty, e.g. local watches for a function with no parameters and no local variables.
        if (!watch_list.empty())
        {
            const watch_data& parent = watch_list[variable_index];

            for (int child_index : parent.children)
            {
                const watch_data& watch = watch_list[child_index];
                dap::Variable var;
                var.name = watch.name;
                var.type = watch.type;
                var.value = watch.value;

                // If this variable has no children then we report its variable reference as 0. Otherwise
                // we report this variable's index and the client will send a new variable request with this
                // reference to fetch its children.
                if (watch.children.empty())
                {
                    var.variablesReference = 0;
                    var.namedVariables = 0;
                    var.indexedVariables = 0;
                }
                else
                {
                    int child_reference = util::encode_variable_reference(frame_index, child_index, watch_kind);
                    var.variablesReference = watch.children.empty() ? 0 : child_reference;
                    var.namedVariables = static_cast<int>(watch.children.size());
                    var.indexedVariables = 0;
                }
                response.variables.push_back(var);
            }
        }

        return response;
    }

    static dap::EvaluateResponse make_user_watch_response(int frame_index, int index)
    {
        dap::EvaluateResponse response;
        const watch_data& watch = debugger.get_stack_frame(frame_index).user_watches[index];
        response.type = watch.type;
        response.result = watch.value;
        if (!watch.children.empty())
        {
            response.variablesReference = util::encode_variable_reference(0, index, watch_kind::user);
            response.namedVariables = static_cast<int>(watch.children.size());
        }
        return response;
    }

    dap::EvaluateResponse evaluate_handler(const dap::EvaluateRequest& request)
    {
        if (request.context && *request.context != "watch")
        {
            dap::EvaluateResponse response;
            response.result = "Unsupported expression";
            return response;
        }

        int frame_index = request.frameId ? static_cast<int>(*request.frameId) : 0;

        if (!debugger.get_stack_frame(frame_index).fetched_watches)
        {
            fetch_watches(frame_index);
        }

        const watch_list& user_watches = debugger.get_stack_frame(frame_index).user_watches;

        // If we have existing watches try to find it in the list first. It will be a child
        // of the root node if so, we don't need to search arbitrary children throughout the list.
        if (int index = debugger.find_user_watch(frame_index, request.expression); index >= 0)
        {
            return make_user_watch_response(frame_index, index);
        }

        // If we've failed to find this watch then we need to request it.
        debugger.set_state(debugger_state::state::waiting_for_user_watches);
        add_watch(request.expression);
        signals::user_watches_received.wait();
        signals::user_watches_received.reset();
        debugger.set_state(debugger_state::state::normal);

        // Now find the watch.
        if (int index = debugger.find_user_watch(frame_index, request.expression); index >= 0)
        {
            return make_user_watch_response(frame_index, index);
        }

        // We have failed again -- this watch must be bad.
        dap::EvaluateResponse response;
        response.result = "Invalid watch";
        return response;
    }

    dap::PauseResponse pause_handler(const dap::PauseRequest& request)
    {
        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        break_cmd();
        return {};
    }

    dap::ContinueResponse continue_handler(const dap::ContinueRequest& request)
    {
        while (debugger.get_state() != debugger_state::state::normal)
            ;

        toggle_watch_info(true);
        clear_watch();

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(debugger_state::state::busy);
        signals::breakpoint_hit.reset();
        go();
        return {};
    }

    dap::NextResponse next_handler(const dap::NextRequest& request)
    {
        while (debugger.get_state() != debugger_state::state::normal)
            ;

        toggle_watch_info(true);
        clear_watch();

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(debugger_state::state::busy);
        signals::breakpoint_hit.reset();
        step_over();
        return {};
    }

    dap::StepInResponse step_in_handler(const dap::StepInRequest& request)
    {
        while (debugger.get_state() != debugger_state::state::normal)
            ;

        toggle_watch_info(true);
        clear_watch();

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(debugger_state::state::busy);
        signals::breakpoint_hit.reset();
        client::step_into();
        return {};
    }

    dap::StepOutResponse step_out_handler(const dap::StepOutRequest& request)
    {
        while (debugger.get_state() != debugger_state::state::normal)
            ;

        client::toggle_watch_info(true);
        client::clear_watch();

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(debugger_state::state::busy);
        signals::breakpoint_hit.reset();
        client::step_outof();
        return {};
    }
}

namespace sent_handlers
{
    void initialize_response(const dap::ResponseOrError<dap::InitializeResponse>&)
    {
        session->send(dap::InitializedEvent());
    }
}

// Tell the debug client that the debugger is stopped at a breakpoint.
void breakpoint_hit()
{
    if (!session)
        return;

    if (debugger.get_state() == debugger_state::state::busy)
        debugger.set_state(debugger_state::state::normal);

    signals::breakpoint_hit.fire();

    dap::StoppedEvent ev;
    ev.reason = "breakpoint";
    ev.threadId = unreal_thread_id;
    session->send(ev);
}

// Tell the debug client that the debugger has produced some log output.
void console_message(const std::string& msg)
{
    if (!session)
        return;

    dap::OutputEvent ev;
    ev.output = msg + "\r\n";
    ev.category = "console";
    session->send(ev);
}

// The debugger has stopped. Send a terminated event to the client. It should respond
// with a disconnect.
void debugger_terminated()
{
    if (!session)
        return;
    dap::TerminatedEvent ev;
    session->send(ev);
   // stop_debugger();
}

void create_adapter()
{
    session = dap::Session::create();

    // Bind handlers.
    session->onError(&handlers::error_handler);
    session->registerHandler(&handlers::initialize_handler);
    session->registerHandler(&handlers::launch_handler);
    session->registerHandler(&handlers::attach_handler);
    session->registerHandler(&handlers::set_breakpoints_handler);
    session->registerHandler(&handlers::set_exception_breakpoints_handler);
    session->registerHandler(&handlers::threads_handler);
    session->registerHandler(&handlers::stack_trace_handler);
    session->registerHandler(&handlers::scopes_handler);
    session->registerHandler(&handlers::variables_handler);
    session->registerHandler(&handlers::pause_handler);
    session->registerHandler(&handlers::continue_handler);
    session->registerHandler(&handlers::next_handler);
    session->registerHandler(&handlers::step_in_handler);
    session->registerHandler(&handlers::step_out_handler);
    session->registerHandler(&handlers::evaluate_handler);
    session->registerHandler(&handlers::disconnect);

    session->registerSentHandler(&sent_handlers::initialize_response);
}

void on_connect(const std::shared_ptr<dap::ReaderWriter>& streams)
{
    create_adapter();
    session->bind(streams);
}

void start_adapter()
{
    create_adapter();

    if (debug_port > 0)
    {
        server = dap::net::Server::create();
        server->start(debug_port, on_connect);
    }
    else
    {
        std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
        std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
        session->bind(in, out);
        log("Bound to in/out\n");
    }
}

void stop_adapter()
{
    if (server)
    {
        server->stop();
    }
    session.reset();
    server.reset();
}

}