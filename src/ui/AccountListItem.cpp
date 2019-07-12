// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Activity.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/contract/UnitDefinition.hpp"
#include "opentxs/core/identifier/Server.hpp"
#include "opentxs/core/identifier/UnitDefinition.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/core/UniqueQueue.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/AccountListItem.hpp"

#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include <atomic>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

#include "AccountListItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::AccountListItem>;

// #define OT_METHOD "opentxs::ui::implementation::AccountListItem::"

namespace opentxs
{
ui::implementation::AccountListRowInternal* Factory::AccountListItem(
    const opentxs::PasswordPrompt& reason,
    const ui::implementation::AccountListInternalInterface& parent,
    const api::client::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::AccountListRowID& rowID,
    const ui::implementation::AccountListSortKey& sortKey,
    const ui::implementation::CustomData& custom)
{
    return new ui::implementation::AccountListItem(
        reason, parent, api, publisher, rowID, sortKey, custom);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
AccountListItem::AccountListItem(
    const opentxs::PasswordPrompt& reason,
    const AccountListInternalInterface& parent,
    const api::client::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const AccountListRowID& rowID,
    const AccountListSortKey& sortKey,
    const CustomData& custom)
    : AccountListItemRow(parent, api, publisher, rowID, true)
    , type_(AccountType::Custodial)
    , unit_(sortKey.first)
    , balance_(extract_custom<Amount>(custom, 0))
    , contract_(api_.Wallet().UnitDefinition(

          extract_custom<OTUnitID>(custom, 1),
          reason))
    , notary_(api_.Wallet().Server(
          identifier::Server::Factory(sortKey.second),
          reason))
    , name_(extract_custom<std::string>(custom, 2))
{
    OT_ASSERT(contract_);
    OT_ASSERT(notary_);
}

std::string AccountListItem::DisplayBalance() const
{
    std::string output{};
    const auto formatted =
        contract_->FormatAmountLocale(balance_, output, ",", ".");

    if (formatted) { return output; }

    return std::to_string(balance_);
}
}  // namespace opentxs::ui::implementation
