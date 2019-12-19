// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/Wallet.hpp"
#include "opentxs/client/NymData.hpp"
#include "opentxs/contact/ContactGroup.hpp"
#include "opentxs/contact/ContactItem.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/ui/ProfileItem.hpp"
#include "opentxs/ui/ProfileSubsection.hpp"

#include "internal/ui/UI.hpp"
#include "Combined.hpp"

#include <map>
#include <memory>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

#include "ProfileSubsection.hpp"

template class opentxs::SharedPimpl<opentxs::ui::ProfileSubsection>;

//#define OT_METHOD "opentxs::ui::implementation::ProfileSubsection::"

namespace opentxs
{
ui::implementation::ProfileSectionRowInternal* Factory::ProfileSubsectionWidget(
    const ui::implementation::ProfileSectionInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::ProfileSectionRowID& rowID,
    const ui::implementation::ProfileSectionSortKey& key,
    const ui::implementation::CustomData& custom
#if OT_QT
    ,
    const bool qt
#endif
)
{
    return new ui::implementation::ProfileSubsection(
        parent,
        api,
        publisher,
        rowID,
        key,
        custom
#if OT_QT
        ,
        qt
#endif
    );
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
ProfileSubsection::ProfileSubsection(
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
    ) noexcept
    : Combined(
          api,
          publisher,
          parent.NymID(),
          parent.WidgetID(),
          parent,
          rowID,
          key
#if OT_QT
          ,
          qt
#endif
      )
{
    init();
    startup_.reset(new std::thread(&ProfileSubsection::startup, this, custom));

    OT_ASSERT(startup_)
}

bool ProfileSubsection::AddItem(
    const std::string& value,
    const bool primary,
    const bool active) const noexcept
{
    return parent_.AddClaim(row_id_.second, value, primary, active);
}

void* ProfileSubsection::construct_row(
    const ProfileSubsectionRowID& id,
    const ProfileSubsectionSortKey& index,
    const CustomData& custom) const noexcept
{
    OT_ASSERT(1 == custom.size())

    names_.emplace(id, index);
    const auto [it, added] = items_[index].emplace(
        id,
        Factory::ProfileItemWidget(*this, api_, publisher_, id, index, custom));

    return it->second.get();
}

bool ProfileSubsection::Delete(const std::string& claimID) const noexcept
{
    Lock lock(lock_);
    auto& claim = find_by_id(lock, Identifier::Factory(claimID));

    if (false == claim.Valid()) { return false; }

    return claim.Delete();
}

std::string ProfileSubsection::Name(const std::string& lang) const noexcept
{
    return proto::TranslateItemType(row_id_.second, lang);
}

std::set<ProfileSubsectionRowID> ProfileSubsection::process_group(
    const opentxs::ContactGroup& group) noexcept
{
    OT_ASSERT(row_id_.second == group.Type())

    std::set<ProfileSubsectionRowID> active{};

    for (const auto& [id, claim] : group) {
        OT_ASSERT(claim)

        CustomData custom{new opentxs::ContactItem(*claim)};
        add_item(id, sort_key(id), custom);
        active.emplace(id);
    }

    return active;
}

#if OT_QT
QVariant ProfileSubsection::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case ProfileQt::SubsectionNameColumn: {
                    return Name("en").c_str();
                }
                case ProfileQt::SubsectionTypeColumn: {
                    return "TODO SubsectionType";
                    // return QString::fromStdString(Type());
                }
                default: {
                    return {};
                }
            }
        }
        default: {
            return {};
        }
    };
}
#endif

void ProfileSubsection::reindex(
    const ProfileSectionSortKey&,
    const CustomData& custom) noexcept
{
    delete_inactive(
        process_group(extract_custom<opentxs::ContactGroup>(custom)));
}

bool ProfileSubsection::SetActive(const std::string& claimID, const bool active)
    const noexcept
{
    Lock lock(lock_);
    auto& claim = find_by_id(lock, Identifier::Factory(claimID));

    if (false == claim.Valid()) { return false; }

    return claim.SetActive(active);
}

bool ProfileSubsection::SetPrimary(
    const std::string& claimID,
    const bool primary) const noexcept
{
    Lock lock(lock_);
    auto& claim = find_by_id(lock, Identifier::Factory(claimID));

    if (false == claim.Valid()) { return false; }

    return claim.SetPrimary(primary);
}

bool ProfileSubsection::SetValue(
    const std::string& claimID,
    const std::string& value) const noexcept
{
    Lock lock(lock_);
    auto& claim = find_by_id(lock, Identifier::Factory(claimID));

    if (false == claim.Valid()) { return false; }

    return claim.SetValue(value);
}

int ProfileSubsection::sort_key(const ProfileSubsectionRowID) const noexcept
{
    return static_cast<int>(items_.size());
}

void ProfileSubsection::startup(const CustomData& custom) noexcept
{
    process_group(extract_custom<opentxs::ContactGroup>(custom));
    finish_startup();
}
}  // namespace opentxs::ui::implementation
