// sirenproof: command-line front end.
//
// In this milestone it only reads a state file and reports what would be blocked. It changes nothing
// on the system.

#include <chrono>
#include <ctime>
#include <format>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "sirenproof/core/policy.hpp"
#include "sirenproof/core/state_json.hpp"
#include "sirenproof/core/version.hpp"

namespace core = sirenproof::core;

namespace {

constexpr int kExitOk = 0;
constexpr int kExitUsage = 1;
constexpr int kExitBadState = 2;

constexpr std::string_view kUsage = R"(Usage:
  sirenproof check <address-or-program> --state <file> [--at <time>]
  sirenproof plan --state <file> [--at <time>]
  sirenproof validate <file>
  sirenproof --version

  <address-or-program>  a web address (youtube.com/shorts/abc) or a program (steam.exe,
                        C:\Games\Steam\steam.exe)
  --state <file>        the rules to evaluate (see examples/state.example.json)
  --at <time>           evaluate at this UTC time, e.g. 2026-11-02T09:30:00Z (default: now)

Nothing is changed on this PC: these commands only report what would happen.
)";

// Offset of local time from UTC at `t`, using the Windows time zone settings.
core::Minutes local_offset(core::TimePoint t) {
    const std::time_t utc = std::chrono::system_clock::to_time_t(t);
    std::tm local{};
    if (localtime_s(&local, &utc) != 0) return core::Minutes{0};
    const std::time_t local_as_utc = _mkgmtime(&local);
    return std::chrono::duration_cast<core::Minutes>(core::Seconds{local_as_utc - utc});
}

std::string describe(core::TimePoint t) {
    return std::format("{:%Y-%m-%d %H:%M} local", t + local_offset(t));
}

std::optional<std::string> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

struct Options {
    std::vector<std::string> positional;
    std::string state_path;
    std::optional<core::TimePoint> at;
};

std::optional<Options> parse_options(const std::vector<std::string>& args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--state" && i + 1 < args.size()) {
            options.state_path = args[++i];
        } else if (args[i] == "--at" && i + 1 < args.size()) {
            options.at = core::parse_time(args[++i]);
            if (!options.at) {
                std::cerr << "sirenproof: --at expects a UTC time like 2026-11-02T09:30:00Z\n";
                return std::nullopt;
            }
        } else if (args[i].starts_with("--")) {
            std::cerr << "sirenproof: unknown option " << args[i] << "\n";
            return std::nullopt;
        } else {
            options.positional.push_back(args[i]);
        }
    }
    return options;
}

std::optional<core::State> load_state(const std::string& path) {
    if (path.empty()) {
        std::cerr << "sirenproof: --state <file> is required\n";
        return std::nullopt;
    }
    const auto text = read_file(path);
    if (!text) {
        std::cerr << "sirenproof: can't read " << path << "\n";
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

bool looks_like_program(std::string_view text) {
    if (text.find('\\') != std::string_view::npos) return true;
    if (text.size() < 4) return false;
    std::string ending(text.substr(text.size() - 4));
    for (char& c : ending) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return ending == ".exe";
}

std::string rule_label(const core::State& state, const std::string& id) {
    for (const core::Rule& rule : state.rules) {
        if (rule.id == id) return rule.name.empty() ? id : "\"" + rule.name + "\" (" + id + ")";
    }
    return id;
}

int run_check(const Options& options) {
    if (options.positional.size() != 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    auto state = load_state(options.state_path);
    if (!state) return kExitBadState;

    const core::TimePoint now = now_or(options.at);
    core::Engine engine(*state, local_offset);
    engine.tick(now);

    const std::string& subject = options.positional[1];
    core::Verdict verdict;
    if (looks_like_program(subject)) {
        verdict = engine.decide(core::AppIdentity{subject, "", "", ""}, now);
    } else {
        const auto url = core::Url::parse(subject);
        if (!url) {
            std::cerr << "sirenproof: " << subject << " isn't a web address or a program\n";
            return kExitUsage;
        }
        verdict = engine.decide(*url, now);
    }

    std::cout << subject << "\n  at " << describe(now) << "\n";
    if (verdict.action == core::Action::Allow) {
        std::cout << "  ALLOWED: no active rule covers it\n";
        return kExitOk;
    }
    std::cout << "  " << (verdict.action == core::Action::Nudge ? "NUDGE" : "BLOCKED") << " by "
              << rule_label(*state, verdict.rule_id) << ", strength " << core::to_string(verdict.strength);
    if (verdict.until) std::cout << ", until " << describe(*verdict.until);
    std::cout << "\n";
    if (!verdict.goal.empty()) std::cout << "  Your goal: " << verdict.goal << "\n";
    return kExitOk;
}

int run_plan(const Options& options) {
    if (options.positional.size() != 1) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    auto state = load_state(options.state_path);
    if (!state) return kExitBadState;

    const core::TimePoint now = now_or(options.at);
    core::Engine engine(*state, local_offset);
    engine.tick(now);
    const core::Plan plan = engine.plan(now);

    std::cout << "At " << describe(now) << ":\n\n";
    if (plan.active_rules.empty()) {
        std::cout << "Nothing is active.\n";
    } else {
        std::cout << "Active rules:\n";
        for (const std::string& id : plan.active_rules) std::cout << "  " << rule_label(*state, id) << "\n";

        std::cout << "\nWhole sites for the hosts file:\n";
        if (plan.hosts.empty()) std::cout << "  (none)\n";
        for (const core::Domain& d : plan.hosts) std::cout << "  " << d.str() << "\n";

        const auto print_entries = [](std::string_view title, const std::vector<core::PlanEntry>& entries) {
            std::cout << "\n" << title << ":\n";
            if (entries.empty()) std::cout << "  (none)\n";
            for (const core::PlanEntry& e : entries) {
                std::cout << std::format("  {:<6} {:<40} {}\n", core::to_string(e.action), e.target, e.rule_id);
            }
        };
        print_entries("Browser rules", plan.web);
        print_entries("Program rules", plan.apps);
    }
    std::cout << "\nNext change: " << (plan.next_change ? describe(*plan.next_change) : "none scheduled") << "\n";
    return kExitOk;
}

int run_validate(const Options& options) {
    if (options.positional.size() != 2) {
        std::cerr << kUsage;
        return kExitUsage;
    }
    const auto state = load_state(options.positional[1]);
    if (!state) return kExitBadState;
    std::cout << options.positional[1] << ": OK, " << state->rules.size() << " rule(s)\n";
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
    if (!options) return kExitUsage;
    const std::string& command = options->positional.empty() ? args[0] : options->positional[0];
    if (command == "check") return run_check(*options);
    if (command == "plan") return run_plan(*options);
    if (command == "validate") return run_validate(*options);

    std::cerr << "sirenproof: unknown command " << command << "\n\n" << kUsage;
    return kExitUsage;
}
