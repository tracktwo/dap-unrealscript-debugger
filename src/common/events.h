
#pragma once
#include <string>
#include <vector>
#include "message.h"

namespace unreal_debugger::serialization::events
{
    // Communication from unreal to the debugger adapter. This is effectively a mapping
    // of the APIs defined in the debugger interface into protobuf messages.
    // See https://docs.unrealengine.com/udk/Three/DebuggerInterface.html#Interface

    enum struct event_kind : char
    {
        show_dll_form,
        build_hierarchy,
        clear_hierarchy,
        add_class_to_hierarchy,
        lock_list,
        unlock_list,
        clear_a_watch,
        add_breakpoint,
        remove_breakpoint,
        editor_load_class,
        editor_goto_line,
        add_line_to_log,
        call_stack_clear,
        call_stack_add,
        set_current_object_name,
        terminated
    };

    struct event
    {
        event(event_kind k) : kind_{ k }
        {}

        virtual message serialize() const = 0;

        // Common serialization helper for messages with no arguments.
        message serialize_empty_message() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)    // kind field
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            verify_message(msg, raw_buf);
            return msg;
        }

        event_kind kind_;
    };

    struct show_dll_form : event
    {
        show_dll_form() : event{ event_kind::show_dll_form }
        {}

        show_dll_form(const message& msg) : event{ event_kind::show_dll_form }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::show_dll_form);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct build_hierarchy : event
    {
        build_hierarchy() : event{ event_kind::build_hierarchy }
        {}

        build_hierarchy(const message& msg) : event{ event_kind::build_hierarchy }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::build_hierarchy);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct clear_hierarchy : event
    {
        clear_hierarchy() : event{ event_kind::clear_hierarchy }
        {}

        clear_hierarchy(const message& msg) : event{ event_kind::clear_hierarchy }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::clear_hierarchy);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct add_class_to_hierarchy : event
    {
        add_class_to_hierarchy(const char* n) :
            event{ event_kind::add_class_to_hierarchy },
            class_name_{ n }
        {}

        add_class_to_hierarchy(const message& msg) : event{ event_kind::add_class_to_hierarchy }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::add_class_to_hierarchy);
            class_name_ = deserialize_string(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(class_name_) // class name string
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string class_name_;
    };

    struct clear_a_watch : event
    {
        clear_a_watch(int type) :
            event{ event_kind::clear_a_watch },
            watch_type_{type}
        {}

        clear_a_watch(const message& msg) : event{ event_kind::clear_a_watch }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::clear_a_watch);
            watch_type_ = deserialize_int(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + sizeof(int)           // watch type
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_int(raw_buf, watch_type_);
            verify_message(msg, raw_buf);
            return msg;
        }

        int watch_type_;
    };

    struct watch
    {
        watch(int parent, int assigned, const std::string& name, const std::string& value) :
            parent_index_{ parent },
            assigned_index_{ assigned },
            name_ { name },
            value_ { value }
        {}

        watch(char*& buf)
        {
            parent_index_ = deserialize_int(buf);
            assigned_index_ = deserialize_int(buf);
            name_ = deserialize_string(buf);
            value_ = deserialize_string(buf);
        }

        // Compute the serialized size of a watch
        int size() const
        {
            return sizeof(int)           // parent index
                + sizeof(int)           // assigned index
                + serialized_length(name_) // name string
                + serialized_length(value_) // value string
                ;
        }

        void serialize(char*& buf) const
        {
            serialize_int(buf, parent_index_);
            serialize_int(buf, assigned_index_);
            serialize_string(buf, name_);
            serialize_string(buf, value_);
        }

        int parent_index_;
        int assigned_index_;
        std::string name_;
        std::string value_;
    };

    struct lock_list : event
    {
        lock_list(int type) :
            event{ event_kind::lock_list },
            watch_type_{type}
        {}

        lock_list(const message& msg) : event{ event_kind::lock_list }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::lock_list);
            watch_type_ = deserialize_int(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + sizeof(int)           // watch type
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_int(raw_buf, watch_type_);
            verify_message(msg, raw_buf);
            return msg;
        }

        int watch_type_;
    };

    struct unlock_list : event
    {
        unlock_list(int type) :
            event{ event_kind::unlock_list },
            watch_type_{type}
        {}

        // The unlock list is expensive to copy due to the very large list of
        // watches it may contain.
        unlock_list(const unlock_list&) = delete;
        unlock_list(unlock_list&&) = default;

        unlock_list(const message& msg) : event{ event_kind::unlock_list }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::unlock_list);
            watch_type_ = deserialize_int(raw_buf);
            int count = deserialize_int(raw_buf);

            for (int i = 0; i < count; ++i)
            {
                watch_info_.emplace_back(raw_buf);
            }

            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + sizeof(int)           // watch type
                + sizeof(int)           // watch count
                ;

            for (const watch& w : watch_info_)
            {
                msg.len_ += w.size();
            }

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_int(raw_buf, watch_type_);
            serialize_int(raw_buf, static_cast<int>(watch_info_.size()));
            for (const watch& w : watch_info_)
            {
                w.serialize(raw_buf);
            }

            verify_message(msg, raw_buf);
            return msg;
        }

        int watch_type_;
        std::vector<watch> watch_info_;
    };

    struct add_breakpoint : event
    {
        add_breakpoint(const char* name, int line) :
            event{ event_kind::add_breakpoint },
            class_name_{ name },
            line_number_{ line }
        {}

        add_breakpoint(const message& msg) : event{ event_kind::add_breakpoint }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::add_breakpoint);
            class_name_ = deserialize_string(raw_buf);
            line_number_ = deserialize_int(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(class_name_)    // class_name string
                + sizeof(int)           // line_number
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            serialize_int(raw_buf, line_number_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string class_name_;
        int line_number_;
    };

    struct remove_breakpoint : event
    {
        remove_breakpoint(const char* name, int line) :
            event{ event_kind::remove_breakpoint },
            class_name_{ name },
            line_number_{ line }
        {}

        remove_breakpoint(const message& msg) : event{ event_kind::remove_breakpoint }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::remove_breakpoint);
            class_name_ = deserialize_string(raw_buf);
            line_number_ = deserialize_int(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(class_name_)    // class_name string
                + sizeof(int)           // line_number
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            serialize_int(raw_buf, line_number_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string class_name_;
        int line_number_;
    };

    struct editor_load_class : event
    {
        editor_load_class(const char* name) :
            event{ event_kind::editor_load_class },
            class_name_{ name }
        {}

        editor_load_class(const message& msg) : event{ event_kind::editor_load_class }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::editor_load_class);
            class_name_ = deserialize_string(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(class_name_)    // class_name string
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string class_name_;
    };

    struct editor_goto_line : event
    {
        editor_goto_line(int line, bool highlight) :
            event{ event_kind::editor_goto_line },
            line_number_{ line },
            highlight_ { highlight }
        {}

        editor_goto_line(const message& msg) : event{ event_kind::editor_goto_line }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::editor_goto_line);
            line_number_ = deserialize_int(raw_buf);
            highlight_ = deserialize_bool(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + sizeof(int)           // line number
                + sizeof(bool)          // highlight
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_int(raw_buf, line_number_);
            serialize_bool(raw_buf, highlight_);
            verify_message(msg, raw_buf);
            return msg;
        }

        int line_number_;
        bool highlight_;
    };

    struct add_line_to_log : event
    {
        add_line_to_log(const char* text) :
            event{ event_kind::add_line_to_log },
            text_{ text }
        {}

        add_line_to_log(const message& msg) : event{ event_kind::add_line_to_log }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::add_line_to_log);
            text_ = deserialize_string(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(text_)          // text string
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, text_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string text_;
    };

    struct call_stack_clear : event
    {
        call_stack_clear() : event{ event_kind::call_stack_clear }
        {}

        call_stack_clear(const message& msg) : event{ event_kind::call_stack_clear }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::call_stack_clear);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct call_stack_add : event
    {
        call_stack_add(const char* str) :
            event{ event_kind::call_stack_add },
            entry_{ str }
        {}

        call_stack_add(const message& msg) : event{ event_kind::call_stack_add }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::call_stack_add);
            entry_ = deserialize_string(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(entry_)  // entry string
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, entry_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string entry_;
    };

    struct set_current_object_name : event
    {
        set_current_object_name(const char* str) :
            event{ event_kind::set_current_object_name },
            object_name_{ str }
        {}

        set_current_object_name(const message& msg) : event{ event_kind::set_current_object_name }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::set_current_object_name);
            object_name_ = deserialize_string(raw_buf);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(event_kind)      // kind field
                + serialized_length(object_name_)          // name string
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_event_kind(raw_buf, kind_);
            serialize_string(raw_buf, object_name_);
            verify_message(msg, raw_buf);
            return msg;
        }

        std::string object_name_;
    };

    struct terminated : event
    {
        terminated() : event{ event_kind::terminated }
        {}

        terminated(const message& msg) : event{ event_kind::terminated }
        {
            char* raw_buf = msg.buf_.get();
            event_kind k = deserialize_event_kind(raw_buf);
            assert(k == event_kind::terminated);
            verify_message(msg, raw_buf);
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };
}