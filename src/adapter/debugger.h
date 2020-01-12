
#include <string>
#include <vector>

class Debugger
{
public:
    enum class WatchKind : char
    {
        Local,
        Global,
        User
    };

    struct  StackFrame
    {
        StackFrame(const std::string& cls, const std::string& func) : class_name(cls), function_name(func)
        {}

        std::string class_name;
        std::string function_name;
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

    int get_current_line() const { return current_line; }
    std::string get_current_class() const { return current_class; }
    const std::vector<StackFrame>& get_callstack() { return callstack; }
    WatchList& get_watch_list(WatchKind kind);

private:

    WatchList local_watches;
    WatchList global_watches;
    WatchList user_watches;

    std::vector<StackFrame> callstack;
    std::string current_class;
    int current_line;

};

extern Debugger debugger;
