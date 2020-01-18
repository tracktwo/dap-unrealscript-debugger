
// A DAP adapter for unreal

#include <exception>
#include <filesystem>
#include <sstream>

#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include "adapter.h"
#include "client.h"
#include "debugger.h"
#include "signals.h"

LARGE_INTEGER last_time;
LARGE_INTEGER freq;

static void init_timer()
{
    QueryPerformanceFrequency(&freq);
}
static void log_timer(const char* msg)
{
    LARGE_INTEGER lint, elapsed;
    QueryPerformanceCounter(&lint);
    elapsed.QuadPart = lint.QuadPart - last_time.QuadPart;
    elapsed.QuadPart *= 1'000'000;
    elapsed.QuadPart /= freq.QuadPart;
    dap::writef(log_file, "%s %lldus\n", msg, elapsed.QuadPart);
    last_time = lint;
}

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

    // FIXME Remove
    static const char* dev_root = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\XCOM 2 War of the Chosen SDK\\Development\\Src\\";
    static const char* mod_root = "C:\\Users\\jonathan\\xcom\\roguecom\\roguecom\\Src\\";

    std::string class_to_source(const std::string& class_name)
    {
        auto idx = class_name.find('.');
        std::string package = class_name.substr(0, idx);
        std::string file = class_name.substr(idx + 1);

        if (package == "RogueCOM")
        {
            return mod_root + package + "\\Classes\\" + file + ".uc";
        }
        else
        {
            return dev_root + package + "\\Classes\\" + file + ".uc";
        }
    }


    constexpr int variable_encoding_global_bit = 0x4000'0000;
    constexpr int variable_encoding_frame_shift = 20;

    // DAP uses 'variableReferences' to identify scopes and variables within those scopes. These are integer
    // values and must be unique per variable but otherwise have no real meaning to the debugger. We encode
    // the position in the stack frame and watch list in the returned variable reference to make them easy to
    // find in the future. Of the 32-bit integer we encode the reference as follows:
    //
    //  [bit 31] xxxxxxxx xxxxxxxx xxxxxxxx xxxxxxxx  [bit 0]
    //
    //  Bit 31: Always 0
    //  Bit 30: 0 for locals, 1 for globals
    //  Bits 29-20: Frame index: 10 bits = 1024 possible frames
    //  Bits 19-0: Variable index in watch list within frame, + 1: 20 bits, but the 0 value is not used = 1,048,575 possible variables per frame.
    //             The variable index is always offset by 1 from the true index within the debugger watch vector. This is
    //             because the value 0 is special to DAP and so we cannot use it to represent the 0th local variable of the 0th stack frame.
    //             Instead simply shift all variable indices to be 1-indexed instead of 0-indexed. This wastes 1 potential variable slot per
    //             frame when we really only need to special case the first frame, but a million variables is a whole lot anyway.
    int encode_variable_reference(int frame_index, int variable_index, Debugger::WatchKind kind)
    {
        constexpr int max_frame = 1 << 10;
        constexpr int max_var = (1 << 20) - 1;

        if (frame_index >= max_frame)
        {
            std::stringstream stream;
            stream << "encode_variable_reference: frame index " << frame_index << " exceeds maximum value " << max_frame;
            throw std::runtime_error(stream.str());
        }

        if (variable_index > max_var)
        {
            std::stringstream stream;
            stream << "encode_variable_reference: variable index " << variable_index << " exceeds maximum value " << max_var;
            throw std::runtime_error(stream.str());
        }

        // Offset the internal variable index in the watch list by 1 to avoid the 0 issue for the first frame.
        int encoding = variable_index + 1;

        // Add in the frame reference
        encoding |= (frame_index << variable_encoding_frame_shift);

        // Add the user/global bit
        switch (kind)
        {
        case Debugger::WatchKind::Local:
            // Nothing to do for locals
            break;
        case Debugger::WatchKind::Global:
            encoding |= variable_encoding_global_bit;
            break;
        case Debugger::WatchKind::User:
            throw std::runtime_error("encode_variable_reference: cannot encode a user watch.");

        }

        return encoding;
    }

    std::tuple<int, int, Debugger::WatchKind> decode_variable_reference(int variable_reference)
    {
        // The mask to apply to isolate the variable index: shift 1 by the frame shift amount and subtract 1 to set all bits below
        // the first bit of the frame.
        constexpr int variable_mask = (1 << variable_encoding_frame_shift) - 1;

        // Record and then unset the global watch flag.
        Debugger::WatchKind kind = (variable_reference & variable_encoding_global_bit) ? Debugger::WatchKind::Global : Debugger::WatchKind::Local;
        variable_reference &= ~variable_encoding_global_bit;

        // Extract the variable portion.
        int variable_index = (variable_reference & variable_mask) - 1;
        
        // Shift the variable portion off, and what's left is the frame.
        int frame_index = variable_reference >> variable_encoding_frame_shift;

        return { frame_index, variable_index, kind };
    }
}

