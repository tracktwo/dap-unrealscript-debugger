
#include "dap/io.h"
#include "client.h"
#include "debugger.h"

// Unreal watch names are of the form "VarName ( Type, Address )"
// Split out and return the 'name' and 'type' portions.
static std::pair<std::string, std::string> split_watch_name(std::string full_name)
{
    // The name extends to the first '('
    auto idx = full_name.find('(');

    // Grab the name. There is a space preceding the '(' we should skip.
    if (idx >= 2)
    {
        std::string name = idx >= 2 ? full_name.substr(0, idx - 1) : "";

        // Move past the '(' and the space that follows it
        full_name = full_name.substr(idx + 2);

        // Find the ',' separating the type from the address.
        auto comma = full_name.find(',');
        if (comma >= 0)
        {
            std::string type = full_name.substr(0, comma);
            return { name, type };
        }
    }
    // Failed to parse the type
    dap::writef(log_file, "Failed to parse type: %s\n", full_name.c_str());
    return { "<unknown name>", "<unknown type>" };
}

Debugger::WatchList& Debugger::get_watch_list(WatchKind kind)
{
    switch (kind)
    {
    case WatchKind::Local: return local_watches;
    case WatchKind::Global: return global_watches;
    case WatchKind::User: return user_watches;
    default:
        // Should be unreachable
        abort();
    }
}


void Debugger::clear_watch(WatchKind kind)
{
    get_watch_list(kind).clear();
}

void Debugger::add_watch(WatchKind kind, int index, int parent, const std::string& full_name, const std::string& value)
{
    WatchList& list = get_watch_list(kind);

    // Ensure we have a root element before adding anything more. The root element is at index 0.
    if (list.empty())
    {
        // Insert a dummy root value with no type or value.
        list.emplace_back("ROOT", "N/A", "N/A", -1);
    }

    // Watch elements from the debug interface will be inserted into the watchlist data structure at the positions they were assigned by the
    // debugger interface. Make sure the list is big enough to hold this element.
    if (list.capacity() <= index)
    {
        list.reserve(list.capacity() * 2);
    }

    if (list.size() <= index)
    {
        list.resize(index + 1, { "<unknown>", "<unknown>", "<unknown>", -1 });
    }


    // Parse the watch 'name', which actually includes name info, type info, and address (currently address is not used and is discarded).
    auto [name, type] = split_watch_name(full_name);

    // Insert a new entry for this watch into the list.
    list[index] = { name, type, value, parent };

    // If this has a parent, add this element to the parent's children list for easy access
    if (parent >= 1)
    {
        list[parent].children.push_back(index);
    }
    else if (parent == -1)
    {
        // This is a top-level item, add it to root node's children list.
        list[0].children.push_back(index);
    }
}

void Debugger::clear_callstack()
{
    callstack.clear();
}

void Debugger::add_callstack(const std::string& full_name)
{
    // Callstack entries are of the form "Kind ClassName:FunctionName" (for Kind == Function).
    // The "Kind" is not of any real use for the DAP so we just strip it. It's unclear yet if
    // there are kinds other than "Function".

    // Skip over the kind
    std::string name = full_name;

    auto idx = name.find(' ');
    if (idx >= 0)
    {
        std::string kind = name.substr(0, idx);
        if (kind != "Function")
        {
            dap::writef(log_file, "Found unknown call stack kind %s\n", full_name.c_str());
        }
        name = name.substr(idx + 1);
    }

    idx = name.find(':');
    if (idx > 0)
    {
        callstack.emplace_back(name.substr(0, idx), name.substr(idx + 1));
    }
    else
    {
        dap::writef(log_file, "No function name in call stack %s\n", full_name.c_str());
        callstack.emplace_back(name, "");
    }
}

