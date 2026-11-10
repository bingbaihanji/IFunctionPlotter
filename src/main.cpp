#include "platform/App.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    platform::App application;
    return application.Run(instance);
}
