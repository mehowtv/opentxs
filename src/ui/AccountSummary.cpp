// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Issuer.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/network/ZMQ.hpp"
#include "opentxs/api/Core.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/AccountSummary.hpp"
#include "opentxs/ui/IssuerItem.hpp"
#include "opentxs/Types.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/core/identifier/Identifier.hpp"
#include "internal/ui/UI.hpp"
#include "List.hpp"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <thread>
#include <tuple>
#include <vector>

#include "AccountSummary.hpp"

#define OT_METHOD "opentxs::ui::implementation::AccountSummary::"

namespace opentxs
{
ui::implementation::AccountSummary* Factory::AccountSummaryModel(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID,
    const proto::ContactItemType currency
#if OT_QT
    ,
    const bool qt
#endif
)
{
    return new ui::implementation::AccountSummary(
        api,
        publisher,
        nymID,
        currency
#if OT_QT
        ,
        qt
#endif
    );
}

#if OT_QT
ui::AccountSummaryQt* Factory::AccountSummaryQtModel(
    ui::implementation::AccountSummary& parent)
{
    using ReturnType = ui::AccountSummaryQt;

    return new ReturnType(parent);
}
#endif  // OT_QT
}  // namespace opentxs

#if OT_QT
namespace opentxs::ui
{
QT_PROXY_MODEL_WRAPPER(AccountSummaryQt, implementation::AccountSummary)
}  // namespace opentxs::ui
#endif

namespace opentxs::ui::implementation
{
AccountSummary::AccountSummary(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID,
    const proto::ContactItemType currency
#if OT_QT
    ,
    const bool qt
#endif
    ) noexcept
    : AccountSummaryList(
          api,
          publisher,
          nymID
#if OT_QT
          ,
          qt,
          Roles{{AccountSummaryQt::AccountIDRole, "account"},
                {AccountSummaryQt::BalanceRole, "balance"}},
          5
#endif
          )
    , listeners_({
          {api_.Endpoints().IssuerUpdate(),
           new MessageProcessor<AccountSummary>(
               &AccountSummary::process_issuer)},
          {api_.Endpoints().ServerUpdate(),
           new MessageProcessor<AccountSummary>(
               &AccountSummary::process_server)},
          {api_.Endpoints().ConnectionStatus(),
           new MessageProcessor<AccountSummary>(
               &AccountSummary::process_connection)},
          {api_.Endpoints().NymDownload(),
           new MessageProcessor<AccountSummary>(&AccountSummary::process_nym)},
      })
    , currency_{currency}
    , issuers_{}
    , server_issuer_map_{}
    , nym_server_map_{}
{
    init();
    setup_listeners(listeners_);
    startup_.reset(new std::thread(&AccountSummary::startup, this));

    OT_ASSERT(startup_)
}

void* AccountSummary::construct_row(
    const AccountSummaryRowID& id,
    const AccountSummarySortKey& index,
    const CustomData& custom) const noexcept
{
    names_.emplace(id, index);
    const auto [it, added] = items_[index].emplace(
        id,
        Factory::IssuerItem(
            *this,
            api_,
            publisher_,
            id,
            index,
            custom,
            currency_
#if OT_QT
            ,
            enable_qt_
#endif
            ));

    return it->second.get();
}

AccountSummarySortKey AccountSummary::extract_key(
    const identifier::Nym& nymID,
    const identifier::Nym& issuerID) noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);
    AccountSummarySortKey output{false, "opentxs notary"};
    auto& [state, name] = output;

    const auto issuer = api_.Wallet().Issuer(nymID, issuerID);

    if (false == bool(issuer)) { return output; }

    const auto serverID = issuer->PrimaryServer(reason);

    if (serverID->empty()) { return output; }

    try {
        const auto server = api_.Wallet().Server(serverID, reason);
        name = server->Alias();
        const auto& serverNymID = server->Nym()->ID();
        eLock lock(shared_lock_);
        nym_server_map_.emplace(serverNymID, serverID);
        server_issuer_map_.emplace(serverID, issuerID);
    } catch (...) {
        return output;
    }

    switch (api_.ZMQ().Status(serverID->str())) {
        case ConnectionState::ACTIVE: {
            state = true;
        } break;
        case ConnectionState::NOT_ESTABLISHED:
        case ConnectionState::STALLED:
        default: {
        }
    }

    return output;
}

void AccountSummary::process_connection(
    const network::zeromq::Message& message) noexcept
{
    wait_for_startup();

    OT_ASSERT(2 == message.Body().size());

    const auto serverID = identifier::Server::Factory(message.Body().at(0));
    process_server(serverID);
}

void AccountSummary::process_issuer(const identifier::Nym& issuerID) noexcept
{
    issuers_.emplace(issuerID);
    const CustomData custom{};
    add_item(issuerID, extract_key(primary_id_, issuerID), custom);
}

void AccountSummary::process_issuer(
    const network::zeromq::Message& message) noexcept
{
    wait_for_startup();

    OT_ASSERT(2 == message.Body().size());

    const auto nymID = identifier::Nym::Factory(message.Body().at(0));
    const auto issuerID = identifier::Nym::Factory(message.Body().at(1));

    OT_ASSERT(false == nymID->empty())
    OT_ASSERT(false == issuerID->empty())

    if (nymID != primary_id_) { return; }

    process_issuer(issuerID);
}

void AccountSummary::process_nym(
    const network::zeromq::Message& message) noexcept
{
    wait_for_startup();

    OT_ASSERT(1 == message.Body().size());

    const auto nymID =
        identifier::Nym::Factory(std::string(*message.Body().begin()));

    sLock lock(shared_lock_);
    const auto it = nym_server_map_.find(nymID);

    if (nym_server_map_.end() == it) { return; }

    const auto serverID = it->second;
    lock.unlock();

    process_server(serverID);
}

void AccountSummary::process_server(
    const network::zeromq::Message& message) noexcept
{
    wait_for_startup();

    OT_ASSERT(1 == message.Body().size());

    const auto serverID =
        identifier::Server::Factory(std::string(*message.Body().begin()));

    OT_ASSERT(false == serverID->empty())

    process_server(serverID);
}

void AccountSummary::process_server(const identifier::Server& serverID) noexcept
{
    sLock lock(shared_lock_);
    const auto it = server_issuer_map_.find(serverID);

    if (server_issuer_map_.end() == it) { return; }

    const auto issuerID = it->second;
    lock.unlock();
    const CustomData custom{};
    add_item(issuerID, extract_key(primary_id_, issuerID), custom);
}

void AccountSummary::startup() noexcept
{
    const auto issuers = api_.Wallet().IssuerList(primary_id_);
    LogDetail(OT_METHOD)(__FUNCTION__)(": Loading ")(issuers.size())(
        " issuers.")
        .Flush();

    for (const auto& id : issuers) { process_issuer(id); }

    finish_startup();
}

AccountSummary::~AccountSummary()
{
    for (auto& it : listeners_) { delete it.second; }
}
}  // namespace opentxs::ui::implementation
