#pragma once

namespace ui {

struct WindowActions {
    void* context = nullptr;
    void (*minimize)(void*) = nullptr;
    void (*toggleMaximize)(void*) = nullptr;
    void (*close)(void*) = nullptr;
    void (*beginDrag)(void*) = nullptr;
};

} // namespace ui
