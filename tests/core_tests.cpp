#include "model.hpp"
#include "storage.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace stimulus;
namespace {
void check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
    bool threw = false; try { f(); } catch (const std::exception&) { threw = true; }
    check(threw, "Expected invalid input to be rejected");
}
}
int main() {
    try {
        Session s;
        check(!s.start({}, 0), "Empty sequence must not start");
        Config c; c.duration = 2; c.repetitions = 2;
        check(s.start({c}, 10), "Valid sequence must start");
        check(s.trials().size() == 2, "Exactly two repetitions");
        s.frame(); s.respond(65, 10.25);
        check(s.current().responses[0].trial_seconds == .25, "Timestamp must use seconds");
        check(!s.advance(11.999), "No early transition");
        check(s.advance(12) && s.index() == 1, "Transition at requested duration");
        s.frame(); s.respond(66, 12.5);
        check(s.current().responses[0].trial_seconds == .5, "Trial-relative response time");
        check(s.advance(14) && !s.running() && s.status() == "completed", "No extra repetition");
        check(encode_responses_csv(s).find("1,0,1,66,2.5,0.5,1") != std::string::npos, "CSV uses trial indices and seconds");
        check(encode_session(s).find("completed") != std::string::npos, "Report status");
        s.start({c, c}, 20); s.abort(20.5, "aborted_escape");
        check(s.trials().size() == 1 && !s.running(), "Abort excludes unpresented trials");
        s.start({c}, 30);
        check(s.running() && s.current().responses.empty(), "Restart clears responses and abort state");
        check(s.advance(35) && s.current().onset_seconds == 5, "Late callback does not skip unseen trials");
        check(!s.advance(36), "Next trial still receives its full duration");
        c.repetitions = 0; rejects([&] { c.validate(); });
        c = Config{}; c.outer_radius = c.inner_radius - 1; rejects([&] { c.validate(); });
        const auto legacy = decode_library(R"({"type":"Fixing","center_Y":312,"repetitionw":3})");
        check(legacy.stimuli[0].center_y == 312 && legacy.stimuli[0].repetitions == 3, "Legacy aliases migrate");
        const auto old_circles = decode_library(R"({"type":"RandomCircles","outter_radius":240})");
        check(old_circles.stimuli[0].outer_radius == 240, "Legacy radius spelling");
        Library library{legacy.stimuli, old_circles.stimuli};
        auto roundtrip = decode_library(encode_library(library));
        check(roundtrip.stimuli[0].center_y == 312 && roundtrip.sequence[0].outer_radius == 240, "Library round trip");
        roundtrip.stimuli[0].duration = 20;
        check(library.stimuli[0].duration == 5, "Configurations own their values");
        rejects([] { decode_library("{"); });
        rejects([] { decode_library(R"({"type":"Unknown"})"); });
        rejects([] { decode_library(R"({"type":"Fixing","duration":1.5})"); });
        rejects([] { decode_library(R"({"type":"Fixing","FPS":0})"); });
        rejects([] { decode_library(R"({"schema_version":2,"stimuli":[],"sequence":[]})"); });
        rejects([] { decode_library(R"({"type":"Fixing","type":"ColoredWords"})"); });
        c = Config{}; c.kind = Kind::RandomCircles; c.count = 2000;
        std::mt19937 a(42), b(42);
        const auto points = sample_annulus(c, a), duplicate = sample_annulus(c, b);
        double squared_sum = 0;
        for (std::size_t i = 0; i < points.size(); ++i) {
            const double r2 = points[i].x * points[i].x + points[i].y * points[i].y;
            check(r2 >= 10000 - 1e-8 && r2 <= 40000 + 1e-8, "Point inside annulus");
            check(points[i].x == duplicate[i].x && points[i].y == duplicate[i].y, "Seed is reproducible");
            squared_sum += r2;
        }
        check(std::abs(squared_sum / points.size() - 25000) < 1000, "Uniform area density");
        c.inner_radius = c.outer_radius;
        for (const auto p : sample_annulus(c, a)) check(std::abs(std::hypot(p.x, p.y) - 200) < 1e-8, "Degenerate annulus is a circle");
        std::cout << "All core regression tests passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
