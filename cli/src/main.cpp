// sirenproof: command-line front end.
//
// Most commands talk to the Sirenproof service over its pipe. check, plan and validate can also work
// offline on a rules file (--state), which changes nothing on the system.

#include <chrono>
#include <format>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/core/policy.hpp"
#include "sirenproof/core/state_json.hpp"
#include "sirenproof/core/time.hpp"
#include "sirenproof/core/version.hpp"
#include "sirenproof/platform/files.hpp"
#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/system.hpp"

namespace core = sirenproof::core;
namespace platform = sirenproof::platform;
using core::Json;

namespace {

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitFailed = 2;

constexpr std::string_view kUsage = R"(Usage:
  sirenproof status                       what is blocked right now, and why
  sirenproof rules                        list your rules
  sirenproof block <target>... [options]  add a rule
      --for <time>        a session starting now, e.g. 25m, 2h, 1h30m (default: always)
      --lock              can't be loosened until it ends (needs --for)
      --unlock <method>   how a lock can end early: wait:<minutes>, type:<characters> or friend
      --nudge             a pause instead of a block
      --name <text>  --goal <text>  --id <id>
  sirenproof remove <rule>                remove a rule (waits out its loosen delay)
  sirenproof cancel <change>              cancel a waiting change
  sirenproof break                        take a short break from blocks that allow it
  sirenproof continue <rule>              continue for a few minutes after a nudge
  sirenproof unlock <rule> [--cancel]     end a lock early, using the method it was given
  sirenproof check <address-or-program>   would this be blocked right now?

  Offline, on a rules file (changes nothing):
  sirenproof check <address-or-program> --state <file> [--at <UTC time>]
  sirenproof plan --state <file> [--at <UTC time>]
  sirenproof validate <file>

Targets: reddit.com, youtube.com/shorts, app:steam.exe, app:C:\Games\, publisher:<name>, package:<name>
)";

struct Options {
    std::vector<std::string> positional;
    std::string state_path;
    std::optional<core::TimePoint> at;
    std::wstring pipe = std::wstring(platform::kServicePipeName);
    std::optional<std::string> for_time, unlock, name, goal, id;
    bool lock = false, nudge = false, cancel = false;
};

// ---------------------------------------------------------------------------------------------------
// Helpers

std::string local(core::TimePoint t) { return std::format("{:%Y-%m-%d %H:%M}", t + platform::local_offset_at(t)); }

std::string local(const Json& iso) {
    if (!iso.is_string()) return "-";
    const auto t = core::parse_time(iso.get<std::string>());
    return t ? local(*t) : iso.get<std::string>();
}

std::optional<int> parse_minutes(std::string_view text) {  // "25m", "2h", "1h30m", "90"
    const auto length = core::parse_duration(text);
    return length ? std::optional(static_cast<int>(length->count())) : std::nullopt;
}

bool looks_like_program(std::string_view text) {
    if (text.find('\\') != std::string_view::npos) return true;
    if (text.size() < 4) return false;
    std::string ending(text.substr(text.size() - 4));
    for (char& c : ending) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return ending == ".exe";
}

std::optional<Options> parse_options(const std::vector<std::string>& args) {
    Options o;
    const auto value = [&](std::size_t& i) -> std::optional<std::string> {
        if (i + 1 >= args.size()) return std::nullopt;
        return args[++i];
    };
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& a = args[i];
        std::optional<std::string> v;
        if (a == "--state") {
            if (!(v = value(i))) return std::nullopt;
            o.state_path = *v;
        } else if (a == "--at") {
            if (!(v = value(i)) || !(o.at = core::parse_time(*v))) {
                std::cerr << "sirenproof: --at expects a UTC time like 2026-11-02T09:30:00Z\n";
                return std::nullopt;
            }
        } else if (a == "--pipe") {
            if (!(v = value(i))) return std::nullopt;
            o.pipe = platform::widen(*v);
        } else if (a == "--for") {
            if (!(o.for_time = value(i))) return std::nullopt;
        } else if (a == "--unlock") {
            if (!(o.unlock = value(i))) return std::nullopt;
        } else if (a == "--name") {
            if (!(o.name = value(i))) return std::nullopt;
        } else if (a == "--goal") {
            if (!(o.goal = value(i))) return std::nullopt;
        } else if (a == "--id") {
            if (!(o.id = value(i))) return std::nullopt;
        } else if (a == "--lock") {
            o.lock = true;
        } else if (a == "--nudge") {
            o.nudge = true;
        } else if (a == "--cancel") {
            o.cancel = true;
        } else if (a.starts_with("--")) {
            std::cerr << "sirenproof: unknown option " << a << "\n";
            return std::nullopt;
        } else {
            o.positional.push_back(a);
        }
    }
    return o;
}

