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

public:
    explicit ServiceClient(std::wstring pipe, QObject* parent = nullptr);

    bool connected() const { return connected_; }
    QString problem() const { return problem_; }
    QVariantMap status() const { return status_; }
    QVariantList rules() const { return rules_; }
    // Windows' "Animation effects" setting; off means no ambient motion.
    bool animations() const { return animations_; }

    Q_INVOKABLE void refresh();

    Q_INVOKABLE QVariantMap startLock(const QVariantList& targets, int minutes, const QString& unlock, const QString& goal);
    Q_INVOKABLE QVariantMap addTarget(const QString& rule, const QString& target);
    Q_INVOKABLE QVariantMap removeTarget(const QString& rule, const QString& target);
    Q_INVOKABLE QVariantMap setStrength(const QString& rule, const QString& strength);
    Q_INVOKABLE QVariantMap removeRule(const QString& rule);
    Q_INVOKABLE QVariantMap cancelPending(const QString& id);
    Q_INVOKABLE QVariantMap startBreak();
    Q_INVOKABLE QVariantMap requestUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap cancelUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap beginTypeUnlock(const QString& rule);
    Q_INVOKABLE QVariantMap finishTypeUnlock(const QString& rule, const QString& text);
    Q_INVOKABLE QVariantMap enterFriendCode(const QString& rule, const QString& code);
    Q_INVOKABLE QVariantMap check(const QString& subject);

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
