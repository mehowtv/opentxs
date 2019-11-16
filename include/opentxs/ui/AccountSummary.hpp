// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_UI_ACCOUNTSUMMARY_HPP
#define OPENTXS_UI_ACCOUNTSUMMARY_HPP

#ifndef Q_MOC_RUN
#include "opentxs/Forward.hpp"

#include "opentxs/ui/List.hpp"

#ifdef SWIG
// clang-format off
%rename(UIAccountSummary) opentxs::ui::AccountSummary;
// clang-format on
#endif  // SWIG

namespace opentxs
{
namespace ui
{
namespace implementation
{
class AccountSummary;
}  // namespace implementation

class AccountSummary : virtual public List
{
public:
    OPENTXS_EXPORT virtual opentxs::SharedPimpl<opentxs::ui::IssuerItem> First()
        const noexcept = 0;
    OPENTXS_EXPORT virtual opentxs::SharedPimpl<opentxs::ui::IssuerItem> Next()
        const noexcept = 0;

    OPENTXS_EXPORT ~AccountSummary() override = default;

protected:
    AccountSummary() noexcept = default;

private:
    AccountSummary(const AccountSummary&) = delete;
    AccountSummary(AccountSummary&&) = delete;
    AccountSummary& operator=(const AccountSummary&) = delete;
    AccountSummary& operator=(AccountSummary&&) = delete;
};
}  // namespace ui
}  // namespace opentxs
#endif

#if OT_QT || defined(Q_MOC_RUN)
class opentxs::ui::AccountSummaryQt final : public QIdentityProxyModel
{
    Q_OBJECT

signals:
    void updated() const;

public:
    // Tree layout
    enum Roles {
        AccountIDRole = Qt::UserRole + 0,
        BalanceRole = Qt::UserRole + 1,
    };
    enum Columns {
        IssuerNameColumn = 0,
        ConnectionStateColumn = 1,
        TrustedColumn = 2,
        AccountNameColumn = 3,
        BalanceColumn = 4,
    };

    ~AccountSummaryQt() final = default;

private:
    friend opentxs::Factory;

    implementation::AccountSummary& parent_;

    void notify() const noexcept;

    AccountSummaryQt(implementation::AccountSummary& parent) noexcept;
    AccountSummaryQt(const AccountSummaryQt&) = delete;
    AccountSummaryQt(AccountSummaryQt&&) = delete;
    AccountSummaryQt& operator=(const AccountSummaryQt&) = delete;
    AccountSummaryQt& operator=(AccountSummaryQt&&) = delete;
};
#endif
#endif
