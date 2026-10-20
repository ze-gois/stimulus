#pragma once
#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace stimulus {
enum class Kind { Fixing, RandomCircles, ColoredWords };
struct Config {
    Kind kind = Kind::Fixing;
    int fps = 60;
    int duration = 5;
    int repetitions = 1;
    int seed = 0;
    int font_size = 70;
    int center_x = 400, center_y = 400;
    int count = 100, size = 5, inner_radius = 100, outer_radius = 200;
    void validate() const;
};
const char* name(Kind kind);
struct Point { double x, y; };
std::vector<Point> sample_annulus(const Config& config, std::mt19937& rng);
struct Response { int key; double session_seconds, trial_seconds; std::uint64_t frame; };
struct Trial {
    Config config;
    std::size_t sequence_index;
    int repetition;
    double onset_seconds = 0, end_seconds = 0;
    std::uint64_t frames = 0;
    std::vector<Response> responses;
};
// The caller supplies monotonic seconds. No graphics, sleeping or wall-clock assumptions.
class Session {
public:
    bool start(const std::vector<Config>& sequence, double now);
    bool advance(double now); // true when a new trial starts or the session finishes
    void respond(int key, double now);
    void frame();
    void abort(double now, std::string reason);
    bool running() const { return running_; }
    const Trial& current() const { return trials_.at(index_); }
    const std::vector<Trial>& trials() const { return trials_; }
    std::size_t index() const { return index_; }
    double elapsed(double now) const { return now - start_; }
    double trial_elapsed(double now) const { return elapsed(now) - current().onset_seconds; }
    const std::string& status() const { return status_; }
private:
    std::vector<Trial> trials_;
    std::size_t index_ = 0;
    double start_ = 0;
    bool running_ = false;
    std::string status_ = "idle";
};
}
