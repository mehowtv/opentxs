// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/api/client/Activity.hpp"
#include "opentxs/api/client/Contacts.hpp"
#include "opentxs/api/client/Manager.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/core/identifier/Nym.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/core/Lockable.hpp"
#include "opentxs/core/PasswordPrompt.hpp"
#include "opentxs/core/UniqueQueue.hpp"
#include "opentxs/network/zeromq/socket/Subscribe.hpp"
#include "opentxs/network/zeromq/Context.hpp"
#include "opentxs/network/zeromq/ListenCallback.hpp"
#include "opentxs/network/zeromq/FrameIterator.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/ui/ActivitySummaryItem.hpp"

#include "internal/api/client/Client.hpp"
#include "internal/ui/UI.hpp"
#include "Row.hpp"

#include <atomic>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>

#include "ActivitySummaryItem.hpp"

template class opentxs::SharedPimpl<opentxs::ui::ActivitySummaryItem>;

#define GET_TEXT_MILLISECONDS 10

#define OT_METHOD "opentxs::ui::implementation::ActivitySummaryItem::"

namespace opentxs
{
ui::implementation::ActivitySummaryRowInternal* Factory::ActivitySummaryItem(
    const ui::implementation::ActivitySummaryInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID,
    const ui::implementation::ActivitySummaryRowID& rowID,
    const ui::implementation::ActivitySummarySortKey& sortKey,
    const ui::implementation::CustomData& custom,
    const Flag& running)
{
    return new ui::implementation::ActivitySummaryItem(
        parent, api, publisher, nymID, rowID, sortKey, custom, running);
}
}  // namespace opentxs

namespace opentxs::ui::implementation
{
ActivitySummaryItem::ActivitySummaryItem(
    const ActivitySummaryInternalInterface& parent,
    const api::client::internal::Manager& api,
    const network::zeromq::socket::Publish& publisher,
    const identifier::Nym& nymID,
    const ActivitySummaryRowID& rowID,
    const ActivitySummarySortKey& sortKey,
    const CustomData& custom,
    const Flag& running) noexcept
    : ActivitySummaryItemRow(parent, api, publisher, rowID, true)
    , running_(running)
    , nym_id_(nymID)
    , key_{sortKey}
    , thread_()
    , display_name_{std::get<1>(key_)}
    , text_("")
    , type_(extract_custom<StorageBox>(custom, 1))
    , time_(extract_custom<Time>(custom, 3))
    , newest_item_thread_(nullptr)
    , newest_item_()
    , next_task_id_(0)
    , break_(false)
{
    startup(custom, newest_item_);
    newest_item_thread_.reset(
        new std::thread(&ActivitySummaryItem::get_text, this));

    OT_ASSERT(newest_item_thread_)
}

std::string ActivitySummaryItem::DisplayName() const noexcept
{
    sLock lock(shared_lock_);

    if (display_name_.empty()) { return api_.Contacts().ContactName(row_id_); }

    return display_name_;
}

std::string ActivitySummaryItem::find_text(
    const PasswordPrompt& reason,
    const ItemLocator& locator) const noexcept
{
    const auto& [itemID, box, accountID] = locator;

    switch (box) {
        case StorageBox::MAILINBOX:
        case StorageBox::MAILOUTBOX: {
            auto text = api_.Activity().MailText(
                nym_id_, Identifier::Factory(itemID), box, reason);

            if (text) {

                return *text;
            } else {
                LogOutput(OT_METHOD)(__FUNCTION__)(
                    ": Mail item does not exist.")
                    .Flush();
            }
        } break;
        case StorageBox::INCOMINGCHEQUE:
        case StorageBox::OUTGOINGCHEQUE: {
            auto text =
                api_.Activity().PaymentText(nym_id_, itemID, accountID, reason);

            if (text) {

                return *text;
            } else {
                LogOutput(OT_METHOD)(__FUNCTION__)(
                    ": Cheque item does not exist.")
                    .Flush();
            }
        } break;
        default: {
            OT_FAIL
        }
    }

    return {};
}

void ActivitySummaryItem::get_text() noexcept
{
    auto reason = api_.Factory().PasswordPrompt(__FUNCTION__);
    sLock lock(shared_lock_, std::defer_lock);
    ItemLocator locator{};

    while (running_) {
        if (break_.load()) { return; }

        int taskID{0};

        if (newest_item_.Pop(taskID, locator)) {
            const auto text = find_text(reason, locator);
            lock.lock();
            text_ = text;
            lock.unlock();
            UpdateNotify();
        }

        Sleep(std::chrono::milliseconds(GET_TEXT_MILLISECONDS));
    }
}

std::string ActivitySummaryItem::ImageURI() const noexcept
{
    // TODO

    return {};
}

#if OT_QT
QVariant ActivitySummaryItem::qt_data(const int column, int role) const noexcept
{
    switch (role) {
        case Qt::DisplayRole: {
            switch (column) {
                case ActivitySummaryQt::DisplayNameColumn: {
                    return DisplayName().c_str();
                }
                case ActivitySummaryQt::DisplayTimeColumn: {
                    QDateTime qdatetime;
                    qdatetime.setSecsSinceEpoch(
                        std::chrono::system_clock::to_time_t(Timestamp()));
                    return qdatetime;
                }
                case ActivitySummaryQt::ImageURIColumn: {
                    return ImageURI().c_str();
                }
                case ActivitySummaryQt::TextColumn: {
                    return Text().c_str();
                }
                default: {
                    return {};
                }
            }
        }
        case ActivitySummaryQt::ThreadIdRole: {
            return ThreadID().c_str();
        }
        case ActivitySummaryQt::TypeRole: {
            return static_cast<int>(Type());
        }
        default: {
            return {};
        }
    }
}
#endif  // OT_QT

void ActivitySummaryItem::reindex(
    const ActivitySummarySortKey& key,
    const CustomData& custom) noexcept
{
    eLock lock(shared_lock_);
    key_ = key;
    lock.unlock();
    startup(custom, newest_item_);
}

void ActivitySummaryItem::startup(
    const CustomData& custom,
    UniqueQueue<ItemLocator>& queue) noexcept
{
    const auto id = extract_custom<std::string>(custom, 0);
    const auto account = extract_custom<std::string>(custom, 2);
    ItemLocator locator{id, type_, account};
    queue.Push(++next_task_id_, locator);
}

std::string ActivitySummaryItem::Text() const noexcept
{
    sLock lock(shared_lock_);

    return text_;
}

std::string ActivitySummaryItem::ThreadID() const noexcept
{
    return row_id_->str();
}

std::chrono::system_clock::time_point ActivitySummaryItem::Timestamp() const
    noexcept
{
    sLock lock(shared_lock_);

    return time_;
}

StorageBox ActivitySummaryItem::Type() const noexcept
{
    sLock lock(shared_lock_);

    return type_;
}

ActivitySummaryItem::~ActivitySummaryItem()
{
    break_.store(true);

    if (newest_item_thread_ && newest_item_thread_->joinable()) {
        newest_item_thread_->join();
        newest_item_thread_.reset();
    }
}
}  // namespace opentxs::ui::implementation
