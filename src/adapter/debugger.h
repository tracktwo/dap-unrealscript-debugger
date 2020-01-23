
#include <string>
#include <vector>

namespace unreal_debugger::client
{

class Debugger
{
public:

    enum class WatchKind
    {
        Local,
        Global,
        User
    };

    Debugger()
    {
        // Ensure we always have one element in the call stack to represent the top-most frame.
        callstack.resize(1);
    }

    enum class State
    {
        normal,
        busy,
        waiting_for_frame_line,
        waiting_for_frame_watches,
        waiting_for_user_watches
    };

    // Watch lists
    struct WatchData
    {
        WatchData(const std::string& n, const std::string& t, const std::string& v, int p) :
            name(n), type(t), value(v), parent(p)
        {}

        std::string name;
        std::string type;
        std::string value;
        int parent;
        std::vector<int> children;
    };

    using WatchList = std::vector<WatchData>;

    struct  StackFrame
    {
        StackFrame() = default;
        StackFrame(const std::string& cls, const std::string& func) : class_name(cls), function_name(func)
        {}

        WatchList& get_watches(WatchKind kind);

        std::string class_name = "";
        int line_number = 0;
        std::string function_name = "";
        WatchList local_watches;
        WatchList global_watches;
        WatchList user_watches;
        bool fetched_watches = false;
    };


    std::vector<StackFrame>& get_callstack() { return callstack; }
    void clear_watch(WatchKind kind);
    void add_watch(WatchKind kind, int index, int parent, const std::string& name, const std::string& value);

    void lock_list(WatchKind kind);
    void unlock_list(WatchKind kind);

    void clear_callstack();
    void add_callstack(const std::string& name);
    int get_current_frame_index() const;
    void set_current_frame_index(int frame);
    StackFrame& get_current_stack_frame();
    void finalize_callstack();
    void set_state(State s) { state = s; }
    State get_state() const { return state; }
    int find_user_watch(int frame_index, const std::string& var_name) const;

private:

    std::vector<StackFrame> callstack;
    int current_frame = 0;
    std::atomic<State> state;
    int watch_lock_depth = 0;
};

extern Debugger debugger;

}

