#include "sirenproof/core/state_json.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace sirenproof::core {

namespace {

using json = nlohmann::ordered_json;

class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

constexpr std::array<std::string_view, 7> kDayNames = {"mon", "tue", "wed", "thu", "fri", "sat", "sun"};

std::string child(std::string_view path, std::string_view key) {
    return path.empty() ? std::string(key) : std::string(path) + "." + std::string(key);
}

std::string element(std::string_view path, std::size_t i) { return std::string(path) + "[" + std::to_string(i) + "]"; }

// Reads one JSON object and remembers which fields were used, so leftovers can be reported.
class Reader {
public:
    Reader(const json& value, std::string path) : value_(value), path_(std::move(path)) {
        if (!value_.is_object()) throw ParseError((path_.empty() ? "state" : path_) + ": expected an object");
    }

    std::string path(std::string_view key) const { return child(path_, key); }
    bool has(std::string_view key) const { return value_.contains(std::string(key)); }

    const json& get(std::string_view key) {
        const auto it = value_.find(std::string(key));
        if (it == value_.end()) throw ParseError(path(key) + ": missing");
        used_.insert(std::string(key));
        return *it;
    }

    std::string text(std::string_view key) {
        const json& v = get(key);
        if (!v.is_string()) throw ParseError(path(key) + ": expected text");
        return v.get<std::string>();
    }
    std::string text_or(std::string_view key, std::string fallback) {
        return has(key) ? text(key) : std::move(fallback);
    }

    std::int64_t integer(std::string_view key, std::int64_t min, std::int64_t max) {
        const json& v = get(key);
        if (!v.is_number_integer() ||
            (v.is_number_unsigned() && v.get<std::uint64_t>() > std::uint64_t{std::numeric_limits<std::int64_t>::max()})) {
            throw ParseError(path(key) + ": expected a whole number");
        }
        const auto n = v.get<std::int64_t>();
        if (n < min || n > max) {
            throw ParseError(path(key) + ": must be between " + std::to_string(min) + " and " + std::to_string(max));
        }
        return n;
    }
    std::int64_t integer_or(std::string_view key, std::int64_t fallback, std::int64_t min, std::int64_t max) {
        return has(key) ? integer(key, min, max) : fallback;
    }

    std::uint64_t unsigned_integer(std::string_view key) {
        const json& v = get(key);
        if (!v.is_number_unsigned() && !(v.is_number_integer() && v.get<std::int64_t>() >= 0)) {
            throw ParseError(path(key) + ": expected a non-negative whole number");
        }
        return v.get<std::uint64_t>();
    }

    bool boolean_or(std::string_view key, bool fallback) {
        if (!has(key)) return fallback;
        const json& v = get(key);
        if (!v.is_boolean()) throw ParseError(path(key) + ": expected true or false");
        return v.get<bool>();
    }

    TimePoint time(std::string_view key) {
        const auto t = parse_time(text(key));
        if (!t) throw ParseError(path(key) + ": expected a time like 2026-11-02T09:00:00Z");
        return *t;
    }

    const json& array(std::string_view key) {
        const json& v = get(key);
        if (!v.is_array()) throw ParseError(path(key) + ": expected a list");
        return v;
    }

