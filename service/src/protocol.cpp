// The pipe protocol. One JSON object per message:
//
//   request  {"v": 1, "op": "rules.add", "args": {...}}
//   reply    {"ok": true, "result": {...}}  or  {"ok": false, "error": "why"}
//
// Every op is something the interactive user may do, except the ext.* ops, which only the browser
// relay may use (and it may use nothing else). Nothing here can run a program, write an address other
// than 0.0.0.0 / :: or touch files outside the service's own; the worst a client can do is block more.

#include <format>

#include "host.hpp"
#include "sirenproof/core/browser.hpp"
#include "sirenproof/core/json_codec.hpp"
#include "sirenproof/core/version.hpp"
#include "sirenproof/platform/paths.hpp"

namespace sirenproof::service {

namespace {

using core::Json;
using core::JsonError;

std::string text_arg(const Json& args, const char* key) {
    const auto it = args.find(key);
    if (it == args.end() || !it->is_string()) throw JsonError(std::format("args.{}: expected text", key));
    return it->get<std::string>();
}

const Json& object_arg(const Json& args, const char* key) {
    const auto it = args.find(key);
    if (it == args.end() || !it->is_object()) throw JsonError(std::format("args.{}: expected an object", key));
    return *it;
}

Json optional_time(const std::optional<core::TimePoint>& t) { return t ? Json(core::format_time(*t)) : Json(nullptr); }

// "steam.exe" is also a valid host name, so programs are told apart by shape first.
bool looks_like_program(std::string_view subject) {
    if (subject.find('\\') != std::string_view::npos) return true;
    if (subject.size() < 4) return false;
    std::string ending(subject.substr(subject.size() - 4));
    for (char& c : ending) c = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
    return ending == ".exe";
}

// Rules as clients see them: never the friend code's salt and hash.
Json public_rule(const core::Rule& rule) {
    Json j = core::rule_to_json(rule);
    if (j.contains("unlock")) {
        j["unlock"].erase("salt");
        j["unlock"].erase("hash");
    }
    return j;
}

Json change_json(const core::ChangeResult& r) {
    static constexpr const char* kOutcomes[] = {"applied", "pending", "refused"};
    Json j{{"outcome", kOutcomes[static_cast<int>(r.outcome)]}, {"message", r.message}};
    if (!r.rule_id.empty()) j["rule"] = r.rule_id;
    if (!r.pending_id.empty()) j["pending"] = r.pending_id;
    if (r.applies_at) j["applies_at"] = core::format_time(*r.applies_at);
    if (!r.friend_code.empty()) j["friend_code"] = r.friend_code;
    return j;
}

Json action_json(const core::ActionResult& r) {
    return Json{{"ok", r.ok}, {"message", r.message}, {"until", optional_time(r.until)}};
}

Json verdict_json(const core::Verdict& v) {
    return Json{{"action", std::string(core::to_string(v.action))},
                {"rule", v.rule_id},
                {"strength", std::string(core::to_string(v.strength))},
                {"until", optional_time(v.until)},
                {"goal", v.goal}};
}

Json entries_json(const std::vector<core::PlanEntry>& entries) {
    Json list = Json::array();
    for (const auto& e : entries) {
        list.push_back({{"target", e.target}, {"action", std::string(core::to_string(e.action))}, {"rule", e.rule_id}});
    }
    return list;
}

std::string reply_ok(Json result) { return Json{{"ok", true}, {"result", std::move(result)}}.dump(); }
std::string reply_error(std::string_view message) { return Json{{"ok", false}, {"error", std::string(message)}}.dump(); }

}  // namespace

std::string Host::handle(std::string_view request, const platform::PipeClient& client) {
    const Json message = Json::parse(request, nullptr, false);
    if (message.is_discarded() || !message.is_object()) return reply_error("not a JSON object");
    if (message.contains("v") && message["v"] != 1) return reply_error("unsupported protocol version");
    if (!message.contains("op") || !message["op"].is_string()) return reply_error("missing op");
    const std::string op = message["op"].get<std::string>();
    const Json args = message.contains("args") ? message["args"] : Json::object();
    if (!args.is_object()) return reply_error("args must be an object");

    // The browser relay (sirenproof-host.exe from this folder) may only use the extension operations,
    // and only it may: a heartbeat from anything else could fake "this browser has the extension".
    const std::filesystem::path caller = platform::process_image_path(client.pid);
    const std::wstring relay = (platform::executable_dir() / L"sirenproof-host.exe").native();
    const bool from_relay =
        !caller.empty() && CompareStringOrdinal(caller.c_str(), -1, relay.c_str(), -1, TRUE) == CSTR_EQUAL;
    if (core::is_extension_op(op) != from_relay) {
        return reply_error(from_relay ? "the browser relay may only ask about pages"
                                      : "only Sirenproof's browser relay may do that");
    }
    // Which browser the relay serves comes from the process tree, never from the message.
    std::optional<std::filesystem::path> browser;
    if (op == "ext.hello" || op == "ext.heartbeat") browser = platform::browser_ancestor(client.pid);

    try {
        const std::scoped_lock lock(mutex_);
        const core::TimePoint t = now();
        bool changed = true;
        Json result;

        if (op == "status") {
            changed = false;
            const core::Plan plan = engine_.plan(t);
            Json pending = Json::array();
            for (const auto& p : state_.pending) {
                std::string what = std::holds_alternative<core::ChangeSettings>(p.change) ? "settings"
                                   : std::holds_alternative<core::RemoveRule>(p.change)
                                       ? "remove " + std::get<core::RemoveRule>(p.change).rule_id
                                       : "change " + std::get<core::ReplaceRule>(p.change).rule.id;
                pending.push_back({{"id", p.id}, {"what", what}, {"applies_at", core::format_time(p.apply_at)}});
            }
            Json unlocking = Json::array();
            for (const auto& u : state_.unlock_requests) unlocking.push_back({{"rule", u.rule_id}, {"due", core::format_time(u.due)}});
            Json locked = Json::array();
            for (const auto& rule : state_.rules) {
                if (!engine_.lock_holds(rule, t)) continue;
                static constexpr const char* kMethods[] = {"none", "wait", "type", "friend"};
                const auto* span = std::get_if<core::Span>(&rule.when);
                locked.push_back({{"rule", rule.id},
                                  {"name", rule.name},
                                  {"goal", rule.goal},
                                  {"from", span ? Json(core::format_time(span->start)) : Json(nullptr)},
                                  {"until", optional_time(engine_.period_end(rule, t))},
                                  {"unlock", kMethods[static_cast<int>(rule.unlock.kind)]},
                                  {"breaks_allowed", rule.breaks_allowed}});
            }
            Json hosts = Json::array();
            for (const auto& d : plan.hosts) hosts.push_back(d.str());
            result = Json{{"version", std::string(kVersion)},
                          {"now", core::format_time(t)},
                          {"clock_offset_seconds", std::chrono::duration_cast<std::chrono::seconds>(clock_->offset()).count()},
                          {"local_offset_minutes", state_.local_offset.value_or(core::Minutes{0}).count()},
                          {"dry_run", options_.dry_run},
                          {"active_rules", plan.active_rules},
                          {"locked", locked},
                          {"hosts", hosts},
                          {"web", entries_json(plan.web)},
                          {"apps", entries_json(plan.apps)},
                          {"next_change", optional_time(plan.next_change)},
                          {"pending", pending},
                          {"unlocking", unlocking},
                          {"breaks", {{"used_today", state_.breaks.used}, {"per_day", state_.settings.breaks_per_day}}},
                          {"can_stop", can_stop_locked(t)},
                          {"browser_policies", web_lock_},
                          {"browsers_without_extension", browsers_.unprotected()},
                          {"log", log_.recent(15)}};
        } else if (op == "rules.list") {
            changed = false;
            Json rules = Json::array();
            for (const auto& rule : state_.rules) rules.push_back(public_rule(rule));
            result = Json{{"rules", rules}, {"settings", core::settings_to_json(state_.settings)}};
        } else if (op == "rules.add" || op == "rules.update") {
            // A new rule may come without an id; the engine then picks one (r1, r2, ...).
            Json rule_json = object_arg(args, "rule");
            const bool pick_id = op == "rules.add" && !rule_json.contains("id");
            if (pick_id) rule_json["id"] = "new";
            core::Rule rule = core::rule_from_json(rule_json, "rule");
            // "for_minutes" starts a session now, by trusted time, so a client with a wrong clock can't
            // shorten it.
            if (const auto it = args.find("for_minutes"); it != args.end()) {
                if (!it->is_number_integer() || it->get<std::int64_t>() < 1 || it->get<std::int64_t>() > 7 * 24 * 60) {
                    throw JsonError("args.for_minutes: expected 1 to 10080");
                }
                rule.when = core::Span{t, t + core::Minutes{it->get<std::int64_t>()}};
                if (auto error = core::validate_rule(rule, state_.settings)) {
                    return reply_ok(change_json({core::ChangeResult::Outcome::Refused, *error, {}, {}, std::nullopt, {}}));
                }
            }
            if (pick_id) rule.id.clear();
            if (options_.debug_limits && rule.strength == core::Strength::Lock) {
                const auto* span = std::get_if<core::Span>(&rule.when);
                if (!span || span->end - span->start > kDebugLockCap) {
                    return reply_ok(change_json({core::ChangeResult::Outcome::Refused,
                                                 "debug builds only allow lock sessions of up to 2 minutes", rule.id, {},
                                                 std::nullopt, {}}));
                }
            }
            result = change_json(op == "rules.add" ? engine_.add_rule(std::move(rule), t, random_)
                                                   : engine_.update_rule(std::move(rule), t, random_));
        } else if (op == "rules.remove") {
            result = change_json(engine_.remove_rule(text_arg(args, "id"), t));
        } else if (op == "settings.update") {
            result = change_json(engine_.update_settings(core::settings_from_json(object_arg(args, "settings"), "settings"), t));
        } else if (op == "pending.cancel") {
            result = Json{{"cancelled", engine_.cancel_pending(text_arg(args, "id"))}};
        } else if (op == "break.start") {
            result = action_json(engine_.start_break(t));
        } else if (op == "nudge.continue") {
            result = action_json(engine_.continue_after_nudge(text_arg(args, "rule"), t));
        } else if (op == "unlock.request") {
            result = action_json(engine_.request_unlock(text_arg(args, "rule"), t));
        } else if (op == "unlock.cancel") {
            result = Json{{"cancelled", engine_.cancel_unlock(text_arg(args, "rule"))}};
        } else if (op == "unlock.type.begin") {
            const auto text = engine_.begin_type_unlock(text_arg(args, "rule"), t, random_);
            if (!text) return reply_error("that rule isn't a lock you can type your way out of right now");
            result = Json{{"text", *text}};
        } else if (op == "unlock.type.finish") {
            result = action_json(engine_.finish_type_unlock(text_arg(args, "rule"), text_arg(args, "text"), t));
        } else if (op == "unlock.friend") {
            result = action_json(engine_.enter_friend_code(text_arg(args, "rule"), text_arg(args, "code"), t));
        } else if (op == "check") {
            changed = false;
            const std::string subject = text_arg(args, "subject");
            if (looks_like_program(subject)) {
                core::AppIdentity app{subject, "", "", ""};
                identities_.complete(app);
                result = verdict_json(engine_.decide(app, t));
            } else if (const auto url = core::Url::parse(subject)) {
                result = verdict_json(engine_.decide(*url, t));
            } else {
                return reply_error(subject + " isn't a web address or a program");
            }
        } else if (op == "service.can_stop") {
            changed = false;
            result = Json{{"can_stop", can_stop_locked(t)}};
        } else if (op == "ext.hello" || op == "ext.heartbeat") {
            changed = false;
            if (browser) browsers_.heartbeat(*browser, std::chrono::steady_clock::now());
            result = Json{{"browser", browser ? Json(platform::narrow(browser->filename().native())) : Json(nullptr)},
                          {"heartbeat_seconds", 10}};
        } else if (op == "ext.decide") {
            changed = false;
            const auto url = core::Url::parse(text_arg(args, "url"));
            const core::Verdict verdict = url ? engine_.decide(*url, t) : core::Verdict{};
            result = verdict_json(verdict);
            if (const core::Rule* rule = engine_.find_rule(verdict.rule_id)) result["rule_name"] = rule->name;
            result["pause_seconds"] = state_.settings.nudge_pause.count();
            result["continue_minutes"] = state_.settings.nudge_continue.count();
        } else if (op == "ext.continue") {
            result = action_json(engine_.continue_after_nudge(text_arg(args, "rule"), t));
        } else {
            return reply_error("unknown op " + op);
        }

        if (changed) {
            log_.info(std::format("{} from process {}", op, client.pid));
            SetEvent(wake_.get());  // enforce straight away
        }
        return reply_ok(std::move(result));
    } catch (const JsonError& e) {
        return reply_error(e.what());
    } catch (const Json::exception& e) {
        return reply_error(std::string("malformed request: ") + e.what());
    }
}

}  // namespace sirenproof::service