// One request to the service. Only a service running from this folder is believed.
std::expected<Json, std::string> call(const Options& o, std::string_view op, Json args = Json::object()) {
    const Json request{{"v", 1}, {"op", op}, {"args", std::move(args)}};
    const auto reply = platform::pipe_call(o.pipe, request.dump(),
                                           {std::chrono::seconds{5}, platform::executable_dir() / L"sirenproof-service.exe"});
    if (!reply) return std::unexpected(reply.error());
    const Json answer = Json::parse(*reply, nullptr, false);
    if (answer.is_discarded() || !answer.is_object()) return std::unexpected("the service sent something unreadable");
    if (!answer.value("ok", false)) return std::unexpected(answer.value("error", std::string("unknown error")));
    return answer["result"];
}

// The service writes times as UTC ("2026-11-02T07:12:05Z"); show them in local time instead.
std::string localise(std::string text) {
    static const std::regex iso(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z)");
    std::string out;
    auto last = text.cbegin();
    for (std::sregex_iterator it(text.begin(), text.end(), iso), end; it != end; ++it) {
        out.append(last, (*it)[0].first);
        const auto t = core::parse_time((*it)[0].str());
        out += t ? local(*t) : (*it)[0].str();
        last = (*it)[0].second;
    }
    out.append(last, text.cend());
    return out;
}

int failed(const std::string& message) {
    std::cerr << "sirenproof: " << localise(message) << "\n";
    return kExitFailed;
}

int report_change(const Json& r) {
    const std::string outcome = r.value("outcome", "");
    if (outcome == "refused") return failed(r.value("message", "refused"));
    if (outcome == "pending") {
        std::cout << "Waiting until " << local(r["applies_at"]) << ": " << localise(r.value("message", "")) << "\n"
                  << "  (cancel it with: sirenproof cancel " << r.value("pending", "") << ")\n";
    } else {
        std::cout << "Done" << (r.contains("rule") ? " (rule " + r.value("rule", "") + ")" : "") << ".\n";
    }
    if (r.contains("friend_code")) {
        std::cout << "\nFriend code: " << r.value("friend_code", "") << "\n"
                  << "Give it to someone you trust now. It is shown only this once and can't be recovered.\n";
    }
    return kExitOk;
}

int report_action(const Json& r) {
    if (!r.value("ok", false)) return failed(r.value("message", "not possible right now"));
    std::cout << (r["until"].is_string() ? "OK, until " + local(r["until"]) : std::string("OK")) << ".\n";
    return kExitOk;
}

// ---------------------------------------------------------------------------------------------------
// Commands that talk to the service

