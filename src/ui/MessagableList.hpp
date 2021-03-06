// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#include "opentxs/ui/MessagableList.hpp"

#include "internal/ui/UI.hpp"
#include "List.hpp"

namespace opentxs::ui::implementation
{
using MessagableListList = List<
    MessagableExternalInterface,
    MessagableInternalInterface,
    MessagableListRowID,
    MessagableListRowInterface,
    MessagableListRowInternal,
    MessagableListRowBlank,
    MessagableListSortKey,
    MessagableListPrimaryID>;

class MessagableList final : public MessagableListList
{
public:
    const Identifier& ID() const noexcept final;

    ~MessagableList() final;

private:
    friend opentxs::Factory;

    const ListenerDefinitions listeners_;
    const OTIdentifier owner_contact_id_;

    void* construct_row(
        const MessagableListRowID& id,
        const MessagableListSortKey& index,
        const CustomData& custom) const noexcept final;
    bool last(const MessagableListRowID& id) const noexcept final
    {
        return MessagableListList::last(id);
    }

    void process_contact(
        const MessagableListRowID& id,
        const MessagableListSortKey& key) noexcept;
    void process_contact(const network::zeromq::Message& message) noexcept;
    void process_nym(const network::zeromq::Message& message) noexcept;
    void startup() noexcept;

    MessagableList(
        const api::client::internal::Manager& api,
        const network::zeromq::socket::Publish& publisher,
        const identifier::Nym& nymID
#if OT_QT
        ,
        const bool qt
#endif
        ) noexcept;
    MessagableList() = delete;
    MessagableList(const MessagableList&) = delete;
    MessagableList(MessagableList&&) = delete;
    MessagableList& operator=(const MessagableList&) = delete;
    MessagableList& operator=(MessagableList&&) = delete;
};
}  // namespace opentxs::ui::implementation