namespace handlers
{
    void error_handler(const char* msg)
    {
        if (log_enabled)
        {
            dap::writef(log_file, "Session error: %s\n", msg);
            stop_debugger();
        }
    }

    // Handle an initialize request. This returns debugger capabilities.
    dap::InitializeResponse initialize_handler(const dap::InitializeRequest&)
    {
        dap::InitializeResponse response;
        response.supportsDelayedStackTraceLoading = true;
        return response;
    }

    // Handle a 'launch' request. There isn't really anything to do here
    // since the debugger can't actually 'launch' anything.
    dap::LaunchResponse launch_handler(const dap::LaunchRequest& req)
    {
        return {};
    }

    // Handle an 'attach' request. This should be implemented.
    // TODO How to get args like the port number?
    dap::AttachResponse attach_handler(const dap::AttachRequest& req)
    {
        return {};
    }

    dap::DisconnectResponse disconnect(const dap::DisconnectRequest& request)
    {
        client::commands::stop_debugging();
        return {};
    }

    // Handle a 'set breakpoints' request
    dap::ResponseOrError<dap::SetBreakpointsResponse> set_breakpoints_handler(const dap::SetBreakpointsRequest& request)
    {
        dap::SetBreakpointsResponse response;

        auto breakpoints = request.breakpoints.value({});

        // Source references are not supported, we need a source path.
        if (request.source.sourceReference.has_value())
        {
            return dap::Error("add breakpoint: Source references are not supported.");
        }

        std::string class_name = util::source_to_class(request.source);
        // Clear any existing breakpoints in the file
        // TODO fetch the list of active breakpoints for this file and remove them.

        response.breakpoints.resize(breakpoints.size());
        for (size_t i = 0; i < breakpoints.size(); i++)
        {

            // TODO add the breakpint to the debugger state.
            client::commands::add_breakpoint(class_name, breakpoints[i].line);
            // We have no real way to know if the breakpoint addition worked, so just say it's good.
            response.breakpoints[i].verified = true;
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

    // Handle a stack trace request.
    dap::ResponseOrError<dap::StackTraceResponse> stack_trace_handler(const dap::StackTraceRequest& request)
    {
        log_timer("stack trace start");
        if (request.threadId != unreal_thread_id)
        {
            int id = request.threadId;
            return dap::Error("Unknown thread id :%d", id);
        }

        int count = 0;
        dap::StackTraceResponse response;

        if (debugger.get_state() == Debugger::State::busy)
        {
            signals::breakpoint_hit.wait();
        }

        // Remember what frame we are currently looking at so we can restore it if we need to change it to
        // fetch information here.
        int previous_frame = debugger.get_current_frame_index();
        bool disabled_watch_info = false;

        // Loop over frames requested by the client. The request may start at a frame > 0, and may not request all frames.
        for (int frame_index = *request.startFrame; frame_index < debugger.get_callstack().size(); ++frame_index)
        {
            dap::StackFrame dap_frame;
            Debugger::StackFrame& debugger_frame = debugger.get_callstack()[frame_index];

            if (debugger_frame.line_number == 0)
            {
                // We have not yet fetched this frame's line number. Request it now.
                debugger.set_current_frame_index(frame_index);
                debugger.set_state(Debugger::State::waiting_for_frame_line);

                // Tell the debugger interface not to bother sending watch info: we only want line numbers
                // when swapping frames to build a call stack.
                if (!disabled_watch_info)
                {
                    client::commands::toggle_watch_info(false);
                    disabled_watch_info = true;
                }

                // Request a stack change and wait for the line number to be received.
                client::commands::change_stack(frame_index);
                signals::line_received.wait();
                signals::line_received.reset();
                debugger.set_state(Debugger::State::normal);
            }

            dap_frame.id = frame_index;
            dap_frame.line = debugger_frame.line_number;

            dap::Source source;
            source.path = util::class_to_source(debugger_frame.class_name);
            source.name = debugger_frame.class_name;

            dap_frame.source = source;
            dap_frame.column = 0;
            dap_frame.name = debugger_frame.function_name;
            response.stackFrames.push_back(dap_frame);

            // If we have reached the number of frames requested by the client we can stop.
            if (++count >= *request.levels)
            {
                break;
            }
        }

        // Restore the frame index to our original value.
        debugger.set_current_frame_index(previous_frame);

        // If we asked the debugger to stop sending watch info, turn it back on now
        if (disabled_watch_info)
        {
            client::commands::toggle_watch_info(true);
        }

        response.totalFrames = debugger.get_callstack().size();
        log_timer("stack trace end");

        return response;
    }

    // Handle a request for scope information
    dap::ResponseOrError<dap::ScopesResponse> scopes_handler(const dap::ScopesRequest& request)
    {

        log_timer("scopes start");

        if (debugger.get_state() == Debugger::State::busy)
        {
            signals::breakpoint_hit.wait();
        }

        dap::Scope scope;
        scope.name = "Locals";
        scope.presentationHint = "locals";
        scope.expensive = true;
        scope.variablesReference = util::encode_variable_reference(request.frameId, 0, Debugger::WatchKind::Local);
        if (debugger.get_callstack()[request.frameId].fetched_watches)
        {
            scope.namedVariables = debugger.get_callstack()[request.frameId].local_watches[0].children.size();
        }
        dap::ScopesResponse response;
        response.scopes.push_back(scope);

        scope.name = "Globals";
        scope.presentationHint = {};
        scope.variablesReference = util::encode_variable_reference(request.frameId, 0, Debugger::WatchKind::Global);
        scope.expensive = true;
        if (debugger.get_callstack()[request.frameId].fetched_watches)
        {
            scope.namedVariables = debugger.get_callstack()[request.frameId].global_watches[0].children.size();
        }
        response.scopes.push_back(scope);
        log_timer("scopes end");
        return response;
    }

    dap::ResponseOrError<dap::VariablesResponse> variables_handler(const dap::VariablesRequest& request)
    {
        log_timer("variables start");

        if (debugger.get_state() == Debugger::State::busy)
        {
            signals::breakpoint_hit.wait();
        }

        auto [frame_index, variable_index, watch_kind] = util::decode_variable_reference(request.variablesReference);

        // If we don't have watch info for this frame yet we need to collect it now.
        if (!debugger.get_callstack()[frame_index].fetched_watches)
        {
            int saved_frame_index = debugger.get_current_frame_index();
            debugger.set_current_frame_index(frame_index);
            debugger.set_state(Debugger::State::waiting_for_frame_watches);

            // Request a stack change and wait for the watches to to be received.
            client::commands::change_stack(frame_index);
            signals::watches_received.wait();
            signals::watches_received.reset();
            debugger.set_state(Debugger::State::normal);
            debugger.set_current_frame_index(saved_frame_index);
            debugger.get_callstack()[frame_index].fetched_watches = true;
        }

        const Debugger::WatchList& watch_list = debugger.get_callstack()[frame_index].get_watches(watch_kind);

        if (request.start.value(0) != 0 || request.count.value(0) != 0)
        {
            // TODO Implement chunked responses.
            return dap::Error("Debugger does not support chunked variable requests");
        }

        dap::VariablesResponse response;

        // The watch list can be empty, e.g. local watches for a function with no parameters and no local variables.
        if (!watch_list.empty())
        {
            const Debugger::WatchData& parent = watch_list[variable_index];

            for (int child_index : parent.children)
            {
                const Debugger::WatchData& watch = watch_list[child_index];
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
                    var.namedVariables = watch.children.size();
                    var.indexedVariables = 0;
                }
                response.variables.push_back(var);
            }
        }

        log_timer("variables end");
        return response;
    }

    dap::PauseResponse pause_handler(const dap::PauseRequest& request)
    {
        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        client::commands::break_cmd();
        return {};
    }

    dap::ContinueResponse continue_handler(const dap::ContinueRequest& request)
    {
        while (debugger.get_state() != Debugger::State::normal)
            ;

        client::commands::toggle_watch_info(true);

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(Debugger::State::busy);
        signals::breakpoint_hit.reset();
        client::commands::go();
        return {};
    }

    dap::NextResponse next_handler(const dap::NextRequest& request)
    {
        while (debugger.get_state() != Debugger::State::normal)
            ;

        client::commands::toggle_watch_info(true);

        log_timer("next start");
        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(Debugger::State::busy);
        signals::breakpoint_hit.reset();
        client::commands::step_over();
        log_timer("next end");
        return {};
    }

    dap::StepInResponse step_in_handler(const dap::StepInRequest& request)
    {
        while (debugger.get_state() != Debugger::State::normal)
            ;

        client::commands::toggle_watch_info(true);

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(Debugger::State::busy);
        signals::breakpoint_hit.reset();
        client::commands::step_into();
        return {};
    }

    dap::StepOutResponse step_out_handler(const dap::StepOutRequest& request)
    {
        while (debugger.get_state() != Debugger::State::normal)
            ;

        client::commands::toggle_watch_info(true);

        // Any code execution change results in fresh information from unreal so we need to reset
        // to the top-most frame.
        debugger.set_current_frame_index(0);
        debugger.set_state(Debugger::State::busy);
        signals::breakpoint_hit.reset();
        client::commands::step_outof();
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
    log_timer("breakpoint start");
    if (!session)
        return;

    if (debugger.get_state() == Debugger::State::busy)
        debugger.set_state(Debugger::State::normal);

    signals::breakpoint_hit.fire();

    dap::StoppedEvent ev;
    ev.reason = "breakpoint";
    ev.threadId = unreal_thread_id;
    session->send(ev);
    log_timer("breakpoint end");
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

// The debugger has stopped. Send a terminated event to the client, and begin
// our shutdown process.
void debugger_terminated()
{
    if (!session)
        return;
    dap::TerminatedEvent ev;
    session->send(ev);
    stop_debugger();
}

void create_adapter()
{
    init_timer();
    session = dap::Session::create();

    // The adapter will communicate over stdin/stdout with the debug client in the editor.


  //  session->bind(dap::file(stdin, false), dap::file(stdout, false));

    // Bind handlers.
    session->onError(&handlers::error_handler);
    session->registerHandler(&handlers::initialize_handler);
    session->registerHandler(&handlers::launch_handler);
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
    session->registerHandler(&handlers::disconnect);

    session->registerSentHandler(&sent_handlers::initialize_response);
}

void on_connect(const std::shared_ptr<dap::ReaderWriter>& streams)
{
    create_adapter();
  //  auto spy = dap::spy(static_cast<std::shared_ptr<dap::Reader>>(streams), log_file);
   // session->bind(spy, static_cast<std::shared_ptr<dap::Writer>>(streams));
    session->bind(streams);
}

void start_debug_server()
{
    server = dap::net::Server::create();
    server->start(9444, on_connect);
}

void stop_debug_server()
{
    server->stop();
}

void start_debug_local()
{
    create_adapter();

    std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
    std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
  //  session->bind(dap::spy(in, log_file), dap::spy(out, log_file));
    session->bind(in, out);
    dap::writef(log_file, "Bound to in/out\n");
}

void stop_adapter()
{
    session.reset();
    server.reset();
}
