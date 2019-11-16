// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Issuer.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/contract/UnitDefinition.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/AccountSummaryItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include <atomic>

#include "AccountSummaryItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::AccountSummaryItem>;

namespace opentxs
{
auto Factory::AccountSummaryItem(
    const opentxs::PasswordPrompt& reason,
    const ui::implementation::IssuerItemInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::IssuerItemRowID& rowID,
    const ui::implementation::IssuerItemSortKey& sortKey,
    const ui::implementation::CustomData& custom)
    -> ui::implementation::IssuerItemRowInternal*
{
    return new ui::implementation::AccountSummaryItem(
        reason, parent, api, publisher, rowID, sortKey, custom);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
AccountSummaryItem::AccountSummaryItem(
    const opentxs::PasswordPrompt& reason,
    const IssuerItemInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const IssuerItemRowID& rowID,
    const IssuerItemSortKey& sortKey,
    const CustomData& custom) noexcept
    : AccountSummaryItemRow(parent, api, publisher, rowID, true)
    , account_id_(std::get<0>(row_id_).get())
    , currency_(std::get<1>(row_id_))
    , balance_(extract_custom<Amount>(custom))
    , name_(sortKey)
    , contract_(load_unit(api_, account_id_, reason))
{
}

auto AccountSummaryItem::DisplayBalance() const noexcept -> std::string
{
    auto reason = api_.Factory().PasswordPrompt("Loading account balance");

    if (0 == contract_->Version()) {
        eLock lock(shared_lock_);

        try {
            contract_ = api_.Wallet().UnitDefinition(
                api_.Storage().AccountContract(account_id_), reason);
        } catch (...) {
        }
    }

    sLock lock(shared_lock_);

    if (0 < contract_->Version()) {
        const auto amount = balance_.load();
        std::string output{};
        const auto formatted =
            contract_->FormatAmountLocale(amount, output, ",", ".");

        if (formatted) { return output; }

        return std::to_string(amount);
    }

    return {};
}

auto AccountSummaryItem::load_unit(
    const api::Core& api,
    const Identifier& id,
    const PasswordPrompt& reason) -> OTUnitDefinition
{
    try {
        return api.Wallet().UnitDefinition(
            api.Storage().AccountContract(id), reason);
    } catch (...) {

        return api.Factory().UnitDefinition();
    }
}

auto AccountSummaryItem::Name() const noexcept -> std::string
{
    sLock lock(shared_lock_);

    return name_;
}

#if OT_QT
auto AccountSummaryItem::qt_data(const int column, int role) const noexcept
    -> QVariant
{
    switch (role) {
        case AccountSummaryQt::AccountIDRole: {
            return AccountID().c_str();
        }
        case AccountSummaryQt::BalanceRole: {
            return static_cast<unsigned long long>(Balance());
        }
        case Qt::DisplayRole: {
            switch (column) {
                case AccountSummaryQt::AccountNameColumn: {
                    return Name().c_str();
                }
                case AccountSummaryQt::BalanceColumn: {
                    return DisplayBalance().c_str();
                }
                default: {
                    [[fallthrough]];
                }
            }
        }
        default: {
        }
    }
    return {};
}
#endif

auto AccountSummaryItem::reindex(
    const IssuerItemSortKey& key,
    const CustomData& custom) noexcept -> void
{
    balance_.store(extract_custom<Amount>(custom));
    eLock lock(shared_lock_);
    name_ = key;
}
}  // namespace opentxs::ui::implementation
