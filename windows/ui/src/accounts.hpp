#pragma once

#include <QObject>
#include <QVariantMap>

namespace sirenproof::ui {

// The "Lock down this PC" setup's view of Windows accounts. It only reads them and opens Windows' own
// settings; it never changes an account.
class Accounts : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;

    // {user, admin, others: [names]}, or {error}.
    Q_INVOKABLE QVariantMap read() const;
    // Settings > Accounts > Other users: where an account is added, or another account's type changed.
    Q_INVOKABLE bool openAccountSettings() const;
    // The User Accounts control panel, which has "Change your account type".
    Q_INVOKABLE bool openUserAccounts() const;
};

}  // namespace sirenproof::ui
