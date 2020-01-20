#pragma once

#include <string>
#include "message.h"

namespace unreal_debugger::serialization::commands
{
    // Communication from the debugger adapter to unreal. This is effectively a mapping
    // of the commands defined in the "The Callback" section of the debugger interface.
    // https://docs.unrealengine.com/udk/Three/DebuggerInterface.html#The%20Callback

    enum class command_kind : char
    {
        add_breakpoint,
        remove_breakpoint,
        add_watch,
        remove_watch,
        clear_watch,
        change_stack,
        set_data_watch,
        break_on_none,
        break_cmd,
        stop_debugging,
        go,
        step_into,
        step_over,
        step_out_of,
        toggle_watch_info
    };

    struct command
    {
        command(command_kind k) : kind_{ k }
        {}

        virtual message serialize() const = 0;

        // Common serialization helper for messages with no arguments.
        message serialize_empty_message() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            return msg;
        }

        command_kind kind_;
    };

    struct add_breakpoint : command
    {
        add_breakpoint(const std::string& cls, int ln) :
            command{ command_kind::add_breakpoint },
            class_name_{ cls },
            line_number_{ ln }
        {}

        add_breakpoint(const message& msg) : command{ command_kind::add_breakpoint }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::add_breakpoint);

            class_name_ = deserialize_string(raw_buf);
            line_number_ = deserialize_int(raw_buf);

            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)        // class name length
                + class_name_.size()    // class name contents
                + sizeof(int)           // line number
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            serialize_int(raw_buf, line_number_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        std::string class_name_;
        int line_number_;
    };

    struct remove_breakpoint : command
    {
        remove_breakpoint() : command{ command_kind::remove_breakpoint }
        {}

        remove_breakpoint(const message& msg) : command{ command_kind::remove_breakpoint }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::remove_breakpoint);
            class_name_ = deserialize_string(raw_buf);
            line_number_ = deserialize_int(raw_buf);

            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)        // class name length
                + class_name_.size()    // class name contents
                + sizeof(int)           // line number
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_string(raw_buf, class_name_);
            serialize_int(raw_buf, line_number_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        std::string class_name_;
        int line_number_;
    };

    struct add_watch : command
    {
        add_watch(const std::string& n)
            : command{ command_kind::add_watch },
            var_name_{ n }
        {}

        add_watch(const message& msg) : command{ command_kind::add_watch }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::add_watch);
            var_name_ = deserialize_string(raw_buf);

            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)        // var name length
                + var_name_.size()    // var name contents
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_string(raw_buf, var_name_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        std::string var_name_;
    };

    struct remove_watch : command
    {
        remove_watch() : command{ command_kind::remove_watch }
        {}

        remove_watch(const message& msg) : command{ command_kind::remove_watch }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::remove_watch);
            var_name_ = deserialize_string(raw_buf);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)        // var name length
                + var_name_.size()    // var name contents
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_string(raw_buf, var_name_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        std::string var_name_;
    };

    struct clear_watch : command
    {
        clear_watch() : command{ command_kind::clear_watch }
        {}

        clear_watch(const message& msg) : command{ command_kind::clear_watch }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::clear_watch);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct change_stack : command
    {
        change_stack(int id) :
            command{ command_kind::change_stack },
            stack_id_{ id }
        {}

        change_stack(const message& msg) : command{ command_kind::change_stack }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::change_stack);
            stack_id_ = deserialize_int(raw_buf);

            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)           // stack id
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_int(raw_buf, stack_id_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        int stack_id_;
    };

    struct set_data_watch : command
    {
        set_data_watch(const std::string& v) :
            command{ command_kind::set_data_watch },
            var_name_{ v }
        {}

        set_data_watch(const message& msg) : command{ command_kind::set_data_watch }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::set_data_watch);
            var_name_ = deserialize_string(raw_buf);

            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(int)        // var name length
                + var_name_.size()    // var name contents
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_string(raw_buf, var_name_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        std::string var_name_;
    };

    struct break_on_none : command
    {
        break_on_none(bool b)
            : command{ command_kind::break_on_none },
            break_value_{ b }
        {}

        break_on_none(const message& msg) : command{ command_kind::break_on_none }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::break_on_none);
            break_value_ = deserialize_bool(raw_buf);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(bool)          // flag length
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_bool(raw_buf, break_value_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        bool break_value_;
    };

    struct break_cmd : command
    {
        break_cmd() : command{ command_kind::break_cmd }
        {}

        break_cmd(const message& msg) : command{ command_kind::break_cmd }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::break_cmd);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct stop_debugging : command
    {
        stop_debugging() : command{ command_kind::stop_debugging }
        {}

        stop_debugging(const message& msg) : command{ command_kind::stop_debugging }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::stop_debugging);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct go : command
    {
        go() : command{ command_kind::go }
        {}

        go(const message& msg) : command{ command_kind::go }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::go);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct step_into : command
    {
        step_into() : command{ command_kind::step_into }
        {}

        step_into(const message& msg) : command{ command_kind::step_into }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::step_into);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct step_over : command
    {
        step_over() : command{ command_kind::step_over }
        {}

        step_over(const message& msg) : command{ command_kind::step_over }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::step_over);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct step_out_of : command
    {
        step_out_of() : command{ command_kind::step_out_of }
        {}

        step_out_of(const message& msg) : command{ command_kind::step_out_of }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::step_out_of);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            return serialize_empty_message();
        }
    };

    struct toggle_watch_info : command
    {
        toggle_watch_info(bool b) :
            command{ command_kind::toggle_watch_info },
            send_watch_info_{ b }
        {}

        toggle_watch_info(const message& msg) : command{ command_kind::toggle_watch_info }
        {
            char* raw_buf = msg.buf_.get();

            command_kind k = deserialize_command_kind(raw_buf);
            assert(k == command_kind::toggle_watch_info);
            send_watch_info_ = deserialize_bool(raw_buf);
            assert(msg.len_ == (raw_buf - msg.buf_.get()));
        }

        virtual message serialize() const
        {
            message msg;
            msg.len_ =
                sizeof(command_kind)    // kind field
                + sizeof(bool)          // flag length
                ;

            msg.buf_ = std::make_unique<char[]>(msg.len_);
            char* raw_buf = msg.buf_.get();
            serialize_command_kind(raw_buf, kind_);
            serialize_bool(raw_buf, send_watch_info_);
            assert(msg.len_ == raw_buf - msg.buf_.get());
            return msg;
        }

        bool send_watch_info_;
    };

}