    void finish() const {
        for (const auto& item : value_.items()) {
            if (!used_.contains(item.key())) throw ParseError(path(item.key()) + ": unknown field");
        }
    }

private:
    const json& value_;
    std::string path_;
    std::set<std::string> used_;
};

// ---------------------------------------------------------------------------------------------------
// Reading

Settings read_settings(const json& value, const std::string& path) {
    Reader r(value, path);
    Settings s;
    s.max_lock = Hours{r.integer_or("max_lock_hours", s.max_lock.count(), 1, 168)};
    s.breaks_per_day = static_cast<int>(r.integer_or("breaks_per_day", s.breaks_per_day, 0, 20));
    s.break_length = Minutes{r.integer_or("break_minutes", s.break_length.count(), 1, 60)};
    s.nudge_pause = Seconds{r.integer_or("nudge_pause_seconds", s.nudge_pause.count(), 3, 300)};
    s.nudge_continue = Minutes{r.integer_or("nudge_continue_minutes", s.nudge_continue.count(), 1, 60)};
    r.finish();
    return s;
}

std::vector<Target> read_targets(const json& list, const std::string& path) {
    std::vector<Target> targets;
    for (std::size_t i = 0; i < list.size(); ++i) {
        const json& item = list[i];
        auto target = item.is_string() ? parse_target(item.get<std::string>()) : std::nullopt;
        if (!target) throw ParseError(element(path, i) + ": not a valid target");
        targets.push_back(*std::move(target));
    }
    return targets;
}

WeeklyWindow read_window(const json& value, const std::string& path) {
    Reader r(value, path);
    WeeklyWindow w;
    const json& days = r.array("days");
    for (std::size_t i = 0; i < days.size(); ++i) {
        const auto name = days[i].is_string() ? days[i].get<std::string>() : std::string{};
        const auto it = std::ranges::find(kDayNames, name);
        if (it == kDayNames.end()) throw ParseError(element(r.path("days"), i) + ": expected mon, tue, ... or sun");
        w.days = static_cast<std::uint8_t>(w.days | (1u << (it - kDayNames.begin())));
    }
    const auto from = parse_clock(r.text("from"));
    const auto to = parse_clock(r.text("to"));
    if (!from || *from >= kMinutesPerDay) throw ParseError(r.path("from") + ": expected a time like 09:00");
    if (!to) throw ParseError(r.path("to") + ": expected a time like 17:00");
    w.from = *from;
    w.to = *to;
    r.finish();
    return w;
}

When read_when(const json& value, const std::string& path) {
    if (value.is_string() && value.get<std::string>() == "always") return Always{};
    Reader r(value, path);
    if (r.has("weekly")) {
        const json& list = r.array("weekly");
        std::vector<WeeklyWindow> windows;
        for (std::size_t i = 0; i < list.size(); ++i) windows.push_back(read_window(list[i], element(r.path("weekly"), i)));
        r.finish();
        auto schedule = WeeklySchedule::make(std::move(windows));
        if (!schedule) throw ParseError(r.path("weekly") + ": not a valid schedule");
        return *std::move(schedule);
    }
    Span span{r.time("from"), r.time("to")};
    r.finish();
    return span;
}

UnlockMethod read_unlock(const json& value, const std::string& path) {
    Reader r(value, path);
    UnlockMethod u;
    const std::string method = r.text("method");
    if (method == "none") {
        u.kind = UnlockMethod::Kind::None;
    } else if (method == "wait") {
        u.kind = UnlockMethod::Kind::Wait;
        u.wait = Minutes{r.integer("wait_minutes", 1, 24 * 60)};
    } else if (method == "type") {
        u.kind = UnlockMethod::Kind::Type;
        u.type_length = static_cast<int>(r.integer("length", 20, 2000));
    } else if (method == "friend") {
        u.kind = UnlockMethod::Kind::Friend;
        u.friend_salt = r.text_or("salt", "");
        u.friend_hash = r.text_or("hash", "");
    } else {
        throw ParseError(r.path("method") + ": expected none, wait, type or friend");
    }
    r.finish();
    return u;
}

Rule read_rule(const json& value, const std::string& path) {
    Reader r(value, path);
    Rule rule;
    rule.id = r.text("id");
    rule.name = r.text_or("name", "");
    rule.goal = r.text_or("goal", "");
    rule.block = read_targets(r.array("block"), r.path("block"));
    if (r.has("allow")) rule.allow = read_targets(r.array("allow"), r.path("allow"));
    if (r.has("when")) rule.when = read_when(r.get("when"), r.path("when"));
    if (r.has("strength")) {
        const auto strength = parse_strength(r.text("strength"));
        if (!strength) throw ParseError(r.path("strength") + ": expected nudge, block or lock");
        rule.strength = *strength;
    }
    rule.loosen_delay = Minutes{r.integer_or("loosen_delay_minutes", rule.loosen_delay.count(), 0, 7 * 24 * 60)};
    if (r.has("unlock")) rule.unlock = read_unlock(r.get("unlock"), r.path("unlock"));
    rule.breaks_allowed = r.boolean_or("breaks_allowed", false);
    r.finish();

    // Structural check only: the lock cap is a policy for new rules, and lowering it later mustn't make
    // an existing state unreadable.
    Settings structural;
    structural.max_lock = Hours{168};
    if (auto error = validate_rule(rule, structural)) throw ParseError(path + ": " + *error);
    return rule;
}

PendingChange read_pending(const json& value, const std::string& path) {
    Reader r(value, path);
    PendingChange p;
    p.id = r.text("id");
    p.apply_at = r.time("apply_at");
    const int kinds = int{r.has("replace_rule")} + int{r.has("remove_rule")} + int{r.has("settings")};
    if (kinds != 1) throw ParseError(path + ": needs exactly one of replace_rule, remove_rule or settings");
    if (r.has("replace_rule")) {
        p.change = ReplaceRule{read_rule(r.get("replace_rule"), r.path("replace_rule"))};
    } else if (r.has("remove_rule")) {
        p.change = RemoveRule{r.text("remove_rule")};
    } else {
        p.change = ChangeSettings{read_settings(r.get("settings"), r.path("settings"))};
    }
    r.finish();
    return p;
}

template <class T, class ReadOne>
std::vector<T> read_list(Reader& r, std::string_view key, ReadOne read_one) {
    std::vector<T> items;
    if (!r.has(key)) return items;
    const json& list = r.array(key);
    for (std::size_t i = 0; i < list.size(); ++i) items.push_back(read_one(list[i], element(r.path(key), i)));
    return items;
}

State read_state(const json& root) {
    Reader r(root, "");
    if (r.integer("version", 0, std::numeric_limits<int>::max()) != kStateFormatVersion) {
        throw ParseError("version: this Sirenproof understands version " + std::to_string(kStateFormatVersion));
    }

    State state;
    if (r.has("settings")) state.settings = read_settings(r.get("settings"), "settings");
    state.rules = read_list<Rule>(r, "rules", read_rule);
    std::set<std::string> ids;
    for (std::size_t i = 0; i < state.rules.size(); ++i) {
        if (!ids.insert(state.rules[i].id).second) throw ParseError(element("rules", i) + ".id: used twice");
    }
    state.pending = read_list<PendingChange>(r, "pending", read_pending);

    state.allowances = read_list<Allowance>(r, "allowances", [](const json& v, const std::string& p) {
        Reader a(v, p);
        Allowance allowance{a.text("rule"), a.time("from"), a.time("until")};
        a.finish();
        return allowance;
    });
    state.releases = read_list<Release>(r, "releases", [](const json& v, const std::string& p) {
        Reader a(v, p);
        Release release{a.text("rule"), a.time("from"), a.time("until")};
        a.finish();
        return release;
    });
    state.unlock_requests = read_list<UnlockRequest>(r, "unlock_requests", [](const json& v, const std::string& p) {
        Reader a(v, p);
        UnlockRequest request{a.text("rule"), a.time("due")};
        a.finish();
        return request;
    });
    state.type_challenges = read_list<TypeChallenge>(r, "type_challenges", [](const json& v, const std::string& p) {
        Reader a(v, p);
        TypeChallenge challenge{a.text("rule"), a.text("text")};
        a.finish();
        return challenge;
    });
    state.friend_attempts = read_list<FriendAttempts>(r, "friend_attempts", [](const json& v, const std::string& p) {
        Reader a(v, p);
        FriendAttempts attempts{a.text("rule"), static_cast<int>(a.integer("failures", 0, 1'000'000)),
                                a.time("retry_after")};
        a.finish();
        return attempts;
    });
    if (r.has("breaks")) {
        Reader b(r.get("breaks"), "breaks");
        state.breaks = {b.text("date"), static_cast<int>(b.integer("used", 0, 1000))};
        b.finish();
    }
    if (r.has("clock")) {
        Reader c(r.get("clock"), "clock");
        constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
        ClockCheckpoint clock;
        clock.trusted = WallTime{Millis{c.integer("trusted_ms", 0, kMax)}};
        clock.wall = WallTime{Millis{c.integer("wall_ms", 0, kMax)}};
        clock.monotonic = Millis{c.integer("monotonic_ms", 0, kMax)};
        clock.boot_id = c.unsigned_integer("boot_id");
        c.finish();
        state.clock = clock;
    }
    if (r.has("next_id")) state.next_id = r.unsigned_integer("next_id");
    r.finish();
    return state;
}

// ---------------------------------------------------------------------------------------------------
// Writing

json write_settings(const Settings& s) {
    return json{{"max_lock_hours", s.max_lock.count()},
                {"breaks_per_day", s.breaks_per_day},
                {"break_minutes", s.break_length.count()},
                {"nudge_pause_seconds", s.nudge_pause.count()},
                {"nudge_continue_minutes", s.nudge_continue.count()}};
}

json write_targets(const std::vector<Target>& targets) {
    json list = json::array();
    for (const Target& t : targets) list.push_back(to_string(t));
    return list;
}

json write_when(const When& when) {
    if (std::holds_alternative<Always>(when)) return "always";
    if (const auto* span = std::get_if<Span>(&when)) {
        return json{{"from", format_time(span->start)}, {"to", format_time(span->end)}};
    }
    json windows = json::array();
    for (const WeeklyWindow& w : std::get<WeeklySchedule>(when).windows()) {
        json days = json::array();
        for (std::size_t d = 0; d < kDayNames.size(); ++d) {
            if ((static_cast<unsigned>(w.days) >> d) & 1u) days.push_back(kDayNames[d]);
        }
        windows.push_back(json{{"days", days}, {"from", format_clock(w.from)}, {"to", format_clock(w.to)}});
    }
    return json{{"weekly", windows}};
}

json write_unlock(const UnlockMethod& u) {
    switch (u.kind) {
        case UnlockMethod::Kind::None: return json{{"method", "none"}};
        case UnlockMethod::Kind::Wait: return json{{"method", "wait"}, {"wait_minutes", u.wait.count()}};
        case UnlockMethod::Kind::Type: return json{{"method", "type"}, {"length", u.type_length}};
        case UnlockMethod::Kind::Friend:
            return json{{"method", "friend"}, {"salt", u.friend_salt}, {"hash", u.friend_hash}};
    }
    return json{{"method", "none"}};
}

json write_rule(const Rule& rule) {
    json j{{"id", rule.id}};
    if (!rule.name.empty()) j["name"] = rule.name;
    if (!rule.goal.empty()) j["goal"] = rule.goal;
    j["block"] = write_targets(rule.block);
    if (!rule.allow.empty()) j["allow"] = write_targets(rule.allow);
    j["when"] = write_when(rule.when);
    j["strength"] = std::string(to_string(rule.strength));
    j["loosen_delay_minutes"] = rule.loosen_delay.count();
    if (rule.unlock.kind != UnlockMethod::Kind::None) j["unlock"] = write_unlock(rule.unlock);
    if (rule.breaks_allowed) j["breaks_allowed"] = true;
    return j;
}

json write_pending(const PendingChange& p) {
    json j{{"id", p.id}, {"apply_at", format_time(p.apply_at)}};
    if (const auto* replace = std::get_if<ReplaceRule>(&p.change)) j["replace_rule"] = write_rule(replace->rule);
    if (const auto* remove = std::get_if<RemoveRule>(&p.change)) j["remove_rule"] = remove->rule_id;
    if (const auto* settings = std::get_if<ChangeSettings>(&p.change)) j["settings"] = write_settings(settings->settings);
    return j;
}

}  // namespace

