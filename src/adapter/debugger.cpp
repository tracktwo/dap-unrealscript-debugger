
#include "dap/io.h"
#include "client.h"
#include "debugger.h"
#include "signals.h"

#include <boost/algorithm/string.hpp>

namespace unreal_debugger::client
{

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
    log("Failed to parse type: %s\n", full_name.c_str());
    return { "<unknown name>", "<unknown type>" };
}

watch_list& stack_frame::get_watches(watch_kind kind)
{
    return const_cast<watch_list&>(const_cast<const stack_frame*>(this)->get_watches(kind));
}

const watch_list& stack_frame::get_watches(watch_kind kind) const
{
    switch (kind)
    {
    case watch_kind::local: return local_watches;
    case watch_kind::global: return global_watches;
    case watch_kind::user: return user_watches;
    default:
        abort();
    }
}

// Clear a watch list. For locals and globals they are associated with the current
// stack frame. User watches are part of the debugger state independent of frame.
void debugger_state::clear_watch(watch_kind kind)
{
    callstack_[current_frame_].get_watches(kind).clear();
    callstack_[current_frame_].get_watches(kind).emplace_back("ROOT", "N/A", "N/A", -1);
}

// Ensure there is enough space in the watch list to hold all the watches we are going to add without needing to
// repeatedly reallocate the vector.
void debugger_state::reserve_watch_size(watch_kind kind, std::size_t size)
{
    watch_list& list = callstack_[current_frame_].get_watches(kind);
    list.reserve(size);
}

void debugger_state::add_watch(watch_kind kind, int index, int parent, const std::string& full_name, const std::string& value)
{
    watch_list& list = callstack_[current_frame_].get_watches(kind);

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

    // Parse the watch 'name', which actually includes name info, type info, and address (currently address is not used and is discarded).
    auto [name, type] = split_watch_name(full_name);

    // Insert a new entry for this watch into the list. We must be inserting at the back and should get the watches in order.
    assert(list.size() == index);

    list.emplace_back(name, type, value, parent);

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

void debugger_state::lock_list(watch_kind kind)
{
    ++watch_lock_depth_;
}

void debugger_state::unlock_list(watch_kind kind)
{
    --watch_lock_depth_;

    // If we have just unlocked the last watch list then we are done receiving watches. Signal
    // that they are available if the debugger is waiting for some watch list to complete.
    if (watch_lock_depth_ == 0)
    {
        if (state_ == state::waiting_for_frame_watches)
        {
            signals::watches_received.fire();
        }
        else if (state_ == state::waiting_for_user_watches)
        {
            signals::user_watches_received.fire();
        }
    }
}


// "clear" the callstack. Due to the order Unreal provides information we don't want to just delete
// any existing callstack: after breaking at a breakpoint unreal sends the current class name, current
// line number, and all watches before clearing and sending call stack information. For DAP we want to
// have line and variable information for stacks other than the top-most, so we store all watch info
// in the stack frame data structure. So, we always want to have at least 1 element in the call stack
// at all times, and by the time we receive the command to clear the call stack we've already received
// all the useful info for the top-most entry and don't want to have to throw it away and re-fetch it.
//
// When we get the 'clear' signal, remove all stack entries _except_ the first one. We have already
// stored the class name, line number, and watches for this one, and they should have been reset into
// this element overwriting whatever was there before. We do need to set a flag indicating we've just
// cleared the stack, though, so we can recognize the first add_callstack event we receive.
void debugger_state::clear_callstack()
{
    callstack_.resize(1);
}

void debugger_state::add_callstack(const std::string& full_name)
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
            log("Found unknown call stack kind %s\n", full_name.c_str());
        }
        name = name.substr(idx + 1);
    }

    idx = name.find(':');

    std::string class_name = idx > 0 ? name.substr(0, idx) : name;
    std::string function_name = idx > 0 ? name.substr(idx + 1) : "";
    callstack_.emplace_back(class_name, function_name);
}

void debugger_state::set_current_frame_index(int frame)
{
    current_frame_ = frame;
}

