#pragma once

enum class ProcState {
    New,
    Ready,
    Running,
    WaitingSleep,
    WaitingIO,
    Terminated
};