#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariant>

#include <functional>
#include <string>

#include "sirenproof/core/json_codec.hpp"

namespace sirenproof::ui {

// The window's view of the Sirenproof service. Refreshes the status every second while the window is
// open, and turns each button press into one pipe request. Every action returns the service's
// result, or {"error": "..."} if it couldn't be reached.
class ServiceClient : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
    Q_PROPERTY(QVariantMap status READ status NOTIFY changed)
    Q_PROPERTY(QVariantList rules READ rules NOTIFY changed)
    Q_PROPERTY(bool animations READ animations CONSTANT)
    Q_PROPERTY(QVariantList readyLists READ readyLists CONSTANT)

public:
    explicit ServiceClient(std::wstring pipe, QObject* parent = nullptr);

    bool connected() const { return connected_; }
    QString problem() const { return problem_; }
    QVariantMap status() const { return status_; }
    QVariantList rules() const { return rules_; }
    // Windows' "Animation effects" setting; off means no ambient motion.
    bool animations() const { return animations_; }
    // Video, Social, Games: [{id, label, detail, effect, targets}].
    QVariantList readyLists() const;

    Q_INVOKABLE void refresh();

    Q_INVOKABLE QVariantMap startLock(const QVariantList& targets, int minutes, const QString& unlock, const QString& goal);
    // A new Block or Nudge rule; `minutes` 0 means until you remove it.
    Q_INVOKABLE QVariantMap addRule(const QVariantList& targets, int minutes, const QString& strength);
    Q_INVOKABLE QVariantMap addTarget(const QString& rule, const QString& target);
    Q_INVOKABLE QVariantMap removeTarget(const QString& rule, const QString& target);
    Q_INVOKABLE QVariantMap setStrength(const QString& rule, const QString& strength);
    Q_INVOKABLE QVariantMap setWhen(const QString& rule, const QVariant& when);
    Q_INVOKABLE QVariantMap removeRule(const QString& rule);
    Q_INVOKABLE QVariantMap cancelPending(const QString& id);
    Q_INVOKABLE QVariantMap startBreak();
    Q_INVOKABLE QVariantMap requestUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap cancelUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap beginTypeUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap finishTypeUnlock(const QString& rule, const QString& text);
    Q_INVOKABLE QVariantMap enterFriendCode(const QString& rule, const QString& code);
    Q_INVOKABLE QVariantMap check(const QString& subject);

    // The schedule painter works in the 168 hours of the week, Monday 00:00 first. These don't call the
    // service. weekHours gives each hour as 0 (off), 1 (part of it) or 2 (all of it), or an empty list
    // for a one-off session. paintHours turns the given hours on or off and returns the new `when`;
    // hours it doesn't touch keep their exact minutes. It returns null if no hour would be left on.
    Q_INVOKABLE QVariantList weekHours(const QVariant& when) const;
    Q_INVOKABLE QVariant paintHours(const QVariant& when, const QVariantList& hours, bool on) const;

    // A command from the palette, understood but not run: {kind, lists, targets, minutes, strength,
    // unlock, subject, screen}, or {error, known} (known is false when it isn't a command at all).
    Q_INVOKABLE QVariantMap parseCommand(const QString& text) const;

signals:
    void changed();

private:
    QVariantMap call(const std::string& op, const core::Json& args);
    // Fetches the rule as last listed, lets `edit` change it, and sends it back as rules.update.
    QVariantMap update_rule(const QString& rule, const std::function<void(core::Json&)>& edit);

    std::wstring pipe_;
    QTimer timer_;
    bool connected_ = false;
    QString problem_;
    QVariantMap status_;
    QVariantList rules_;
    bool animations_ = true;
};

}  // namespace sirenproof::ui