std::expected<State, std::string> parse_state(std::string_view json_text) {
    try {
        const json root = json::parse(json_text);
        return read_state(root);
    } catch (const ParseError& e) {
        return std::unexpected(std::string(e.what()));
    } catch (const json::exception& e) {
        return std::unexpected("not valid JSON: " + std::string(e.what()));
    }
}

std::string serialise_state(const State& state) {
    json root{{"version", kStateFormatVersion}, {"settings", write_settings(state.settings)}};

    json rules = json::array();
    for (const Rule& rule : state.rules) rules.push_back(write_rule(rule));
    root["rules"] = rules;

    json pending = json::array();
    for (const PendingChange& p : state.pending) pending.push_back(write_pending(p));
    root["pending"] = pending;

    json allowances = json::array();
    for (const Allowance& a : state.allowances) {
        allowances.push_back({{"rule", a.rule_id}, {"from", format_time(a.from)}, {"until", format_time(a.until)}});
    }
    root["allowances"] = allowances;

    json releases = json::array();
    for (const Release& r : state.releases) {
        releases.push_back({{"rule", r.rule_id}, {"from", format_time(r.from)}, {"until", format_time(r.until)}});
    }
    root["releases"] = releases;

    json requests = json::array();
    for (const UnlockRequest& u : state.unlock_requests) requests.push_back({{"rule", u.rule_id}, {"due", format_time(u.due)}});
    root["unlock_requests"] = requests;

    json challenges = json::array();
    for (const TypeChallenge& c : state.type_challenges) challenges.push_back({{"rule", c.rule_id}, {"text", c.text}});
    root["type_challenges"] = challenges;

    json attempts = json::array();
    for (const FriendAttempts& f : state.friend_attempts) {
        attempts.push_back(
            {{"rule", f.rule_id}, {"failures", f.failures}, {"retry_after", format_time(f.retry_after)}});
    }
    root["friend_attempts"] = attempts;

    root["breaks"] = {{"date", state.breaks.date}, {"used", state.breaks.used}};
    if (state.clock) {
        root["clock"] = {{"trusted_ms", state.clock->trusted.time_since_epoch().count()},
                         {"wall_ms", state.clock->wall.time_since_epoch().count()},
                         {"monotonic_ms", state.clock->monotonic.count()},
                         {"boot_id", state.clock->boot_id}};
    }
    root["next_id"] = state.next_id;
    return root.dump(2) + "\n";
}

}  // namespace sirenproof::core
