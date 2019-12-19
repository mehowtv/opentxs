// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

namespace opentxs::ui::implementation
{
using ProfileList = List<
    ProfileExternalInterface,
    ProfileInternalInterface,
    ProfileRowID,
    ProfileRowInterface,
    ProfileRowInternal,
    ProfileRowBlank,
    ProfileSortKey,
    ProfilePrimaryID>;

class Profile final : public ProfileList
{
public:
    bool AddClaim(
        const proto::ContactSectionName section,
        const proto::ContactItemType type,
        const std::string& value,
        const bool primary,
        const bool active) const noexcept final;
    ItemTypeList AllowedItems(
        const proto::ContactSectionName section,
        const std::string& lang) const noexcept final;
    SectionTypeList AllowedSections(const std::string& lang) const
        noexcept final;
    bool Delete(const int section, const int type, const std::string& claimID)
        const noexcept final;
    std::string DisplayName() const noexcept final;
#if OT_QT
    int FindRow(const ProfileRowID& id, const ProfileSortKey& key) const
        noexcept final
    {
        return find_row(id, key);
    }
#endif
    const identifier::Nym& NymID() const noexcept final { return primary_id_; }
    std::string ID() const noexcept final { return primary_id_->str(); }
    std::string PaymentCode() const noexcept final;
    bool SetActive(
        const int section,
        const int type,
        const std::string& claimID,
        const bool active) const noexcept final;
    bool SetPrimary(
        const int section,
        const int type,
        const std::string& claimID,
        const bool primary) const noexcept final;
    bool SetValue(
        const int section,
        const int type,
        const std::string& claimID,
        const std::string& value) const noexcept final;

#if OT_QT
    QVariant qt_data(const int column, const int role) const noexcept;
#endif

    ~Profile();

private:
    friend opentxs::Factory;

    const ListenerDefinitions listeners_;
    std::string name_;
    std::string payment_code_;

    static const std::set<proto::ContactSectionName> allowed_types_;
    static const std::map<proto::ContactSectionName, int> sort_keys_;

    static int sort_key(const proto::ContactSectionName type) noexcept;
    static bool check_type(const proto::ContactSectionName type) noexcept;
    static std::string nym_name(
        const api::Wallet& wallet,
        const identifier::Nym& nymID) noexcept;

    void* construct_row(
        const ProfileRowID& id,
        const ProfileSortKey& index,
        const CustomData& custom) const noexcept final;

    bool last(const ProfileRowID& id) const noexcept final
    {
        return ProfileList::last(id);
    }

    void process_nym(const identity::Nym& nym) noexcept;
    void process_nym(const network::zeromq::Message& message) noexcept;
    void startup() noexcept;

    Profile(
        const api::client::internal::Manager& api,
        const network::zeromq::socket::Publish& publisher,
        const identifier::Nym& nymID
#if OT_QT
        ,
        const bool qt
#endif
        ) noexcept;
    Profile() = delete;
    Profile(const Profile&) = delete;
    Profile(Profile&&) = delete;
    Profile& operator=(const Profile&) = delete;
    Profile& operator=(Profile&&) = delete;
};
}  // namespace opentxs::ui::implementation
