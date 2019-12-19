// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/Wallet.hpp"
#include "opentxs/api/Endpoints.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/client/NymData.hpp"
#include "opentxs/contact/Contact.hpp"
#include "opentxs/contact/ContactData.hpp"
#include "opentxs/contact/ContactSection.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/identity/Nym.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/Profile.hpp"
#include "opentxs/ui/ProfileSection.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "List.hpp"

#include <map>
#include <memory>
#include <set>
#include <thread>
#include <tuple>
#include <vector>

#include "Profile.hpp"

template struct std::pair<int, std::string>;

#define OT_METHOD "opentxs::ui::implementation::Profile::"

namespace opentxs
{
ui::implementation::Profile* Factory::ProfileModel(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID
#if OT_QT
    ,
    const bool qt
#endif
)
{
    return new ui::implementation::Profile(
        api,
        publisher,
        nymID
#if OT_QT
        ,
        qt
#endif
    );
}

#if OT_QT
ui::ProfileQt* Factory::ProfileQtModel(ui::implementation::Profile& parent)
{
    using ReturnType = ui::ProfileQt;

    return new ReturnType(parent);
}
#endif  // OT_QT
}  // namespace opentxs

#if OT_QT
namespace opentxs::ui
{
QT_PROXY_MODEL_WRAPPER(ProfileQt, implementation::Profile)

QString ProfileQt::displayName() const noexcept
{
    return parent_.DisplayName().c_str();
}
QString ProfileQt::nymID() const noexcept { return parent_.ID().c_str(); }
QString ProfileQt::paymentCode() const noexcept
{
    return parent_.PaymentCode().c_str();
}
}  // namespace opentxs::ui
#endif

namespace opentxs::ui::implementation
{
const std::set<proto::ContactSectionName> Profile::allowed_types_{
    proto::CONTACTSECTION_COMMUNICATION,
    proto::CONTACTSECTION_PROFILE};

const std::map<proto::ContactSectionName, int> Profile::sort_keys_{
    {proto::CONTACTSECTION_COMMUNICATION, 0},
    {proto::CONTACTSECTION_PROFILE, 1}};

Profile::Profile(
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID
#if OT_QT
    ,
    const bool qt
#endif
    ) noexcept
    : ProfileList(
          api,
          publisher,
          nymID
#if OT_QT
          ,
          qt,
          Roles{
              {ProfileQt::NymIdRole, "nymid"},
              {ProfileQt::ClaimIdRole, "claimid"},
              {ProfileQt::PaymentCodeRole, "paymentcode"},
              {ProfileQt::ClaimValueRole, "claimvalue"},
          },
          8
#endif
          )
    , listeners_({
          {api_.Endpoints().NymDownload(),
           new MessageProcessor<Profile>(&Profile::process_nym)},
      })
    , name_(nym_name(api_.Wallet(), nymID))
    , payment_code_()
{
    init();
    setup_listeners(listeners_);
    startup_.reset(new std::thread(&Profile::startup, this));

    OT_ASSERT(startup_)
}

bool Profile::AddClaim(
    const proto::ContactSectionName section,
    const proto::ContactItemType type,
    const std::string& value,
    const bool primary,
    const bool active) const noexcept
{
    auto reason = api_.Factory().PasswordPrompt("Adding a claim to nym");
    auto nym = api_.Wallet().mutable_Nym(primary_id_, reason);

    switch (section) {
        case proto::CONTACTSECTION_SCOPE: {

            return nym.SetScope(type, value, primary, reason);
        }
        case proto::CONTACTSECTION_COMMUNICATION: {
            switch (type) {
                case proto::CITEMTYPE_EMAIL: {

                    return nym.AddEmail(value, primary, active, reason);
                }
                case proto::CITEMTYPE_PHONE: {

                    return nym.AddPhoneNumber(value, primary, active, reason);
                }
                case proto::CITEMTYPE_OPENTXS: {

                    return nym.AddPreferredOTServer(value, primary, reason);
                }
                default: {
                }
            }
        } break;
        case proto::CONTACTSECTION_PROFILE: {

            return nym.AddSocialMediaProfile(
                value, type, primary, active, reason);
        }
        case proto::CONTACTSECTION_CONTRACT: {

            return nym.AddContract(value, type, primary, active, reason);
        }
        case proto::CONTACTSECTION_PROCEDURE:
#if OT_CRYPTO_SUPPORTED_SOURCE_BIP47
        {
            return nym.AddPaymentCode(value, type, primary, active, reason);
        }
#endif
        default: {
        }
    }

    Claim claim{};
    auto& [id, claimSection, claimType, claimValue, start, end, attributes] =
        claim;
    id = "";
    claimSection = section;
    claimType = type;
    claimValue = value;
    start = 0;
    end = 0;

    if (primary) { attributes.emplace(proto::CITEMATTR_PRIMARY); }

    if (primary || active) { attributes.emplace(proto::CITEMATTR_ACTIVE); }

    return nym.AddClaim(claim, reason);
}

Profile::ItemTypeList Profile::AllowedItems(
    const proto::ContactSectionName section,
    const std::string& lang) const noexcept
{
    return ui::ProfileSection::AllowedItems(section, lang);
}

Profile::SectionTypeList Profile::AllowedSections(const std::string& lang) const
    noexcept
{
    SectionTypeList output{};

    for (const auto& type : allowed_types_) {
        output.emplace_back(type, proto::TranslateSectionName(type, lang));
    }

    return output;
}

bool Profile::check_type(const proto::ContactSectionName type) noexcept
{
    return 1 == allowed_types_.count(type);
}

void* Profile::construct_row(
    const ProfileRowID& id,
    const ContactSortKey& index,
    const CustomData& custom) const noexcept
{
    names_.emplace(id, index);
    const auto [it, added] = items_[index].emplace(
        id,
        Factory::ProfileSectionWidget(
            *this,
            api_,
            publisher_,
            id,
            index,
            custom
#if OT_QT
            ,
            enable_qt_
#endif
            ));

    return it->second.get();
}

bool Profile::Delete(
    const int sectionType,
    const int type,
    const std::string& claimID) const noexcept
{
    Lock lock(lock_);
    auto& section = find_by_id(lock, static_cast<ProfileRowID>(sectionType));

    if (false == section.Valid()) { return false; }

    return section.Delete(type, claimID);
}

std::string Profile::DisplayName() const noexcept
{
    Lock lock(lock_);

    return name_;
}

std::string Profile::nym_name(
    const api::Wallet& wallet,
    const identifier::Nym& nymID) noexcept
{
    for (const auto& [id, name] : wallet.NymList()) {
        if (nymID.str() == id) { return name; }
    }

    return {};
}

#if OT_QT
QVariant Profile::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case ProfileQt::DisplayNameColumn: {
                    return DisplayName().c_str();
                }
                default: {
                    return {};
                }
            }
        }

        case ProfileQt::NymIdRole: {
            return ID().c_str();
        }
        case ProfileQt::PaymentCodeRole: {
            return PaymentCode().c_str();
        }
        default: {
            return {};
        }
    };
}
#endif

