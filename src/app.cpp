#include "app.hpp"
#include "platform.hpp"
#include "raylib.h"
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace stimulus {
namespace {
constexpr Color ink{28, 39, 51, 255};
constexpr Color muted{82, 99, 113, 255};
constexpr Color accent{18, 107, 110, 255};
constexpr Color panel{229, 237, 240, 255};
const char* words[] = {"Gray", "Yellow", "Gold", "Orange", "Pink", "Red", "Maroon", "Green", "Lime", "Blue", "Purple", "Violet", "Beige", "Brown", "White", "Black", "Magenta"};
const Color colors[] = {GRAY, YELLOW, GOLD, ORANGE, PINK, RED, MAROON, GREEN, LIME, BLUE, PURPLE, VIOLET, BEIGE, BROWN, WHITE, BLACK, MAGENTA};
bool button(float x, float y, float width, const char* label, bool enabled = true, bool selected = false) {
    const Rectangle rect{x, y, width, 32};
    const bool hover = enabled && CheckCollisionPointRec(GetMousePosition(), rect);
    DrawRectangleRounded(rect, 0.15f, 4, !enabled ? LIGHTGRAY : selected || hover ? accent : panel);
    DrawText(label, static_cast<int>(x + 10), static_cast<int>(y + 8), 16, selected || hover ? WHITE : ink);
    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}
void field(int y, const char* label, int& value, int low, int high, int step = 1) {
    DrawText(label, 445, y + 8, 17, ink);
    DrawText(TextFormat("%d", value), 650, y + 8, 18, ink);
    if (button(605, static_cast<float>(y), 32, "-", value > low)) value = std::max(low, value - step);
    if (button(744, static_cast<float>(y), 32, "+", value < high)) value = std::min(high, value + step);
}
std::string export_name(const char* suffix) {
    const auto time = std::time(nullptr);
    std::ostringstream name;
    name << "session-" << std::put_time(std::gmtime(&time), "%Y%m%dT%H%M%SZ") << '-' << static_cast<long long>(GetTime() * 1000) << suffix;
    return name.str();
}
void heading(const char* title, const char* subtitle) {
    DrawText(title, 24, 20, 32, ink);
    DrawText(subtitle, 24, 60, 17, muted);
}
bool valid_selection(int selected, std::size_t size) { return selected >= 0 && static_cast<std::size_t>(selected) < size; }
}
App::App() {
    for (auto kind : {Kind::Fixing, Kind::RandomCircles, Kind::ColoredWords}) {
        Config config; config.kind = kind; library_.stimuli.push_back(config);
    }
    try {
        if (const auto text = load_library_text()) library_ = decode_library(*text);
        message_ = "Ready. Select a stimulus and add it to the sequence.";
    } catch (const std::exception& e) { message_ = e.what(); }
}
bool App::import_json(const std::string& text) {
    if (screen_ == Screen::Presenting) { message_ = "Stop the session before importing."; return false; }
    try {
        if (text.size() > 4 * 1024 * 1024) throw std::runtime_error("JSON exceeds 4 MiB");
        auto imported = decode_library(text); // validate before replacing any state
        library_ = std::move(imported);
        selected_ = sequence_selected_ = -1;
        library_page_ = sequence_page_ = 0;
        screen_ = Screen::Library;
        message_ = "Imported. Save to keep this library.";
        return true;
    } catch (const std::exception& e) { message_ = e.what(); return false; }
}
void App::begin_trial() {
    const auto& t = session_.current();
    // Reproducible stream per sequence item/repetition, independent of global rand().
    std::seed_seq seed{static_cast<unsigned>(t.config.seed), static_cast<unsigned>(t.sequence_index), static_cast<unsigned>(t.repetition)};
    rng_.seed(seed);
    last_tick_ = -1;
    word_ = static_cast<int>(rng_() % 17); color_ = static_cast<int>(rng_() % 17);
#ifndef __EMSCRIPTEN__
    SetTargetFPS(t.config.fps);
#endif
}
void App::pick(const Config& c, double elapsed) {
    const auto tick = static_cast<long long>(elapsed * c.fps);
    if (c.kind == Kind::RandomCircles && tick != last_tick_) {
        points_ = sample_annulus(c, rng_);
        last_tick_ = tick;
    }
}
void App::draw_stimulus(const Config& c) {
    if (c.kind == Kind::RandomCircles) {
        for (const auto& p : points_) DrawCircle(static_cast<int>(400 + p.x), static_cast<int>(400 + p.y), static_cast<float>(c.size), BLACK);
    } else {
        const auto text = c.kind == Kind::Fixing ? "+" : words[word_];
        const int x = c.kind == Kind::Fixing ? c.center_x : 400;
        const int y = c.kind == Kind::Fixing ? c.center_y : 400;
        DrawText(text, x - MeasureText(text, c.font_size) / 2, y - c.font_size / 2, c.font_size,
                 c.kind == Kind::Fixing ? DARKGRAY : colors[color_]);
    }
}
void App::finish() {
    screen_ = Screen::Report;
    SetTargetFPS(60);
    ShowCursor();
    message_ = "Session finished. Export JSON for trial details, CSV for responses.";
}
void App::tick() {
    // Input is handled before drawing. Exactly one BeginDrawing/EndDrawing pair per callback.
    if (screen_ == Screen::Presenting) {
        const double now = GetTime();
        if (!presentation_focused()) { session_.abort(now, "aborted_focus_loss"); finish(); }
        else if (IsKeyPressed(KEY_ESCAPE)) { session_.abort(now, "aborted_escape"); finish(); }
        else {
            // Keys polled here belong to the frame displayed before this boundary.
            for (int key = GetKeyPressed(); key != 0; key = GetKeyPressed()) session_.respond(key, now);
            if (session_.advance(now)) {
                if (session_.running()) begin_trial(); else finish();
            }
            if (session_.running()) pick(session_.current().config, session_.trial_elapsed(now));
        }
    }
    BeginDrawing();
    ClearBackground(RAYWHITE);
    try {
        switch (screen_) {
        case Screen::Library: library_screen(); break;
        case Screen::Editor: editor_screen(); break;
        case Screen::Report: report_screen(); break;
        case Screen::Presenting: draw_stimulus(session_.current().config); session_.frame(); break;
        }
    } catch (const std::exception& e) { message_ = e.what(); }
    if (screen_ != Screen::Presenting) {
        DrawRectangle(0, 748, 800, 52, panel);
        DrawText(message_.substr(0, 98).c_str(), 16, 765, 15, ink);
    }
    EndDrawing();
}
void App::library_screen() {
    heading("Stimulus", "Build a sequence, edit its stimuli, then present. Escape stops a session.");
    DrawText("LIBRARY", 24, 112, 18, accent);
    DrawText("EXPERIMENT SEQUENCE", 414, 112, 18, accent);
    auto list = [](const std::vector<Config>& items, int& selection, int& page, int x) {
        const int pages = std::max(1, (static_cast<int>(items.size()) + 9) / 10);
        page = std::clamp(page, 0, pages - 1);
        for (int row = 0; row < 10; ++row) {
            const int i = page * 10 + row;
            if (i >= static_cast<int>(items.size())) break;
            const auto& c = items[static_cast<std::size_t>(i)];
            const std::string label = std::to_string(i + 1) + ". " + name(c.kind) + "  " + std::to_string(c.duration) + "s x" + std::to_string(c.repetitions);
            if (button(static_cast<float>(x), static_cast<float>(145 + row * 38), 360, label.c_str(), true, selection == i)) selection = i;
        }
        if (items.empty()) DrawText("No items yet", x + 12, 170, 18, muted);
        if (button(static_cast<float>(x), 535, 80, "Prev", page > 0)) --page;
        DrawText(TextFormat("%d / %d", page + 1, pages), x + 150, 543, 18, ink);
        if (button(static_cast<float>(x + 280), 535, 80, "Next", page + 1 < pages)) ++page;
    };
    list(library_.stimuli, selected_, library_page_, 24);
    list(library_.sequence, sequence_selected_, sequence_page_, 414);
    const bool selected = valid_selection(selected_, library_.stimuli.size());
    const bool seq = valid_selection(sequence_selected_, library_.sequence.size());
    if (button(24, 586, 110, "New")) {
        selected_ = -1; draft_ = Config{}; preview_start_ = GetTime(); last_tick_ = -1; screen_ = Screen::Editor;
    }
    if (button(146, 586, 110, "Edit", selected)) {
        draft_ = library_.stimuli[static_cast<std::size_t>(selected_)]; preview_start_ = GetTime(); last_tick_ = -1; screen_ = Screen::Editor;
    }
    if (button(268, 586, 116, "Add >", selected && library_.sequence.size() < 1000)) library_.sequence.push_back(library_.stimuli[static_cast<std::size_t>(selected_)]);
    if (button(414, 586, 110, "Remove", seq)) {
        library_.sequence.erase(library_.sequence.begin() + sequence_selected_); sequence_selected_ = -1;
    }
    if (button(536, 586, 110, "Move up", seq && sequence_selected_ > 0)) {
        std::swap(library_.sequence[sequence_selected_], library_.sequence[sequence_selected_ - 1]); --sequence_selected_;
    }
    if (button(658, 586, 116, "Start", !library_.sequence.empty())) {
        // Clear navigation keys so they cannot become experimental responses.
        while (GetKeyPressed() != 0) {}
        reset_focus_tracking();
        session_.start(library_.sequence, GetTime());
        begin_trial(); screen_ = Screen::Presenting; HideCursor();
        // Replace this menu frame with the first trial, matching the recorded boundary.
        ClearBackground(RAYWHITE); pick(session_.current().config, 0); draw_stimulus(session_.current().config); session_.frame();
        return;
    }
    if (button(24, 634, 170, "Save library")) { save_library_text(encode_library(library_)); message_ = "Library and sequence saved."; }
    if (button(206, 634, 178, "Export library")) { export_text("stimulus-library.json", encode_library(library_)); message_ = "Library exported."; }
    if (button(414, 634, 170, "Import JSON")) import_json(read_import_text());
    if (button(596, 634, 178, "Delete selected", selected)) {
        library_.stimuli.erase(library_.stimuli.begin() + selected_); selected_ = -1;
        message_ = "Removed from library. Sequence copies are unchanged. Save to persist.";
    }
    DrawText("Sequence entries are independent copies of library settings.", 24, 695, 17, muted);
}
void App::editor_screen() {
    heading("Stimulus editor", "Adjust values with - / +. Apply changes to the library; add a fresh copy to the sequence.");
    if (button(24, 105, 118, "Fixation", true, draft_.kind == Kind::Fixing)) draft_.kind = Kind::Fixing;
    if (button(150, 105, 118, "Circles", true, draft_.kind == Kind::RandomCircles)) draft_.kind = Kind::RandomCircles;
    if (button(276, 105, 118, "Words", true, draft_.kind == Kind::ColoredWords)) draft_.kind = Kind::ColoredWords;
    // Preview uses the same renderer, clipped and scaled to its own viewport.
    BeginScissorMode(24, 165, 370, 370);
    Camera2D camera{{24, 165}, {0, 0}, 0, 370.0f / 800.0f};
    BeginMode2D(camera);
    pick(draft_, GetTime() - preview_start_); draw_stimulus(draft_);
    EndMode2D(); EndScissorMode();
    DrawRectangleLines(24, 165, 370, 370, muted);
    if (button(24, 552, 170, "Resample")) { word_ = static_cast<int>(rng_() % 17); color_ = static_cast<int>(rng_() % 17); last_tick_ = -1; }
    field(105, "Duration (s)", draft_.duration, 1, 3600);
    field(145, "Repetitions", draft_.repetitions, 1, 100);
    field(185, "Update Hz", draft_.fps, 1, 240);
    field(225, "Random seed", draft_.seed, 0, 1000000);
    if (draft_.kind == Kind::RandomCircles) {
        field(285, "Point count", draft_.count, 1, 2000, 10);
        field(325, "Point radius", draft_.size, 1, 100);
        field(365, "Inner radius", draft_.inner_radius, 0, draft_.outer_radius, 5);
        field(405, "Outer radius", draft_.outer_radius, draft_.inner_radius, 400, 5);
    } else {
        field(285, "Font size", draft_.font_size, 8, 200, 2);
        if (draft_.kind == Kind::Fixing) {
            field(325, "Center X", draft_.center_x, 0, 800, 10);
            field(365, "Center Y", draft_.center_y, 0, 800, 10);
        }
    }
    DrawText("Coordinates use an 800 x 800 canvas.", 445, 470, 16, muted);
    DrawText("Words stay fixed for each repetition.", 445, 497, 16, muted);
    if (button(445, 552, 160, "Apply")) {
        draft_.validate();
        if (valid_selection(selected_, library_.stimuli.size())) library_.stimuli[selected_] = draft_;
        else {
            if (library_.stimuli.size() >= 1000) throw std::runtime_error("Library limit: 1000 items");
            library_.stimuli.push_back(draft_); selected_ = static_cast<int>(library_.stimuli.size()) - 1;
        }
        screen_ = Screen::Library; message_ = "Applied. Save library to persist changes.";
    }
    if (button(617, 552, 160, "Cancel")) screen_ = Screen::Library;
}
void App::report_screen() {
    heading("Session report", "Software timestamps are in seconds; physical display onset is not measured.");
    DrawText(session_.status().c_str(), 24, 125, 26, accent);
    std::size_t responses = 0;
    for (const auto& trial : session_.trials()) responses += trial.responses.size();
    DrawText(TextFormat("Trials recorded: %d", static_cast<int>(session_.trials().size())), 24, 180, 22, ink);
    DrawText(TextFormat("Responses: %d", static_cast<int>(responses)), 24, 220, 22, ink);
    DrawText("JSON includes configurations, repetitions, frame counts and timing.", 24, 290, 18, muted);
    DrawText("CSV contains one row per key response. Indices start at zero.", 24, 322, 18, muted);
    if (button(24, 400, 220, "Export session JSON")) export_text(export_name(".json"), encode_session(session_));
    if (button(264, 400, 220, "Export responses CSV")) export_text(export_name(".csv"), encode_responses_csv(session_));
    if (button(504, 400, 220, "Back to library")) screen_ = Screen::Library;
}
void App::close() {
    if (session_.running()) session_.abort(GetTime(), "aborted_window_close");
}
}
