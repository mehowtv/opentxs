// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_UI_PROFILE_HPP
#define OPENTXS_UI_PROFILE_HPP

#ifndef Q_MOC_RUN
#include "opentxs/Forward.hpp"

#include "opentxs/ui/List.hpp"
#include "opentxs/Proto.hpp"

#include <string>
#include <tuple>
#include <vector>

#ifdef SWIG
#include <algorithm>
#include <tuple>
#include <vector>

// clang-format off
%extend opentxs::ui::Profile {
    bool AddClaim(
        const int section,
        const int type,
        const std::string& value,
        const bool primary,
        const bool active) const noexcept
    {
        return $self->AddClaim(
            static_cast<opentxs::proto::ContactSectionName>(section),
            static_cast<opentxs::proto::ContactItemType>(type),
            value,
            primary,
            active);
    }
    std::vector<std::pair<int, std::string>> AllowedItems(
        const int section,
        const std::string& lang) noexcept
    {
        const auto types = opentxs::ui::ProfileSection::AllowedItems(
            static_cast<opentxs::proto::ContactSectionName>(section),
            lang);
        std::vector<std::pair<int, std::string>> output;
        std::transform(
            types.begin(),
            types.end(),
            std::inserter(output, output.end()),
            [](std::pair<opentxs::proto::ContactItemType, std::string> type) ->
                std::pair<int, std::string> {
                    return {static_cast<int>(type.first), type.second};} );

        return output;
    }
    std::vector<std::pair<int, std::string>> AllowedSections(
        const std::string& lang) noexcept
    {
        const auto sections = $self->AllowedSections(lang);
        std::vector<std::pair<int, std::string>> output;
        std::transform(
            sections.begin(),
            sections.end(),
            std::inserter(output, output.end()),
            [](std::pair<opentxs::proto::ContactSectionName, std::string> type) ->
                std::pair<int, std::string> {
                    return {static_cast<int>(type.first), type.second};} );

        return output;
    }
}
%ignore opentxs::ui::Profile::AddClaim;
%ignore opentxs::ui::Profile::AllowedItems;
%ignore opentxs::ui::Profile::AllowedSections;
%rename(UIProfile) opentxs::ui::Profile;
// clang-format on
#endif  // SWIG

namespace opentxs
{
namespace ui
{
namespace implementation
{
class Profile;
}  // namespace implementation

class Profile : virtual public List
{
public:
    using ItemType = std::pair<proto::ContactItemType, std::string>;
    using ItemTypeList = std::vector<ItemType>;
    using SectionType = std::pair<proto::ContactSectionName, std::string>;
    using SectionTypeList = std::vector<SectionType>;

    OPENTXS_EXPORT virtual bool AddClaim(
        const proto::ContactSectionName section,
        const proto::ContactItemType type,
        const std::string& value,
        const bool primary,
        const bool active) const noexcept = 0;
    OPENTXS_EXPORT virtual ItemTypeList AllowedItems(
        const proto::ContactSectionName section,
        const std::string& lang) const noexcept = 0;
    OPENTXS_EXPORT virtual SectionTypeList AllowedSections(
        const std::string& lang) const noexcept = 0;
    OPENTXS_EXPORT virtual bool Delete(
        const int section,
        const int type,
        const std::string& claimID) const noexcept = 0;
    OPENTXS_EXPORT virtual std::string DisplayName() const noexcept = 0;
    OPENTXS_EXPORT virtual opentxs::SharedPimpl<opentxs::ui::ProfileSection>
    First() const noexcept = 0;
    OPENTXS_EXPORT virtual std::string ID() const noexcept = 0;
    OPENTXS_EXPORT virtual opentxs::SharedPimpl<opentxs::ui::ProfileSection>
    Next() const noexcept = 0;
    OPENTXS_EXPORT virtual std::string PaymentCode() const noexcept = 0;
    OPENTXS_EXPORT virtual bool SetActive(
        const int section,
        const int type,
        const std::string& claimID,
        const bool active) const noexcept = 0;
    OPENTXS_EXPORT virtual bool SetPrimary(
        const int section,
        const int type,
        const std::string& claimID,
        const bool primary) const noexcept = 0;
    OPENTXS_EXPORT virtual bool SetValue(
        const int section,
        const int type,
        const std::string& claimID,
        const std::string& value) const noexcept = 0;

    OPENTXS_EXPORT ~Profile() override = default;

protected:
    Profile() noexcept = default;

private:
    Profile(const Profile&) = delete;
    Profile(Profile&&) = delete;
    Profile& operator=(const Profile&) = delete;
    Profile& operator=(Profile&&) = delete;
};
}  // namespace ui
}  // namespace opentxs
#endif

#if OT_QT || defined(Q_MOC_RUN)
class opentxs::ui::ProfileQt final : public QIdentityProxyModel
{
    Q_OBJECT
    Q_PROPERTY(QString displayName READ displayName NOTIFY updated)
    Q_PROPERTY(QString nymID READ nymID NOTIFY updated)
    Q_PROPERTY(QString paymentCode READ paymentCode NOTIFY updated)

signals:
    void updated() const;

public:
    // Table layout
    enum Roles {
        NymIdRole = Qt::UserRole + 0,
        PaymentCodeRole = Qt::UserRole + 1,
        ClaimIdRole = Qt::UserRole + 2,
        ClaimValueRole = Qt::UserRole + 3,
    };
    enum Columns {
        DisplayNameColumn = 0,
        SectionNameColumn = 1,
        SectionTypeColumn = 2,
        SubsectionNameColumn = 3,
        SubsectionTypeColumn = 4,
        ClaimIsActiveColumn = 5,
        ClaimIsPrimaryColumn = 6,
    };

    // Tree layout
    OPENTXS_EXPORT QString displayName() const noexcept;
    OPENTXS_EXPORT QString nymID() const noexcept;
    OPENTXS_EXPORT QString paymentCode() const noexcept;

    ~ProfileQt() final = default;

private:
    friend opentxs::Factory;

    implementation::Profile& parent_;

    void notify() const noexcept;

    ProfileQt(implementation::Profile& parent) noexcept;
    ProfileQt(const ProfileQt&) = delete;
    ProfileQt(ProfileQt&&) = delete;
    ProfileQt& operator=(const ProfileQt&) = delete;
    ProfileQt& operator=(ProfileQt&&) = delete;
};
#endif
#endif
