// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/client/NymData.hpp"
#include "opentxs/contact/ContactItem.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/ui/ProfileItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include "ProfileItem.hpp"

namespace opentxs
{
ui::implementation::ProfileSubsectionRowInternal* Factory::ProfileItemWidget(
    const ui::implementation::ProfileSubsectionInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::ProfileSubsectionRowID& rowID,
    const ui::implementation::ProfileSubsectionSortKey& sortKey,
    const ui::implementation::CustomData& custom)
{
    return new ui::implementation::ProfileItem(
        parent, api, publisher, rowID, sortKey, custom);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
ProfileItem::ProfileItem(
    const ProfileSubsectionInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ProfileSubsectionRowID& rowID,
    const ProfileSubsectionSortKey& sortKey,
    const CustomData& custom) noexcept
    : ProfileItemRow(parent, api, publisher, rowID, true)
    , item_{new opentxs::ContactItem(
          extract_custom<opentxs::ContactItem>(custom))}
{
}

bool ProfileItem::add_claim(const Claim& claim) const noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    auto nym = api_.Wallet().mutable_Nym(parent_.NymID(), reason);

    return nym.AddClaim(claim, reason);
}

Claim ProfileItem::as_claim() const noexcept
{
    sLock lock(shared_lock_);
    Claim output{};
    auto& [id, section, type, value, start, end, attributes] = output;
    id = "";
    section = parent_.Section();
    type = parent_.Type();
    value = item_->Value();
    start = item_->Start();
    end = item_->End();

    if (item_->isPrimary()) { attributes.emplace(proto::CITEMATTR_PRIMARY); }

    if (item_->isPrimary() || item_->isActive()) {
        attributes.emplace(proto::CITEMATTR_ACTIVE);
    }

    return output;
}

#if OT_QT
QVariant ProfileItem::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::CheckStateRole: {
            switch (column) {
                case ProfileQt::ClaimIsActiveColumn: {
                    return IsActive() ? Qt::Checked : Qt::Unchecked;
                }
                case ProfileQt::ClaimIsPrimaryColumn: {
                    return IsPrimary() ? Qt::Checked : Qt::Unchecked;
                }
                default: {
                    return {};
                }
            }
        }
        case ProfileQt::ClaimIdRole: {
            return ClaimID().c_str();
        }
        case ProfileQt::ClaimValueRole: {
            return Value().c_str();
        }
        default: {
            return {};
        }
    };
}
#endif

bool ProfileItem::Delete() const noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    auto nym = api_.Wallet().mutable_Nym(parent_.NymID(), reason);

    return nym.DeleteClaim(row_id_, reason);
}

void ProfileItem::reindex(
    const ProfileSubsectionSortKey&,
    const CustomData& custom) noexcept
{
    eLock lock(shared_lock_);
    item_.reset(
        new opentxs::ContactItem(extract_custom<opentxs::ContactItem>(custom)));

    OT_ASSERT(item_);
}

bool ProfileItem::SetActive(const bool& active) const noexcept
{
    Claim claim{};
    auto& attributes = std::get<6>(claim);

    if (active) {
        attributes.emplace(proto::CITEMATTR_ACTIVE);
    } else {
        attributes.erase(proto::CITEMATTR_ACTIVE);
        attributes.erase(proto::CITEMATTR_PRIMARY);
    }

    return add_claim(claim);
}

bool ProfileItem::SetPrimary(const bool& primary) const noexcept
{
    Claim claim{};
    auto& attributes = std::get<6>(claim);

    if (primary) {
        attributes.emplace(proto::CITEMATTR_PRIMARY);
        attributes.emplace(proto::CITEMATTR_ACTIVE);
    } else {
        attributes.erase(proto::CITEMATTR_PRIMARY);
    }

    return add_claim(claim);
}

bool ProfileItem::SetValue(const std::string& newValue) const noexcept
{
    Claim claim{};
    std::get<3>(claim) = newValue;

    return add_claim(claim);
}
}  // namespace opentxs::ui::implementation
