#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>

#include "sirenproof/platform/win.hpp"

namespace sirenproof::tray {

// What the pause shows for one program.
struct PauseView {
    std::string id;    // the service's id for this pause
    std::string rule;  // the rule's id, for "continue"
    std::wstring program;
    std::wstring rule_name;
    std::wstring goal;
    int pause_seconds = 15;
    int continue_minutes = 5;
};

// For checking the pause's budget (plan §3.7) without getting in anyone's way: it's drawn for real but
// invisible and click-through, and `first_frame` hears when its first frame is done.
struct PauseMeasuring {
    bool invisible = false;
    std::function<void()> first_frame;
    std::function<void(const char* step)> step;  // where the time goes on the way to that frame
};

// The pause for a program a Nudge rule caught: a screen over the monitor in use, with the goal, a
// countdown and two choices, close the program or go on for a few minutes. Drawn with Direct2D, and only
// redrawn while the countdown runs or the pointer moves over a button.
class PauseOverlay {
public:
    // close: true for "Close it", false for "Continue".
    using Choice = std::function<void(const PauseView& view, bool close)>;

    using Measuring = PauseMeasuring;

    PauseOverlay(HINSTANCE instance, Choice choice, Measuring measuring = Measuring());
    ~PauseOverlay();
    PauseOverlay(const PauseOverlay&) = delete;
    PauseOverlay& operator=(const PauseOverlay&) = delete;

    // Shows the pause (or swaps to another one); `waited` is how long it has been waiting already.
    void show(const PauseView& view, std::chrono::seconds waited);
    void hide();
    bool showing() const;
    const std::string& id() const;

    struct Impl;

private:
    std::unique_ptr<Impl> impl_;
};

// Renders the pause into a PNG file, `elapsed` seconds into its countdown (development and checks).
bool render_pause_png(const PauseView& view, double elapsed, int width, int height, const std::wstring& file);

}  // namespace sirenproof::tray
