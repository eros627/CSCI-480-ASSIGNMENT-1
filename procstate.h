#pragma once

enum class ProcState {
    New,
    Ready,
    Running,
    WaitingSleep,
    WaitingIO,
    WaitingLock,
    WaitingEvent,
    Terminated 
};