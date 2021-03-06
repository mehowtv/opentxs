// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_CONTACT_CONTACTSECTION_HPP
#define OPENTXS_CONTACT_CONTACTSECTION_HPP

#include "opentxs/Forward.hpp"

#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"

#include <map>
#include <memory>

namespace opentxs
{
namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

class ContactSection
{
public:
    typedef std::map<proto::ContactItemType, std::shared_ptr<ContactGroup>>
        GroupMap;

    OPENTXS_EXPORT ContactSection(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const proto::ContactSectionName section,
        const GroupMap& groups);
    OPENTXS_EXPORT ContactSection(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber version,
        const VersionNumber parentVersion,
        const proto::ContactSectionName section,
        const std::shared_ptr<ContactItem>& item);
    OPENTXS_EXPORT ContactSection(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber parentVersion,
        const proto::ContactSection& serialized);
    OPENTXS_EXPORT ContactSection(const ContactSection&);

    OPENTXS_EXPORT ContactSection operator+(const ContactSection& rhs) const;

    OPENTXS_EXPORT ContactSection
    AddItem(const std::shared_ptr<ContactItem>& item) const;
    OPENTXS_EXPORT GroupMap::const_iterator begin() const;
    OPENTXS_EXPORT std::shared_ptr<ContactItem> Claim(
        const Identifier& item) const;
    OPENTXS_EXPORT ContactSection Delete(const Identifier& id) const;
    OPENTXS_EXPORT GroupMap::const_iterator end() const;
    OPENTXS_EXPORT std::shared_ptr<ContactGroup> Group(
        const proto::ContactItemType& type) const;
    OPENTXS_EXPORT bool HaveClaim(const Identifier& item) const;
    OPENTXS_EXPORT bool SerializeTo(
        proto::ContactData& data,
        const bool withIDs = false) const;
    OPENTXS_EXPORT std::size_t Size() const;
    OPENTXS_EXPORT const proto::ContactSectionName& Type() const;
    OPENTXS_EXPORT VersionNumber Version() const;

    OPENTXS_EXPORT ~ContactSection() = default;

private:
    const api::internal::Core& api_;
    const VersionNumber version_{0};
    const std::string nym_{};
    const proto::ContactSectionName section_{proto::CONTACTSECTION_ERROR};
    const GroupMap groups_{};

    static VersionNumber check_version(
        const VersionNumber in,
        const VersionNumber targetVersion);
    static GroupMap create_group(
        const std::string& nym,
        const proto::ContactSectionName section,
        const std::shared_ptr<ContactItem>& item);
    static GroupMap extract_groups(
        const api::internal::Core& api,
        const std::string& nym,
        const VersionNumber parentVersion,
        const proto::ContactSection& serialized);

    ContactSection add_scope(const std::shared_ptr<ContactItem>& item) const;

    ContactSection() = delete;
    ContactSection(ContactSection&&) = delete;
    ContactSection& operator=(const ContactSection&) = delete;
    ContactSection& operator=(ContactSection&&) = delete;
};
}  // namespace opentxs

#endif
