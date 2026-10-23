#include "sharekeys/queue.hpp"
#include "sharekeys/win32_keyboard.hpp"
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

// Explicitly opt in on an interactive test desktop. Not part of default CTest.
int main(int argc, char** argv) {
    using namespace sharekeys;
    if (argc != 2 || std::string_view(argv[1]) != "--allow-input") {
        std::cerr << "Requires --allow-input: injects F24 and observes its low-level hook events.\n"; return 2;
    }
    try {
        SpscQueue<KeyboardEvent, 32> events;
        KeyboardHook hook([&](const auto& event) { if (event.key.vk == VK_F24) events.push(event); return false; }, true);
        Win32KeyboardSink sink(hook); InjectedKeyboard keyboard(sink);
        if (!keyboard.apply({{0, VK_F24, false}, true, true, 1}) || !keyboard.releaseAll()) { std::cerr << "SendInput rejected input\n"; return 1; }
        bool down{}, up{};
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < deadline && !up) {
            KeyboardEvent event;
            while (events.pop(event)) {
                if (!event.injected) continue;
                if (event.down) down = true; else if (down) up = true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!down || !up) { std::cerr << "Hook did not observe ordered injected down/up\n"; return 1; }
        std::cout << "PASS: SendInput accepted F24 and hook observed injected down/up. This does not test physical capture or application execution.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
