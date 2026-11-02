#include "service_client.hpp"

#include <chrono>
#include <optional>
#include <variant>

#include "json_variant.hpp"
#include "sirenproof/core/command.hpp"
#include "sirenproof/core/target.hpp"
#include "sirenproof/core/time.hpp"
#include "sirenproof/platform/paths.hpp"
#include "sirenproof/platform/pipe.hpp"
#include "sirenproof/platform/win.hpp"

namespace sirenproof::ui {

namespace {

using core::Json;

QVariantMap error(const std::string& message) { return {{"error", QString::fromStdString(message)}}; }

bool windows_animations_on() {
    BOOL on = TRUE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &on, 0);
    return on != FALSE;
}

constexpr int kHoursPerWeek = 7 * 24;

// The minutes of the week a rule applies, or nullopt for a one-off session.
std::optional<core::WeekMinutes> week_minutes(const QVariant& when) {
    try {
        const core::When parsed = core::when_from_json(from_variant(when), "when");
        if (std::holds_alternative<core::Always>(parsed)) return core::WeekMinutes{}.set();
        if (const auto* weekly = std::get_if<core::WeeklySchedule>(&parsed)) return weekly->minutes();
    } catch (const core::JsonError&) {
    }
    return std::nullopt;
}

}  // namespace

ServiceClient::ServiceClient(std::wstring pipe, QObject* parent)
    : QObject(parent), pipe_(std::move(pipe)), animations_(windows_animations_on()) {
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &ServiceClient::refresh);
    timer_.start();
    refresh();
}

QVariantMap ServiceClient::call(const std::string& op, const Json& args) {
    const Json request{{"v", 1}, {"op", op}, {"args", args}};
    const auto reply = platform::pipe_call(pipe_, request.dump(),
                                           {std::chrono::seconds{2}, platform::executable_dir() / L"sirenproof-service.exe"});
    if (!reply) return error(reply.error());
    const Json answer = Json::parse(*reply, nullptr, false);
    if (answer.is_discarded() || !answer.is_object()) return error("the service sent something unreadable");
    if (!answer.value("ok", false)) return error(answer.value("error", std::string("unknown error")));
    return to_variant(answer["result"]).toMap();
}

void ServiceClient::refresh() {
    const QVariantMap status = call("status", Json::object());
    const bool connected = !status.contains("error");
    if (connected) {
        status_ = status;
        const QVariantMap listed = call("rules.list", Json::object());
        if (!listed.contains("error")) rules_ = listed.value("rules").toList();
        problem_.clear();
    } else {
        problem_ = status.value("error").toString();
    }
    connected_ = connected;
    emit changed();
}

QVariantMap ServiceClient::startLock(const QVariantList& targets, int minutes, const QString& unlock, const QString& goal) {
    // The service sets the real start and end from its trusted clock; these only have to be valid.
    const auto now = std::chrono::floor<core::Seconds>(std::chrono::system_clock::now());
    Json rule{{"name", "Focus lock"},
              {"goal", goal.toStdString()},
              {"block", from_variant(targets)},
              {"when", {{"from", core::format_time(now)}, {"to", core::format_time(now + core::Minutes{minutes})}}},
              {"strength", "lock"}};
    if (unlock == "wait") rule["unlock"] = {{"method", "wait"}, {"wait_minutes", 30}};
    if (unlock == "type") rule["unlock"] = {{"method", "type"}, {"length", 200}};
    if (unlock == "friend") rule["unlock"] = {{"method", "friend"}};
    const QVariantMap result = call("rules.add", Json{{"rule", rule}, {"for_minutes", minutes}});
    refresh();
    return result;
}

QVariantMap ServiceClient::update_rule(const QString& id, const std::function<void(Json&)>& edit) {
    for (const QVariant& listed : rules_) {
        if (listed.toMap().value("id").toString() != id) continue;
        Json rule = from_variant(listed);
        edit(rule);
        const QVariantMap result = call("rules.update", Json{{"rule", rule}});
        refresh();
        return result;
    }
    return error("there is no rule called " + id.toStdString());
}

QVariantList ServiceClient::readyLists() const {
    QVariantList lists;
    for (const core::ReadyList& list : core::ready_lists()) {
        QStringList targets;
        for (const std::string_view t : list.targets) targets.push_back(QString::fromUtf8(t.data(), static_cast<qsizetype>(t.size())));
        const auto text = [](std::string_view s) { return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size())); };
        lists.push_back(QVariantMap{{"id", text(list.id)},
                                    {"label", text(list.label)},
                                    {"detail", text(list.detail)},
                                    {"effect", text(list.effect)},
                                    {"targets", targets}});
    }
    return lists;
}

QVariantMap ServiceClient::addRule(const QVariantList& targets, int minutes, const QString& strength) {
    core::Rule rule;
    for (const QVariant& value : targets) {
        auto target = core::parse_target(value.toString().toStdString());
        if (!target) return error(value.toString().toStdString() + " isn't a site, page or app");
        rule.block.push_back(*std::move(target));
    }
    if (rule.block.empty()) return error("there's nothing to block");
    rule.name = core::to_string(rule.block.front());
    if (rule.block.size() > 1) rule.name += " and " + std::to_string(rule.block.size() - 1) + " more";
    rule.strength = strength == "nudge" ? core::Strength::Nudge : core::Strength::Block;
    if (rule.strength == core::Strength::Nudge) rule.loosen_delay = core::Minutes{2};  // as the command line does

    // The service sets the real start and end from its trusted clock; these only have to be valid.
    const auto now = std::chrono::floor<core::Seconds>(std::chrono::system_clock::now());
    if (minutes > 0) rule.when = core::Span{now, now + core::Minutes{minutes}};
    Json rule_json = core::rule_to_json(rule);
    rule_json.erase("id");  // the service picks one
    Json args{{"rule", rule_json}};
    if (minutes > 0) args["for_minutes"] = minutes;
    const QVariantMap result = call("rules.add", args);
    refresh();
    return result;
}

