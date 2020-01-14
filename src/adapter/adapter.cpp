
// A DAP adapter for unreal

#include <exception>
#include <filesystem>

#include "dap/io.h"
#include "dap/network.h"
#include "dap/protocol.h"
#include "dap/session.h"

#include "adapter.h"
#include "client.h"
#include "debugger.h"

std::unique_ptr<dap::Session> session;
std::unique_ptr<dap::net::Server> server;

// Unrealscript debugger does not expose separate threads. We arbitrarily name the sole
// thread we can access '1'.
static const int unreal_thread_id = 1;

Signal term;

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

    std::string class_to_source(const std::string& class_name)
    {
        auto idx = class_name.find('.');
        std::string package = class_name.substr(0, idx);
        std::string file = class_name.substr(idx + 1);

        return dev_root + package + "\\Classes\\" + file + ".uc";
    }
}

namespace handlers
{
    void error_handler(const char* msg)
    {
        if (log_enabled)
        {
            dap::writef(log_file, "Session error: %s\n", msg);
            term.fire();
        }
    }

    // Handle an initialize request. This returns debugger capabilities.
    dap::InitializeResponse initialize_handler(const dap::InitializeRequest&)
    {
        dap::InitializeResponse response;
        // Return default capabilities.
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
        if (request.threadId != unreal_thread_id)
        {
            int id = request.threadId;
            return dap::Error("Unknown thread id :%d", id);
        }

        int count = 0;
        dap::StackTraceResponse response;

        for (auto it = debugger.get_callstack().rbegin(); it != debugger.get_callstack().rend(); ++it)
        {
            dap::StackFrame frame;

            frame.id = count;

            if (count == 0)
            {
                frame.line = debugger.get_current_line();
            }
            else
            {
                frame.line = 1;
            }

            dap::Source source;
            source.path = util::class_to_source(it->class_name);
            source.name = it->class_name;

            frame.source = source;
            frame.column = 0;
            frame.name = it->function_name;
            response.stackFrames.push_back(frame);
            ++count;
        }

        return response;
    }

    // Handle a request for scope information
    dap::ResponseOrError<dap::ScopesResponse> scopes_handler(const dap::ScopesRequest& request)
    {

        // TODO fixme
        if (request.frameId != 0)
        {
            return dap::Error("Only the top frame is currently supported.");
        }

        dap::Scope scope;
        scope.name = "Locals";
        scope.presentationHint = "locals";
        scope.variablesReference = 1;
        dap::ScopesResponse response;
        response.scopes.push_back(scope);
        scope.name = "Globals";
        scope.presentationHint = {};

         // TODO Fixme
        scope.variablesReference = 0x4000'0001;
        response.scopes.push_back(scope);
        return response;
    }

    dap::ResponseOrError<dap::VariablesResponse> variables_handler(const dap::VariablesRequest& request)
    {
        Debugger::WatchKind watch_kind = (request.variablesReference & 0x4000'0000) ? Debugger::WatchKind::Global : Debugger::WatchKind::Local;
        Debugger::WatchList& watch_list = debugger.get_watch_list(watch_kind);

        int variable = (request.variablesReference & ~0x4000'0000) - 1;
        if (variable >= watch_list.size())
        {
            return dap::Error("Unknown variablesReference '%d'",
                int(request.variablesReference));
        }

        if (request.start.value(0) != 0 || request.count.value(0) != 0)
        {
            // TODO Implement chunked responses.
            return dap::Error("Debugger does not support chunked variable requests");
        }

        dap::VariablesResponse response;

        Debugger::WatchData& parent = watch_list[variable];

        for (int child_index : parent.children)
        {
            Debugger::WatchData& watch = watch_list[child_index];
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
            }
            else
            {
                int child_reference = child_index + 1;
                if (watch_kind == Debugger::WatchKind::Global)
                {
                    child_reference |= 0x4000'0000;
                }
                var.variablesReference = watch.children.empty() ? 0 : (child_index + 1);
            }
            response.variables.push_back(var);
        }

        return response;
    }

    dap::PauseResponse pause_handler(const dap::PauseRequest& request)
    {
        client::commands::break_cmd();
        return {};
    }

    dap::ContinueResponse continue_handler(const dap::ContinueRequest& request)
    {
        client::commands::go();
        return {};
    }

    dap::NextResponse next_handler(const dap::NextRequest& request)
    {
        client::commands::step_over();
        return {};
    }

    dap::StepInResponse step_in_handler(const dap::StepInRequest& request)
    {
        client::commands::step_into();
        return {};
    }

    dap::StepOutResponse step_out_handler(const dap::StepOutRequest& request)
    {
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
    dap::StoppedEvent ev;
    ev.reason = "breakpoint";
    ev.threadId = unreal_thread_id;
    session->send(ev);
}

void console_message(const std::string& msg)
{
    dap::OutputEvent ev;
    ev.output = msg;
    ev.category = "console";
    session->send(ev);
}

void create_adapter()
{
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
    term.wait();
}

void start_debug_server()
{
    server = dap::net::Server::create();
    server->start(9444, on_connect);
}

void start_debug_local()
{
    create_adapter();

    std::shared_ptr<dap::Reader> in = dap::file(stdin, false);
    std::shared_ptr<dap::Writer> out = dap::file(stdout, false);
  //  session->bind(dap::spy(in, log_file), dap::spy(out, log_file));
    session->bind(in, out);
    dap::writef(log_file, "Bound to in/out\n");
    term.wait();

}

void stop_adapter()
{
    session.release();
    server.release();
}
