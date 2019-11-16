// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Activity.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/ui/ActivityThread.hpp"
#include "opentxs/ui/ActivityThreadItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include "ActivityThreadItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::ActivityThreadItem>;

namespace opentxs::ui::implementation
{
ActivityThreadItem::ActivityThreadItem(
    const ActivityThreadInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID,
    const ActivityThreadRowID& rowID,
    const ActivityThreadSortKey& sortKey,
    const CustomData& custom,
    const bool loading,
    const bool pending) noexcept
    : ActivityThreadItemRow(parent, api, publisher, rowID, true)
    , nym_id_(nymID)
    , time_(std::get<0>(sortKey))
    , item_id_(std::get<0>(row_id_))
    , box_(std::get<1>(row_id_))
    , account_id_(std::get<2>(row_id_))
    , text_(extract_custom<std::string>(custom))
    , loading_(Flag::Factory(loading))
    , pending_(Flag::Factory(pending))
{
}

bool ActivityThreadItem::MarkRead() const noexcept
{
    return api_.Activity().MarkRead(
        nym_id_, Identifier::Factory(parent_.ThreadID()), item_id_);
}

#if OT_QT
QVariant ActivityThreadItem::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case ActivityThreadQt::TextColumn: {
                    return Text().c_str();
                }
                case ActivityThreadQt::DisplayAmountColumn: {
                    return DisplayAmount().c_str();
                }
                case ActivityThreadQt::MemoColumn: {
                    return Memo().c_str();
                }
                case ActivityThreadQt::TimeColumn: {
                    QDateTime qdatetime;
                    qdatetime.setSecsSinceEpoch(
                        std::chrono::system_clock::to_time_t(Timestamp()));
                    return qdatetime;
                }
                default: {
                    return {};
                }
            }
        }
        case ActivityThreadQt::PolarityRole: {
            return polarity(Amount());
        }
        case ActivityThreadQt::TypeRole: {
            return static_cast<int>(Type());
        }
        case ActivityThreadQt::AmountRole: {
            return static_cast<unsigned long long>(Amount());
        }
        case Qt::CheckStateRole: {
            switch (column) {
                case ActivityThreadQt::LoadingColumn: {
                    return Loading();
                }
                case ActivityThreadQt::PendingColumn: {
                    return Pending();
                }
                default: {
                }
            }

            [[fallthrough]];
        }
        default: {
            return {};
        }
    }
}
#endif

void ActivityThreadItem::reindex(
    const ActivityThreadSortKey&,
    const CustomData& custom) noexcept
{
    const auto text = extract_custom<std::string>(custom);

    if (false == text.empty()) {
        eLock lock(shared_lock_);
        text_ = text;
    }
}

std::string ActivityThreadItem::Text() const noexcept
{
    sLock lock(shared_lock_);

    return text_;
}

std::chrono::system_clock::time_point ActivityThreadItem::Timestamp() const
    noexcept
{
    sLock lock(shared_lock_);

    return time_;
}
}  // namespace opentxs::ui::implementation
