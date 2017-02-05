/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#ifndef OPENTXS_STORAGE_TREE_THREADS_HPP
#define OPENTXS_STORAGE_TREE_THREADS_HPP

#include "opentxs/core/app/Editor.hpp"
#include "opentxs/storage/tree/Node.hpp"
#include "opentxs/storage/Storage.hpp"

#include <map>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

namespace opentxs
{
namespace storage
{

class Mailbox;
class Nym;
class Thread;

class Threads : public Node
{
private:
    friend class Nym;

    mutable std::map<std::string, std::unique_ptr<class Thread>> threads_;
    Mailbox& mail_inbox_;
    Mailbox& mail_outbox_;

    class Thread* thread(const std::string& id) const;
    class Thread* thread(
        const std::string& id,
        const std::unique_lock<std::mutex>& lock) const;
    void save(
        class Thread* thread,
        const std::unique_lock<std::mutex>& lock,
        const std::string& id);

    void init(const std::string& hash) override;
    bool save(const std::unique_lock<std::mutex>& lock) override;
    proto::StorageNymList serialize() const;

    Threads(
        const Storage& storage,
        const keyFunction& migrate,
        const std::string& hash,
        Mailbox& mailInbox,
        Mailbox& mailOutbox);
    Threads() = delete;
    Threads(const Threads&) = delete;
    Threads(Threads&&) = delete;
    Threads operator=(const Threads&) = delete;
    Threads operator=(Threads&&) = delete;

public:
    bool Exists(const std::string& id) const;
    bool Migrate() const override;
    const class Thread& Thread(const std::string& id) const;

    std::string Create(const std::set<std::string>& participants);
    bool FindAndDeleteItem(const std::string& itemID);
    Editor<class Thread> mutable_Thread(const std::string& id);

    ~Threads() = default;
};
}  // namespace storage
}  // namespace opentxs
#endif  // OPENTXS_STORAGE_TREE_THREADS_HPP