QVariantMap ServiceClient::addTarget(const QString& rule, const QString& target) {
    return update_rule(rule, [&](Json& r) { r["block"].push_back(target.trimmed().toStdString()); });
}

QVariantMap ServiceClient::removeTarget(const QString& rule, const QString& target) {
    return update_rule(rule, [&](Json& r) {
        Json kept = Json::array();
        for (const auto& t : r["block"]) {
            if (t != target.toStdString()) kept.push_back(t);
        }
        r["block"] = kept;
    });
}

QVariantMap ServiceClient::setStrength(const QString& rule, const QString& strength) {
    return update_rule(rule, [&](Json& r) { r["strength"] = strength.toStdString(); });
}

QVariantMap ServiceClient::setWhen(const QString& rule, const QVariant& when) {
    return update_rule(rule, [&](Json& r) { r["when"] = from_variant(when); });
}

QVariantList ServiceClient::weekHours(const QVariant& when) const {
    QVariantList hours;
    const auto minutes = week_minutes(when);
    if (!minutes) return hours;
    for (int hour = 0; hour < kHoursPerWeek; ++hour) {
        int on = 0;
        for (int m = 0; m < 60; ++m) on += minutes->test(static_cast<std::size_t>(hour * 60 + m)) ? 1 : 0;
        hours.push_back(on == 60 ? 2 : on > 0 ? 1 : 0);
    }
    return hours;
}

QVariant ServiceClient::paintHours(const QVariant& when, const QVariantList& hours, bool on) const {
    auto minutes = week_minutes(when);
    if (!minutes) return {};
    for (const QVariant& value : hours) {
        const int hour = value.toInt();
        if (hour < 0 || hour >= kHoursPerWeek) continue;
        for (int m = 0; m < 60; ++m) minutes->set(static_cast<std::size_t>(hour * 60 + m), on);
    }
    // Every hour on is "always", which the service can tell is no looser than any schedule.
    if (minutes->all()) return QStringLiteral("always");
    const auto schedule = core::WeeklySchedule::from_minutes(*minutes);
    if (!schedule) return {};
    return to_variant(core::when_to_json(*schedule));
}

QVariantMap ServiceClient::parseCommand(const QString& text) const {
    const auto parsed = core::parse_command(text.toStdString());
    if (!parsed) {
        return {{"error", QString::fromStdString(parsed.error().message)}, {"known", parsed.error().known_verb}};
    }
    const core::Command& c = *parsed;
    static constexpr const char* kKinds[] = {"block", "lock", "break", "check", "go"};
    static constexpr const char* kStrengths[] = {"nudge", "block", "lock"};

    QStringList lists;
    QStringList all_targets;  // the lists' targets and the rest, for running it as one rule
    for (const std::string& id : c.lists) {
        lists.push_back(QString::fromStdString(id));
        if (const core::ReadyList* list = core::find_ready_list(id)) {
            for (const std::string_view t : list->targets) {
                const QString target = QString::fromUtf8(t.data(), static_cast<qsizetype>(t.size()));
                if (!all_targets.contains(target)) all_targets.push_back(target);
            }
        }
    }
    QStringList targets;
    for (const std::string& t : c.targets) {
        targets.push_back(QString::fromStdString(t));
        if (!all_targets.contains(targets.back())) all_targets.push_back(targets.back());
    }
    return {{"kind", kKinds[static_cast<int>(c.kind)]},
            {"lists", lists},
            {"targets", targets},
            {"all_targets", all_targets},
            {"minutes", c.length ? static_cast<int>(c.length->count()) : 0},
            {"strength", kStrengths[static_cast<int>(c.strength)]},
            {"unlock", QString::fromStdString(c.unlock)},
            {"subject", QString::fromStdString(c.subject)},
            {"screen", QString::fromStdString(c.screen)}};
}

QVariantMap ServiceClient::removeRule(const QString& rule) {
    const QVariantMap result = call("rules.remove", Json{{"id", rule.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::cancelPending(const QString& id) {
    const QVariantMap result = call("pending.cancel", Json{{"id", id.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::startBreak() {
    const QVariantMap result = call("break.start", Json::object());
    refresh();
    return result;
}

QVariantMap ServiceClient::requestUnlock(const QString& rule) {
    const QVariantMap result = call("unlock.request", Json{{"rule", rule.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::cancelUnlock(const QString& rule) {
    const QVariantMap result = call("unlock.cancel", Json{{"rule", rule.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::beginTypeUnlock(const QString& rule) {
    return call("unlock.type.begin", Json{{"rule", rule.toStdString()}});
}

QVariantMap ServiceClient::finishTypeUnlock(const QString& rule, const QString& text) {
    const QVariantMap result = call("unlock.type.finish", Json{{"rule", rule.toStdString()}, {"text", text.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::enterFriendCode(const QString& rule, const QString& code) {
    const QVariantMap result = call("unlock.friend", Json{{"rule", rule.toStdString()}, {"code", code.toStdString()}});
    refresh();
    return result;
}

QVariantMap ServiceClient::check(const QString& subject) {
    return call("check", Json{{"subject", subject.toStdString()}});
}

}  // namespace sirenproof::ui