int run_status(const Options& o) {
    auto s = call(o, "status");
    if (!s) return failed(s.error());
    const Json& r = *s;
    std::cout << "Sirenproof " << r.value("version", "") << " - "
              << (r.value("dry_run", false) ? "DRY RUN: nothing is actually blocked" : "enforcing") << "\n";
    std::cout << "Trusted time: " << local(r["now"]);
    if (const auto off = r.value("clock_offset_seconds", std::int64_t{0}); std::abs(off) > 120) {
        std::cout << "  (the Windows clock is " << off / 60 << " min off; ignored while it would end a block)";
    }
    std::cout << "\n\n";

    const auto list = [](const Json& items) {
        std::string out;
        for (const auto& item : items) out += (out.empty() ? "" : ", ") + item.get<std::string>();
        return out.empty() ? std::string("none") : out;
    };
    std::cout << "Active rules:  " << list(r["active_rules"]) << "\n";
    for (const auto& l : r["locked"]) std::cout << "Locked:        " << l.value("rule", "") << " until " << local(l["until"]) << "\n";
    std::cout << "Hosts file:    " << list(r["hosts"]) << "\n";
    std::cout << "Programs:      ";
    if (r["apps"].empty()) std::cout << "none";
    for (const auto& a : r["apps"]) std::cout << a.value("action", "") << " " << a.value("target", "") << " (" << a.value("rule", "") << ")  ";
    std::cout << "\n";
    for (const auto& p : r["pending"]) {
        std::cout << "Waiting:       " << p.value("id", "") << " " << p.value("what", "") << " at " << local(p["applies_at"])
                  << "  (sirenproof cancel " << p.value("id", "") << ")\n";
    }
    for (const auto& u : r["unlocking"]) std::cout << "Unlocking:     " << u.value("rule", "") << " at " << local(u["due"]) << "\n";
    std::cout << "Breaks today:  " << r["breaks"].value("used_today", 0) << " of " << r["breaks"].value("per_day", 0) << "\n";
    std::cout << "Next change:   " << local(r["next_change"]) << "\n";
    return kExitOk;
}

int run_rules(const Options& o) {
    auto r = call(o, "rules.list");
    if (!r) return failed(r.error());
    if ((*r)["rules"].empty()) std::cout << "No rules yet. Try: sirenproof block reddit.com --for 25m\n";
    for (const auto& rule : (*r)["rules"]) std::cout << rule.dump(2) << "\n";
    return kExitOk;
}

