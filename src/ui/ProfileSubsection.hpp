// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

namespace opentxs::ui::implementation
{
using ProfileSubsectionList = List<
    ProfileSubsectionExternalInterface,
    ProfileSubsectionInternalInterface,
    ProfileSubsectionRowID,
    ProfileSubsectionRowInterface,
    ProfileSubsectionRowInternal,
    ProfileSubsectionRowBlank,
    ProfileSubsectionSortKey,
    ProfileSubsectionPrimaryID>;
using ProfileSubsectionRow = RowType<
    ProfileSectionRowInternal,
    ProfileSectionInternalInterface,
    ProfileSectionRowID>;

class ProfileSubsection final : public Combined<
                                    ProfileSubsectionList,
                                    ProfileSubsectionRow,
                                    ProfileSectionSortKey>
{
public:
    bool AddItem(
        const std::string& value,
        const bool primary,
        const bool active) const noexcept final;
    bool Delete(const std::string& claimID) const noexcept final;
    std::string Name(const std::string& lang) const noexcept final;
    const identifier::Nym& NymID() const noexcept final { return primary_id_; }
    proto::ContactSectionName Section() const noexcept final
    {
        return row_id_.first;
    }
    bool SetActive(const std::string& claimID, const bool active) const
        noexcept final;
    bool SetPrimary(const std::string& claimID, const bool primary) const
        noexcept final;
    bool SetValue(const std::string& claimID, const std::string& value) const
        noexcept final;
    proto::ContactItemType Type() const noexcept final
    {
        return row_id_.second;
    }

    void reindex(
        const ProfileSectionSortKey& key,
        const CustomData& custom) noexcept final;

#if OT_QT
    QVariant qt_data(const int column, const int role) const noexcept final;
#endif

    ~ProfileSubsection() = default;

private:
    friend opentxs::Factory;

    static bool check_type(const ProfileSubsectionRowID type);

    void* construct_row(
        const ProfileSubsectionRowID& id,
        const ProfileSubsectionSortKey& index,
        const CustomData& custom) const noexcept final;

    bool last(const ProfileSubsectionRowID& id) const noexcept final
    {
        return ProfileSubsectionList::last(id);
    }
    std::set<ProfileSubsectionRowID> process_group(
        const opentxs::ContactGroup& group) noexcept;
    int sort_key(const ProfileSubsectionRowID type) const noexcept;
    void startup(const CustomData& custom) noexcept;

    ProfileSubsection(
        const ProfileSectionInternalInterface& parent,
        const api::client::internal::Manager& api,
        const network::zeromq::socket::Publish& publisher,
        const ProfileSectionRowID& rowID,
        const ProfileSectionSortKey& key,
        const CustomData& custom
#if OT_QT
        ,
        const bool qt
#endif
        ) noexcept;
    ProfileSubsection() = delete;
    ProfileSubsection(const ProfileSubsection&) = delete;
    ProfileSubsection(ProfileSubsection&&) = delete;
    ProfileSubsection& operator=(const ProfileSubsection&) = delete;
    ProfileSubsection& operator=(ProfileSubsection&&) = delete;
};
}  // namespace opentxs::ui::implementation
