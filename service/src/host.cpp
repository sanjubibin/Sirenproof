#include "host.hpp"

#include <algorithm>
#include <cstdlib>
#include <format>

#include "sirenproof/core/state_json.hpp"
#include "sirenproof/platform/files.hpp"

namespace sirenproof::service {

namespace {

constexpr auto kCheckpointEvery = std::chrono::seconds{30};
constexpr DWORD kStepMs = 1000;

std::string format_offset(core::Minutes m) {
    const auto total = m.count();
    return std::format("UTC{}{:02}:{:02}", total < 0 ? '-' : '+', std::abs(total) / 60, std::abs(total) % 60);
}

}  // namespace

Host::Host(HostOptions options, platform::Logger& log)
    : options_(std::move(options)),
      log_(log),
      engine_(state_, [this](core::TimePoint) { return state_.local_offset.value_or(core::Minutes{0}); }),
      store_(options_.data_dir),
      hosts_(options_.hosts_file, options_.dry_run, log),
      apps_(options_.dry_run, log),
      policies_(options_.policies_root, options_.policies_path, options_.data_dir / L"policies.json", options_.dry_run, log),
      browsers_(options_.dry_run || !options_.guard_browsers, log),
      wake_(CreateEventW(nullptr, FALSE, FALSE, nullptr)) {}

Host::~Host() { finish(); }

std::expected<void, std::string> Host::start() {
    const std::optional<std::wstring_view> acl =
        options_.secure_data_dir ? std::optional{platform::kSystemAndAdminsOnly} : std::nullopt;
    if (auto ok = platform::ensure_trusted_directory(options_.data_dir, acl); !ok) return std::unexpected(ok.error());

    auto loaded = store_.load();
    if (!loaded) return std::unexpected("can't load the saved state: " + loaded.error());
    if (!loaded->warning.empty()) log_.warn(loaded->warning);
    state_ = std::move(loaded->state);

    const core::ClockReading reading = platform::read_clock();
    const core::TimeChangeArbiter arbiter = [this](core::TimePoint from, core::TimePoint to) {
        return engine_.time_change_is_safe(from, to);
    };
    clock_ = state_.clock ? core::TrustedClock(*state_.clock, reading, arbiter) : core::TrustedClock(reading);
    if (!state_.local_offset) state_.local_offset = platform::local_offset_at(clock_->now());

    log_.info(std::format("started: {} rule(s), {}, time zone {}, hosts file {}", state_.rules.size(),
                          options_.dry_run ? "DRY RUN (nothing is enforced)" : "enforcing",
                          format_offset(*state_.local_offset), platform::narrow(options_.hosts_file.native())));

    pipe_.emplace(options_.pipe_name,
                  [this](std::string_view request, const platform::PipeClient& client) { return handle(request, client); },
                  log_);
    if (auto ok = pipe_->start(); !ok) return std::unexpected(ok.error());

    const std::scoped_lock lock(mutex_);
    step();
    save(true);
    return {};
}

void Host::run(HANDLE stop, const std::function<void(bool)>& on_stoppable) {
    std::optional<bool> stoppable;
    platform::DirectoryWatch hosts_watch(options_.hosts_file.parent_path());
    for (;;) {
        HANDLE waits[3] = {stop, wake_.get(), hosts_watch.event()};
        const DWORD count = hosts_watch.valid() ? 3 : 2;
        const DWORD which = WaitForMultipleObjects(count, waits, FALSE, kStepMs);
        if (which == WAIT_OBJECT_0) break;
        if (which == WAIT_OBJECT_0 + 2) hosts_watch.rearm();

        bool now_stoppable = false;
        {
            const std::scoped_lock lock(mutex_);
            step();
            save(false);
            now_stoppable = can_stop_locked(now());
        }
        if (stoppable != now_stoppable) {
            stoppable = now_stoppable;
            on_stoppable(now_stoppable);
        }
    }
}

void Host::finish() {
    if (pipe_) {
        pipe_->stop();
        pipe_.reset();
    }
    const std::scoped_lock lock(mutex_);
    if (clock_) save(true);
}

bool Host::can_stop() {
    const std::scoped_lock lock(mutex_);
    return can_stop_locked(now());
}

bool Host::can_stop_locked(core::TimePoint t) const {
    return std::ranges::none_of(state_.rules, [&](const core::Rule& rule) { return engine_.lock_holds(rule, t); });
}

void Host::warn_once(std::string& last, const std::string& message) {
    if (message != last) log_.warn(message);
    last = message;
}

void Host::step() {
    // 1. Trusted time. A clock change that would loosen something in force is ignored.
    clock_->update(platform::read_clock(), [this](core::TimePoint from, core::TimePoint to) {
        return engine_.time_change_is_safe(from, to);
    });
    const core::TimePoint t = now();
    if (clock_->last_jump_ignored()) {
        warn_once(last_clock_warning_, std::format("the Windows clock is {} s away from trusted time; ignoring it while "
                                                   "that would end a block early",
                                                   std::chrono::duration_cast<std::chrono::seconds>(clock_->offset()).count()));
    } else {
        last_clock_warning_.clear();
    }

    // 2. Time zone: same rule as the clock.
    const core::Minutes zone = platform::local_offset_at(t);
    if (zone != state_.local_offset) {
        if (engine_.offset_change_is_safe(t, [zone](core::TimePoint) { return zone; })) {
            log_.info(std::format("time zone is now {}", format_offset(zone)));
            state_.local_offset = zone;
            last_zone_warning_.clear();
        } else {
            warn_once(last_zone_warning_, std::format("time zone changed to {}; keeping {} while that would end a "
                                                      "block early",
                                                      format_offset(zone), format_offset(*state_.local_offset)));
        }
    }

    // 3. Debug builds only: an escape hatch for developers who lock themselves out while testing.
    if (options_.debug_limits) {
        const auto hatch = options_.data_dir / L"dev-unlock";
        if (std::filesystem::exists(hatch)) {
            std::error_code ignored;
            std::filesystem::remove(hatch, ignored);
            state_.rules.clear();
            state_.pending.clear();
            log_.warn("dev-unlock file found: all rules removed (debug builds only)");
        }
    }

    // 4. Rules and enforcement.
    engine_.tick(t);
    const core::Plan plan = engine_.plan(t);

    if (auto changed = hosts_.enforce(plan.hosts); !changed) {
        warn_once(last_hosts_error_, "can't update the hosts file: " + changed.error());
    } else {
        last_hosts_error_.clear();
    }

    if (plan.apps.empty()) {
        apps_.reset();
    } else {
        apps_.sweep([&](const core::AppIdentity& app) { return engine_.decide(app, t); }, plan.active_rules,
                    std::chrono::system_clock::now());
    }

    // 5. Browsers, while a lock that blocks websites is in force: private windows and DNS-over-HTTPS
    // off, and every open browser must have the extension connected.
    web_lock_ = std::ranges::any_of(plan.active_rules, [&](const std::string& id) {
        const core::Rule* rule = engine_.find_rule(id);
        return rule && rule->strength == core::Strength::Lock && std::ranges::any_of(rule->block, core::is_web_target);
    });
    if (auto set = policies_.set(web_lock_); !set) {
        warn_once(last_policy_error_, "can't set browser policies: " + set.error());
    } else {
        last_policy_error_.clear();
    }
    if (web_lock_) {
        browsers_.enforce(std::chrono::steady_clock::now());
    } else {
        browsers_.relax();
    }
}

void Host::save(bool force) {
    core::State without_clock = state_;
    without_clock.clock.reset();
    std::string text = core::serialise_state(without_clock);
    const auto elapsed = std::chrono::steady_clock::now() - last_save_;
    if (!force && text == last_saved_ && elapsed < kCheckpointEvery) return;

    state_.clock = clock_->checkpoint();
    if (auto saved = store_.save(state_); !saved) {
        log_.error("can't save the state: " + saved.error());
        return;
    }
    last_saved_ = std::move(text);
    last_save_ = std::chrono::steady_clock::now();
}

}  // namespace sirenproof::service