int run_block(const Options& o) {
    if (o.positional.size() < 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    core::Rule rule;
    if (o.id) rule.id = *o.id;
    if (o.name) rule.name = *o.name;
    if (o.goal) rule.goal = *o.goal;
    for (std::size_t i = 1; i < o.positional.size(); ++i) {
        auto target = core::parse_target(o.positional[i]);
        if (!target) return failed(o.positional[i] + " isn't a target (see sirenproof --help)");
        rule.block.push_back(*std::move(target));
    }
    if (o.nudge && o.lock) return failed("choose --nudge or --lock, not both");
    rule.strength = o.lock ? core::Strength::Lock : o.nudge ? core::Strength::Nudge : core::Strength::Block;
    if (o.nudge) rule.loosen_delay = core::Minutes{2};

    std::optional<int> minutes;
    if (o.for_time) {
        minutes = parse_minutes(*o.for_time);
        if (!minutes) return failed("--for expects a time like 25m, 2h or 1h30m");
    }
    if (o.lock && !minutes) return failed("a lock needs an end: add --for, e.g. --for 2h");
    // The service sets the real start and end from trusted time; this placeholder only has to be valid.
    const auto placeholder = std::chrono::floor<core::Seconds>(std::chrono::system_clock::now());
    if (minutes) rule.when = core::Span{placeholder, placeholder + core::Minutes{*minutes}};

    if (o.unlock) {
        if (!o.lock) return failed("only a lock has an unlock method");
        const std::string& u = *o.unlock;
        if (u == "friend") {
            rule.unlock.kind = core::UnlockMethod::Kind::Friend;
        } else if (u.starts_with("wait:")) {
            rule.unlock.kind = core::UnlockMethod::Kind::Wait;
            rule.unlock.wait = core::Minutes{parse_minutes(u.substr(5)).value_or(0)};
        } else if (u.starts_with("type:")) {
            rule.unlock.kind = core::UnlockMethod::Kind::Type;
            rule.unlock.type_length = parse_minutes(u.substr(5)).value_or(0);
        } else {
            return failed("--unlock expects wait:<minutes>, type:<characters> or friend");
        }
    }

    Json rule_json = core::rule_to_json(rule);
    if (rule.id.empty()) rule_json.erase("id");  // the service picks one
    Json args{{"rule", rule_json}};
    if (minutes) args["for_minutes"] = *minutes;
    auto r = call(o, "rules.add", args);
    if (!r) return failed(r.error());
    return report_change(*r);
}

int run_simple(const Options& o, std::string_view op, const char* key) {
    if (o.positional.size() != 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    auto r = call(o, op, Json{{key, o.positional[1]}});
    if (!r) return failed(r.error());
    if (op == "rules.remove") return report_change(*r);
    if (op == "pending.cancel") {
        std::cout << ((*r).value("cancelled", false) ? "Cancelled.\n" : "There's no waiting change with that id.\n");
        return kExitOk;
    }
    return report_action(*r);
}

int run_unlock(const Options& o) {
    if (o.positional.size() != 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    const std::string& id = o.positional[1];
    auto rules = call(o, "rules.list");
    if (!rules) return failed(rules.error());
    std::string method;
    for (const auto& rule : (*rules)["rules"]) {
        if (rule.value("id", "") == id) method = rule.contains("unlock") ? rule["unlock"].value("method", "none") : "none";
    }
    if (method.empty()) return failed("there is no rule called " + id);

    if (method == "wait") {
        if (o.cancel) {
            auto r = call(o, "unlock.cancel", Json{{"rule", id}});
            if (!r) return failed(r.error());
            std::cout << ((*r).value("cancelled", false) ? "Unlock cancelled. The lock stays.\n" : "Nothing to cancel.\n");
            return kExitOk;
        }
        auto r = call(o, "unlock.request", Json{{"rule", id}});
        if (!r) return failed(r.error());
        return report_action(*r);
    }
    if (method == "type") {
        auto begin = call(o, "unlock.type.begin", Json{{"rule", id}});
        if (!begin) return failed(begin.error());
        std::cout << "Type this exactly, then press Enter:\n\n  " << (*begin).value("text", "") << "\n\n> " << std::flush;
        std::string typed;
        std::getline(std::cin, typed);
        auto r = call(o, "unlock.type.finish", Json{{"rule", id}, {"text", typed}});
        if (!r) return failed(r.error());
        return report_action(*r);
    }
    if (method == "friend") {
        std::cout << "Friend code: " << std::flush;
        std::string code;
        std::getline(std::cin, code);
        auto r = call(o, "unlock.friend", Json{{"rule", id}, {"code", code}});
        if (!r) return failed(r.error());
        return report_action(*r);
    }
    return failed("this lock has no way to end early; it ends on its own");
}

int run_check_service(const Options& o) {
    auto r = call(o, "check", Json{{"subject", o.positional[1]}});
    if (!r) return failed(r.error());
    const std::string action = (*r).value("action", "allow");
    if (action == "allow") {
        std::cout << "ALLOWED right now.\n";
    } else {
        std::cout << (action == "nudge" ? "NUDGE" : "BLOCKED") << " by rule " << (*r).value("rule", "") << ", until "
                  << local((*r)["until"]) << "\n";
        if (!(*r).value("goal", "").empty()) std::cout << "Your goal: " << (*r).value("goal", "") << "\n";
    }
    return kExitOk;
}

// ---------------------------------------------------------------------------------------------------
// Offline commands on a rules file

std::optional<core::State> load_state(const std::string& path) {
    const auto text = platform::read_file(platform::widen(path));
    if (!text) {
        std::cerr << "sirenproof: " << text.error() << "\n";
        return std::nullopt;
    }
    auto state = core::parse_state(*text);
    if (!state) {
        std::cerr << "sirenproof: " << path << ": " << state.error() << "\n";
        return std::nullopt;
    }
    return *std::move(state);
}

core::TimePoint now_or(const std::optional<core::TimePoint>& at) {
    return at.value_or(std::chrono::floor<core::Seconds>(std::chrono::system_clock::now()));
}

int run_check_offline(const Options& o) {
    auto state = load_state(o.state_path);
    if (!state) return kExitFailed;
    const core::TimePoint now = now_or(o.at);
    core::Engine engine(*state, platform::local_offset_at);
    engine.tick(now);

    const std::string& subject = o.positional[1];
    core::Verdict verdict;
    if (looks_like_program(subject)) {
        verdict = engine.decide(core::AppIdentity{subject, "", "", ""}, now);
    } else if (const auto url = core::Url::parse(subject)) {
        verdict = engine.decide(*url, now);
    } else {
        return failed(subject + " isn't a web address or a program");
    }

    std::cout << subject << "\n  at " << local(now) << " local\n";
    if (verdict.action == core::Action::Allow) {
        std::cout << "  ALLOWED: no active rule covers it\n";
        return kExitOk;
    }
    std::cout << "  " << (verdict.action == core::Action::Nudge ? "NUDGE" : "BLOCKED") << " by " << verdict.rule_id
              << ", strength " << core::to_string(verdict.strength);
    if (verdict.until) std::cout << ", until " << local(*verdict.until) << " local";
    std::cout << "\n";
    if (!verdict.goal.empty()) std::cout << "  Your goal: " << verdict.goal << "\n";
    return kExitOk;
}

int run_plan(const Options& o) {
    auto state = load_state(o.state_path);
    if (!state) return kExitFailed;
    const core::TimePoint now = now_or(o.at);
    core::Engine engine(*state, platform::local_offset_at);
    engine.tick(now);
    const core::Plan plan = engine.plan(now);

    std::cout << "At " << local(now) << " local:\n\n";
    if (plan.active_rules.empty()) std::cout << "Nothing is active.\n";
    for (const std::string& id : plan.active_rules) std::cout << "Active: " << id << "\n";
    std::cout << "\nWhole sites for the hosts file:\n";
    if (plan.hosts.empty()) std::cout << "  (none)\n";
    for (const core::Domain& d : plan.hosts) std::cout << "  " << d.str() << "\n";
    for (const auto& [title, entries] : {std::pair{"Browser rules", &plan.web}, std::pair{"Program rules", &plan.apps}}) {
        std::cout << "\n" << title << ":\n";
        if (entries->empty()) std::cout << "  (none)\n";
        for (const core::PlanEntry& e : *entries) {
            std::cout << std::format("  {:<6} {:<40} {}\n", core::to_string(e.action), e.target, e.rule_id);
        }
    }
    std::cout << "\nNext change: " << (plan.next_change ? local(*plan.next_change) + " local" : "none scheduled") << "\n";
    return kExitOk;
}

int run_validate(const Options& o) {
    if (o.positional.size() != 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    const auto state = load_state(o.positional[1]);
    if (!state) return kExitFailed;
    std::cout << o.positional[1] << ": OK, " << state->rules.size() << " rule(s)\n";
    return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty() || args[0] == "help" || args[0] == "--help" || args[0] == "-h") {
        std::cout << kUsage;
        return args.empty() ? kExitUsage : kExitOk;
    }
    if (args[0] == "--version") {
        std::cout << "sirenproof " << sirenproof::kVersion << "\n";
        return kExitOk;
    }

    const auto options = parse_options(args);
    if (!options || options->positional.empty()) return kExitUsage;
    const Options& o = *options;
    const std::string& command = o.positional[0];

    if (command == "validate") return run_validate(o);
    if (command == "plan") {
        if (o.state_path.empty()) return failed("plan needs --state <file>; for the live state use: sirenproof status");
        return run_plan(o);
    }
    if (command == "check") {
        if (o.positional.size() != 2) {
            std::cerr << kUsage;
            return kExitUsage;
        }
        return o.state_path.empty() ? run_check_service(o) : run_check_offline(o);
    }
    if (command == "status") return run_status(o);
    if (command == "rules") return run_rules(o);
    if (command == "block") return run_block(o);
    if (command == "remove") return run_simple(o, "rules.remove", "id");
    if (command == "cancel") return run_simple(o, "pending.cancel", "id");
    if (command == "continue") return run_simple(o, "nudge.continue", "rule");
    if (command == "break") {
        auto r = call(o, "break.start");
        return r ? report_action(*r) : failed(r.error());
    }
    if (command == "unlock") return run_unlock(o);

    std::cerr << "sirenproof: unknown command " << command << "\n\n" << kUsage;
    return kExitUsage;
}
