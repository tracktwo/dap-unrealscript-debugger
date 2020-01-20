
#pragma once

#include <cassert>

namespace unreal_debugger::serialization
{
    namespace commands
    {
        enum class command_kind : char;
    }

    namespace events
    {
        enum class event_kind : char;
    }

    struct message
    {
        std::unique_ptr<char[]> buf_;
        int len_;
    };

    // Verify that a message has been completely serialized or deserialized:
    // the position of the raw buffer 'buf' should be msg.len_ bytes from the
    // start of msg.buf_.
    inline void verify_message(const message& msg, char* buf)
    {
        assert(msg.len_ == (buf - msg.buf_.get()));
    }

    // Serialization helpers
    inline void serialize_bool(char*& buf, bool b)
    {
        *(bool*)buf = b;
        buf += sizeof(bool);
    }

    inline void serialize_int(char*& buf, int v)
    {
        *(int*)buf = v;
        buf += sizeof(int);
    }

    inline void serialize_string(char*& buf, const std::string& str)
    {
        *(int*)buf = str.size();
        buf += sizeof(int);
        memcpy(buf, str.c_str(), str.size());
        buf += str.size();
    }

    inline void serialize_command_kind(char*& buf, commands::command_kind kind)
    {
        *(commands::command_kind*)buf = kind;
        buf += sizeof(commands::command_kind);
    }

    inline void serialize_event_kind(char*& buf, events::event_kind kind)
    {
        *(events::event_kind*)buf = kind;
        buf += sizeof(events::event_kind);
    }

    // Deserialization helpers
    inline commands::command_kind deserialize_command_kind(char*& buf)
    {
        commands::command_kind k = *(commands::command_kind*)buf;
        buf += sizeof(commands::command_kind);
        return k;
    }

    inline events::event_kind deserialize_event_kind(char*& buf)
    {
        events::event_kind k = *(events::event_kind*)buf;
        buf += sizeof(events::event_kind);
        return k;
    }

    inline int deserialize_int(char*& buf)
    {
        int val = *(int*)buf;
        buf += sizeof(int);
        return val;
    }

    inline bool deserialize_bool(char*& buf)
    {
        bool val = *(bool*)buf;
        buf += sizeof(bool);
        return val;
    }

    inline std::string deserialize_string(char*& buf)
    {
        int len = *(int*)buf;
        buf += sizeof(int);

        std::string str(buf, len);
        buf += len;

        return str;
    }
}