int debugger_state::get_current_frame_index() const
{
    return current_frame_;
}

const stack_frame& debugger_state::get_current_stack_frame() const
{
    return callstack_[current_frame_];
}

stack_frame& debugger_state::get_current_stack_frame()
{
    return callstack_[current_frame_];
}

// Unreal indexes the callstack with the top-most frame as id 0, and sends the frames
// starting from bottom up. When we build the internal vector for the callstack the frames
// are pushed onto the back in the order they're received, so we wind up with the frames
// in reverse order of how unreal numbers them. DAP also wants to receive the frames with the
// top-most as id 0.
//
// Unreal also sends some info before the stack: we get the current line number and class name
// for a breakpoint that is hit before the call stack is cleared and reset. This line number and
// class name logically belongs with the topmost frame, but we need to be careful to keep it
// when unreal later clears the stack. This is done by storing the line number and class name in
// entry 0, and "clearing" the stack removes all entries except the first.
//
// Once the callstack is complete, we need to copy the saved 
void debugger_state::finalize_callstack()
{
    // The bottom-most and top-most entries on the current call stack are the same entry, but
    // both are incomplete: only the bottom has the line number, and only the top has the function
    // name.
    stack_frame& bottom_frame = *callstack_.begin();
    stack_frame& top_frame = *callstack_.rbegin();

    // Copy the line number to the top-most frame.
    top_frame.line_number = bottom_frame.line_number;

    // Move the watch info to the top-most frame.
    std::swap(top_frame.local_watches, bottom_frame.local_watches);
    std::swap(top_frame.global_watches, bottom_frame.global_watches);

    // Reverse the call stack so our 0th index is the top-most entry
    std::reverse(callstack_.begin(), callstack_.end());
   
    // pop off the now redundant duplicated entry we have on the end of the stack.
    // This leaves the stack with index 0 as the top-most entry, and with complete info.
    callstack_.pop_back();

    callstack_[0].fetched_watches = true;
}

int debugger_state::find_user_watch(int frame_index, const std::string& var_name) const
{
    if (frame_index >= callstack_.size())
    {
        log("Error: Requested user watch %s for invalid frame %d\n", var_name.c_str(), frame_index);
        return -1;
    }

    const watch_list& user_watches = callstack_[frame_index].user_watches;

    if (user_watches.empty() || user_watches[0].children.empty())
        return -1;

    for (int i = 0; i < user_watches[0].children.size(); ++i)
    {
        if (user_watches[user_watches[0].children[i]].name == var_name)
        {
            return user_watches[0].children[i];
        }
    }

    return -1;
}

void debugger_state::add_breakpoint(const std::string& class_name, int line)
{
    std::string upcase = boost::algorithm::to_upper_copy(class_name);

    if (auto it = breakpoints_.find(upcase); it != breakpoints_.end())
    {
        // We already have at least one breakpoint in this file. Add another to the list,
        // ensuring no duplicates. This could be more efficient if we kept the list sorted,
        // but the set_breakpoints message would then need some way to be able to figure out
        // which was the last added breakpoint.
        const std::vector<int>& vec = it->second;
        if (std::find(vec.begin(), vec.end(), line) == vec.end())
        {
            it->second.push_back(line);
        }
    }
    else
    {
        // This is the first breakpoint for this file.
        std::vector<int> vec;
        vec.push_back(line);
        breakpoints_[upcase] = std::move(vec);
    }
}

const std::vector<int>* debugger_state::get_breakpoints(const std::string& class_name) const
{
    std::string upcase = boost::algorithm::to_upper_copy(class_name);
    if (auto it = breakpoints_.find(upcase); it != breakpoints_.end())
    {
        return &it->second;
    }

    return nullptr;
}

// Remove all breakpoints mapped in the given class.
void debugger_state::remove_breakpoints(const std::string& class_name)
{
    std::string upcase = boost::algorithm::to_upper_copy(class_name);
    if (auto it = breakpoints_.find(upcase); it != breakpoints_.end())
    {
        breakpoints_.erase(it);
    }
}

}
