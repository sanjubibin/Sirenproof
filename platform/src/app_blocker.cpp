#include "sirenproof/platform/app_blocker.hpp"

#include <algorithm>
#include <cwctype>
#include <format>
#include <set>

#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::platform {

namespace {

constexpr auto kReinspectAfter = std::chrono::seconds{60};  // in case a process id was reused

constexpr std::wstring_view kOwnPrograms[] = {L"sirenproof.exe", L"sirenproof-service.exe", L"sirenproof-tray.exe",
                                              L"sirenproof-ui.exe", L"sirenproof-host.exe"};

std::wstring lower(std::wstring text) {
    for (wchar_t& c : text) c = static_cast<wchar_t>(towlower(c));
    return text;
}

}  // namespace

AppBlocker::AppBlocker(bool dry_run, Logger& log, std::chrono::seconds grace)
    : dry_run_(dry_run), log_(log), grace_(grace), self_pid_(GetCurrentProcessId()) {
    wchar_t windows[MAX_PATH] = {};
    const UINT length = GetWindowsDirectoryW(windows, MAX_PATH);
    windows_dir_ = lower(std::wstring(windows, length));
    if (!windows_dir_.empty() && windows_dir_.back() != L'\\') windows_dir_ += L'\\';
    own_dir_ = lower(executable_dir().native());
    if (!own_dir_.empty() && own_dir_.back() != L'\\') own_dir_ += L'\\';
}

void AppBlocker::reset() {
    processes_.clear();
    rule_since_.clear();
}

bool AppBlocker::eligible(const ProcessDetails& details) const {
    if (details.pid == self_pid_ || details.session == 0 || details.critical) return false;
    const std::wstring path = lower(widen(details.identity.path));
    if (!windows_dir_.empty() && path.starts_with(windows_dir_)) return false;
    // Sirenproof's own programs, by name and by where they are: a game renamed sirenproof.exe somewhere
    // else is still a game, and Sirenproof's folder is one only administrators can write to.
    if (own_dir_.empty() || !path.starts_with(own_dir_)) return true;
    const std::wstring name = path.substr(own_dir_.size());
    return std::ranges::find(kOwnPrograms, name) == std::ranges::end(kOwnPrograms);
}

int AppBlocker::sweep(const Decide& decide, const std::vector<std::string>& active_rules, Clock::time_point now) {
    // When did each rule's block begin, as far as we've seen?
    std::erase_if(rule_since_, [&](const auto& entry) {
        return std::ranges::find(active_rules, entry.first) == active_rules.end();
    });
    for (const std::string& rule : active_rules) rule_since_.try_emplace(rule, now);

    std::set<std::uint32_t> alive;
    int closed = 0;
    for (const ProcessEntry& entry : list_processes()) {
        alive.insert(entry.pid);
        auto it = processes_.find(entry.pid);
        if (it == processes_.end() || it->second.exe_name != entry.exe_name || now - it->second.inspected > kReinspectAfter) {
            Tracked tracked;
            tracked.exe_name = entry.exe_name;
            tracked.inspected = now;
            if (auto details = inspect_process(entry.pid)) {
                tracked.details = *std::move(details);
                tracked.eligible = eligible(tracked.details);
                if (tracked.eligible) identities_.complete(tracked.details.identity);
            } else {
                tracked.eligible = false;  // gone already, or not ours to inspect
            }
            if (it != processes_.end() && it->second.details.started == tracked.details.started) {
                tracked.close_at = it->second.close_at;  // same process: keep its countdown and its pause
                tracked.reported = it->second.reported;
                tracked.pause_id = it->second.pause_id;
                tracked.pause_rule = it->second.pause_rule;
                tracked.paused_since = it->second.paused_since;
                tracked.pause_dismissed = it->second.pause_dismissed;
            }
            it = processes_.insert_or_assign(entry.pid, std::move(tracked)).first;
        }

        Tracked& process = it->second;
        if (!process.eligible) continue;

        const core::Verdict verdict = decide(process.details.identity);
        const std::string& path = process.details.identity.path;
        if (verdict.action != core::Action::Nudge) {
            process.pause_id.clear();
            process.pause_dismissed = false;
        }
        if (verdict.action == core::Action::Allow) {
            process.close_at.reset();
            process.reported = false;
            continue;
        }
        if (verdict.action == core::Action::Nudge) {
            process.close_at.reset();
            if (process.pause_id.empty() && !process.pause_dismissed) {
                process.pause_id = "n" + std::to_string(++pauses_);
                process.paused_since = now;
                log_.info(std::format("paused {} (rule {}) until it's continued or closed", path, verdict.rule_id));
            }
            process.pause_rule = verdict.rule_id;
            continue;
        }

        if (!process.close_at) {
            const auto since = rule_since_.contains(verdict.rule_id) ? rule_since_[verdict.rule_id] : now;
            const bool was_running_before = process.details.started < std::chrono::time_point_cast<std::chrono::milliseconds>(since);
            process.close_at = was_running_before ? now + grace_ : now;
            if (was_running_before) {
                log_.info(std::format("{} is blocked by rule {}; closing it in {} s so you can save your work", path,
                                      verdict.rule_id, grace_.count()));
            }
        }
        if (now < *process.close_at) continue;

        if (dry_run_) {
            if (!process.reported) log_.info(std::format("dry run: would close {} (rule {})", path, verdict.rule_id));
            process.reported = true;
            continue;
        }
        if (terminate_process(process.details.pid, process.details.started)) {
            ++closed;
            log_.info(std::format("closed {} (rule {})", path, verdict.rule_id));
            processes_.erase(it);
        } else if (!process.reported) {
            log_.warn(std::format("couldn't close {}: {}", path, last_error_text()));
            process.reported = true;
        }
    }

    std::erase_if(processes_, [&](const auto& entry) { return !alive.contains(entry.first); });
    return closed;
}

std::vector<AppBlocker::Paused> AppBlocker::paused() const {
    std::vector<Paused> waiting;
    for (const auto& [pid, process] : processes_) {
        if (process.pause_id.empty()) continue;
        waiting.push_back({process.pause_id, pid, process.details.identity.path, process.pause_rule, process.paused_since});
    }
    std::ranges::sort(waiting, {}, &Paused::since);
    return waiting;
}

std::expected<void, std::string> AppBlocker::close_paused(std::string_view id) {
    const auto it = std::ranges::find_if(processes_, [&](const auto& entry) { return entry.second.pause_id == id; });
    if (it == processes_.end()) return std::unexpected("that program isn't waiting at the pause any more");
    Tracked& process = it->second;
    const std::string& path = process.details.identity.path;
    if (dry_run_) {
        log_.info(std::format("dry run: would close {} (rule {})", path, process.pause_rule));
        process.pause_id.clear();
        process.pause_dismissed = true;
        return {};
    }
    if (!terminate_process(process.details.pid, process.details.started)) {
        return std::unexpected("couldn't close it: " + last_error_text());
    }
    log_.info(std::format("closed {} at the pause (rule {})", path, process.pause_rule));
    processes_.erase(it);
    return {};
}

}  // namespace sirenproof::platform
