#include "model.hpp"
#include <cmath>
#include <stdexcept>
#include <utility>

namespace stimulus {
const char* name(Kind kind) {
    switch (kind) {
    case Kind::Fixing: return "Fixing";
    case Kind::RandomCircles: return "RandomCircles";
    case Kind::ColoredWords: return "ColoredWords";
    }
    throw std::invalid_argument("Unknown stimulus type");
}
void Config::validate() const {
    (void)name(kind);
    if (fps < 1 || fps > 240 || duration < 1 || duration > 3600 ||
        repetitions < 1 || repetitions > 100 || seed < 0 ||
        font_size < 8 || font_size > 200 || center_x < 0 || center_x > 800 ||
        center_y < 0 || center_y > 800 || count < 1 || count > 2000 ||
        size < 1 || size > 100 || inner_radius < 0 || outer_radius < inner_radius || outer_radius > 400)
        throw std::invalid_argument("Configuration outside supported ranges");
}
std::vector<Point> sample_annulus(const Config& c, std::mt19937& rng) {
    c.validate();
    std::uniform_real_distribution<double> uniform(0.0, 1.0);
    std::vector<Point> result;
    result.reserve(static_cast<std::size_t>(c.count));
    for (int i = 0; i < c.count; ++i) {
        // Uniform area density, with angles in radians (not integer degrees).
        const double r = std::sqrt(c.inner_radius * c.inner_radius + uniform(rng) *
                                  (c.outer_radius * c.outer_radius - c.inner_radius * c.inner_radius));
        const double theta = uniform(rng) * 6.2831853071795864769;
        result.push_back({r * std::cos(theta), r * std::sin(theta)});
    }
    return result;
}
bool Session::start(const std::vector<Config>& sequence, double now) {
    if (running_) throw std::logic_error("Session already running");
    if (!std::isfinite(now)) throw std::invalid_argument("Invalid start time");
    std::vector<Trial> next;
    for (std::size_t i = 0; i < sequence.size(); ++i) {
        sequence[i].validate();
        if (next.size() + static_cast<std::size_t>(sequence[i].repetitions) > 10000)
            throw std::invalid_argument("At most 10000 trials per session");
        for (int r = 0; r < sequence[i].repetitions; ++r)
            next.push_back({sequence[i], i, r, 0, 0, 0, {}});
    }
    trials_ = std::move(next);
    index_ = 0;
    start_ = now;
    running_ = !trials_.empty();
    status_ = running_ ? "running" : "empty";
    return running_;
}
bool Session::advance(double now) {
    if (!running_ || trial_elapsed(now) < current().config.duration) return false;
    trials_[index_].end_seconds = elapsed(now);
    if (index_ + 1 == trials_.size()) {
        running_ = false;
        status_ = "completed";
    } else {
        ++index_;
        trials_[index_].onset_seconds = elapsed(now);
    }
    return true;
}
void Session::respond(int key, double now) {
    if (running_) trials_[index_].responses.push_back({key, elapsed(now), trial_elapsed(now), current().frames});
}
void Session::frame() { if (running_) ++trials_[index_].frames; }
void Session::abort(double now, std::string reason) {
    if (!running_) return;
    trials_[index_].end_seconds = elapsed(now);
    trials_.resize(index_ + 1); // unpresented trials must not masquerade as observations
    running_ = false;
    status_ = std::move(reason);
}
}
