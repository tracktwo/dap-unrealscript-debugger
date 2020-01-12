#include "debugger.h"



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