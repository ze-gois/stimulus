#pragma once
#include "model.hpp"
#include "storage.hpp"
#include <random>
#include <string>

namespace stimulus {
class App {
public:
    App();
    void tick();
    bool import_json(const std::string& text);
    void close();
private:
    enum class Screen { Library, Editor, Presenting, Report };
    Screen screen_ = Screen::Library;
    Library library_;
    Config draft_;
    int selected_ = -1, sequence_selected_ = -1;
    int library_page_ = 0, sequence_page_ = 0;
    std::string message_;
    Session session_;
    std::mt19937 rng_{0};
    std::vector<Point> points_;
    int word_ = 0, color_ = 0;
    long long last_tick_ = -1;
    double preview_start_ = 0;
    void library_screen();
    void editor_screen();
    void report_screen();
    void begin_trial();
    void pick(const Config& config, double elapsed);
    void draw_stimulus(const Config& config);
    void finish();
};
}
