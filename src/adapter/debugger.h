
#include <string>
#include <vector>
#include <map>

namespace unreal_debugger::client
{

enum class watch_kind
{
    local,
    global,
    user
};

// Watch lists
struct watch_data
{
    watch_data(const std::string& n, const std::string& t, const std::string& v, int p) :
        name(n), type(t), value(v), parent(p)
    {}

    std::string name;
    std::string type;
    std::string value;
    int parent;
    std::vector<int> children;
};

using watch_list = std::vector<watch_data>;

struct stack_frame
{
    stack_frame() = default;
    stack_frame(const std::string& cls, const std::string& func) : class_name(cls), function_name(func)
    {}

    const watch_list& get_watches(watch_kind kind) const;
    watch_list& get_watches(watch_kind kind);

    std::string class_name = "";
    int line_number = 0;
    std::string function_name = "";
    watch_list local_watches;
    watch_list global_watches;
    watch_list user_watches;
    bool fetched_watches = false;
};

class debugger_state
{
public:

    enum class state
    {
        normal,
        waiting_for_frame_line,
        waiting_for_frame_watches,
        waiting_for_user_watches,
        waiting_for_add_breakpoint
    };

    debugger_state()
    {
        // Ensure we always have one element in the call stack to represent the top-most frame.
        callstack_.resize(1);
    }

    void clear_watch(watch_kind kind);
    void reserve_watch_size(watch_kind kind, std::size_t size);
    void add_watch(watch_kind kind, int index, int parent, const std::string& name, const std::string& value);

    void lock_list(watch_kind kind);
    void unlock_list(watch_kind kind);

    void clear_callstack();
    void add_callstack(const std::string& name);
    int get_current_frame_index() const;
    void set_current_frame_index(int frame);
    const stack_frame& get_current_stack_frame() const;
    stack_frame& get_current_stack_frame();
    const stack_frame& get_stack_frame(int idx) const { return callstack_[idx]; }
    size_t callstack_size() const { return callstack_.size(); }

    void add_breakpoint(const std::string& class_name, int line);
    void remove_breakpoints(const std::string& class_name);
    const std::vector<int>* get_breakpoints(const std::string& class_name) const;

    void finalize_callstack();
    void set_state(state s) { state_ = s; }
    state get_state() const { return state_; }
    int find_user_watch(int frame_index, const std::string& var_name) const;

private:

    std::vector<stack_frame> callstack_;
    int current_frame_ = 0;
    std::atomic<state> state_;
    int watch_lock_depth_ = 0;

    // A map from class name to a list of line numbers representing the breakpoints in this file.
    // Note that unreal provides breakpoint info with the class names in all uppercase, so this map
    // always contains upcased strings.
    std::map<std::string, std::vector<int>> breakpoints_;
};

extern debugger_state debugger;

}