std::string Profile::PaymentCode() const noexcept
{
    Lock lock(lock_);

    return payment_code_;
}

void Profile::process_nym(const identity::Nym& nym) noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    Lock lock(lock_);
    name_ = nym.Alias();
    payment_code_ = nym.PaymentCode(reason);
    lock.unlock();
    UpdateNotify();
    std::set<ProfileRowID> active{};

    for (const auto& section : nym.Claims()) {
        auto& type = section.first;

        if (check_type(type)) {
            CustomData custom{new opentxs::ContactSection(*section.second)};
            add_item(type, sort_key(type), custom);
            active.emplace(type);
        }
    }

    delete_inactive(active);
}

void Profile::process_nym(const network::zeromq::Message& message) noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    wait_for_startup();

    OT_ASSERT(1 == message.Body().size());

    const std::string id(*message.Body().begin());
    const auto nymID = identifier::Nym::Factory(id);

    OT_ASSERT(false == nymID->empty())

    if (nymID != primary_id_) { return; }

    const auto nym = api_.Wallet().Nym(nymID, reason);

    OT_ASSERT(nym)

    process_nym(*nym);
}

bool Profile::SetActive(
    const int sectionType,
    const int type,
    const std::string& claimID,
    const bool active) const noexcept
{
    Lock lock(lock_);
    auto& section = find_by_id(lock, static_cast<ProfileRowID>(sectionType));

    if (false == section.Valid()) { return false; }

    return section.SetActive(type, claimID, active);
}

bool Profile::SetPrimary(
    const int sectionType,
    const int type,
    const std::string& claimID,
    const bool primary) const noexcept
{
    Lock lock(lock_);
    auto& section = find_by_id(lock, static_cast<ProfileRowID>(sectionType));

    if (false == section.Valid()) { return false; }

    return section.SetPrimary(type, claimID, primary);
}

bool Profile::SetValue(
    const int sectionType,
    const int type,
    const std::string& claimID,
    const std::string& value) const noexcept
{
    Lock lock(lock_);
    auto& section = find_by_id(lock, static_cast<ProfileRowID>(sectionType));

    if (false == section.Valid()) { return false; }

    return section.SetValue(type, claimID, value);
}

int Profile::sort_key(const proto::ContactSectionName type) noexcept
{
    return sort_keys_.at(type);
}

void Profile::startup() noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    LogVerbose(OT_METHOD)(__FUNCTION__)(": Loading nym ")(primary_id_).Flush();
    const auto nym = api_.Wallet().Nym(primary_id_, reason);

    OT_ASSERT(nym)

    process_nym(*nym);
    finish_startup();
}

Profile::~Profile()
{
    for (auto& it : listeners_) { delete it.second; }
}
}  // namespace opentxs::ui::implementation
