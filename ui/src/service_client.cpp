#include "service_client.hpp"

#include <chrono>

#include "json_variant.hpp"
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
