#pragma once

#include <cstdint>
#include <initializer_list>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "sirenproof/core/crypto.hpp"
#include "sirenproof/core/rule.hpp"
#include "sirenproof/core/target.hpp"
#include "sirenproof/core/time.hpp"

namespace sirenproof::test {

// Predictable "random" bytes: 0, 1, 2, ...
class CountingRandom : public core::RandomSource {
public:
    void fill(std::span<std::uint8_t> out) override {
        for (std::uint8_t& b : out) b = next_++;
    }

private:
    std::uint8_t next_ = 0;
};

inline core::TimePoint at(std::string_view text) {
    const auto t = core::parse_time(text);
    if (!t) throw std::invalid_argument("bad test time: " + std::string(text));
    return *t;
}

inline core::Minutes utc(core::TimePoint) { return core::Minutes{0}; }

inline core::Target target(std::string_view text) {
    auto t = core::parse_target(text);
    if (!t) throw std::invalid_argument("bad test target: " + std::string(text));
    return *std::move(t);
}

inline core::Rule make_rule(std::string id, std::initializer_list<std::string_view> block,
                            core::Strength strength = core::Strength::Block) {
    core::Rule rule;
    rule.id = std::move(id);
    for (std::string_view b : block) rule.block.push_back(target(b));
    rule.strength = strength;
    return rule;
}

inline core::WeeklySchedule schedule(std::uint8_t days, int from_minute, int to_minute) {
    auto s = core::WeeklySchedule::make({{days, from_minute, to_minute}});
    if (!s) throw std::invalid_argument("bad test schedule");
    return *std::move(s);
}

}  // namespace sirenproof::test
