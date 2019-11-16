// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/contact/Contact.hpp"
#include "opentxs/contact/ContactData.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/ui/PayableList.hpp"
#include "opentxs/ui/PayableListItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include "PayableListItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::PayableListItem>;

namespace opentxs
{
ui::internal::PayableListItem* Factory::PayableListItem(
    const ui::implementation::PayableInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const ui::implementation::PayableListRowID& rowID,
    const ui::implementation::PayableListSortKey& key,
    const std::string& paymentcode,
    const proto::ContactItemType& currency)
{
    return new ui::implementation::PayableListItem(
        parent, api, publisher, rowID, key, paymentcode, currency);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
PayableListItem::PayableListItem(
    const PayableInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const PayableListRowID& rowID,
    const PayableListSortKey& key,
    const std::string& paymentcode,
    const proto::ContactItemType& currency) noexcept
    : ot_super(parent, api, publisher, rowID, key)
    , payment_code_(paymentcode)
    , currency_(currency)
{
}

std::string PayableListItem::PaymentCode() const noexcept
{
    Lock lock(lock_);

    return payment_code_;
}

//    enum Roles {
//        ContactIDRole = Qt::UserRole + 0,
//        SectionRole = Qt::UserRole + 1,
//    };
//    enum Columns {
//        DisplayNameColumn = 0,
//        PaycodeColumn = 1
//    };

#if OT_QT
QVariant PayableListItem::qt_data(const int column, int role) const noexcept
{
    if (0 == column) { return ContactListItem::qt_data(column, role); }

    switch (role) {
        case Qt::DisplayRole: {

            return PaymentCode().c_str();
        }
        default: {
            return {};
        }
    }
}
#endif

void PayableListItem::reindex(
    const ContactListSortKey& key,
    const CustomData& custom) noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);

    ot_super::reindex(key, custom);
    const auto contact = api_.Contacts().Contact(row_id_, reason);

    OT_ASSERT(contact);

    payment_code_ = contact->PaymentCode(currency_);
}
}  // namespace opentxs::ui::implementation
