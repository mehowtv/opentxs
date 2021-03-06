// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#include "opentxs/api/Editor.hpp"
#include "opentxs/core/Flag.hpp"
#include "opentxs/Types.hpp"

#include "Node.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <shared_mutex>
#include <string>
#include <tuple>

namespace opentxs::storage
{
class Nym final : public Node
{
public:
    std::set<std::string> BlockchainAccountList(
        const proto::ContactItemType type) const;
    proto::ContactItemType BlockchainAccountType(
        const std::string& accountID) const;

    const storage::Bip47Channels& Bip47Channels() const;
    const storage::Contexts& Contexts() const;
    const PeerReplies& FinishedReplyBox() const;
    const PeerRequests& FinishedRequestBox() const;
    const PeerReplies& IncomingReplyBox() const;
    const PeerRequests& IncomingRequestBox() const;
    const storage::Issuers& Issuers() const;
    const Mailbox& MailInbox() const;
    const Mailbox& MailOutbox() const;
    const PeerReplies& ProcessedReplyBox() const;
    const PeerRequests& ProcessedRequestBox() const;
    const PeerReplies& SentReplyBox() const;
    const PeerRequests& SentRequestBox() const;
    const storage::Threads& Threads() const;
    const storage::PaymentWorkflows& PaymentWorkflows() const;
    const storage::Txos& TXOs() const { return *txos(); }

    Editor<storage::Bip47Channels> mutable_Bip47Channels();
    Editor<storage::Contexts> mutable_Contexts();
    Editor<PeerReplies> mutable_FinishedReplyBox();
    Editor<PeerRequests> mutable_FinishedRequestBox();
    Editor<PeerReplies> mutable_IncomingReplyBox();
    Editor<PeerRequests> mutable_IncomingRequestBox();
    Editor<storage::Issuers> mutable_Issuers();
    Editor<Mailbox> mutable_MailInbox();
    Editor<Mailbox> mutable_MailOutbox();
    Editor<PeerReplies> mutable_ProcessedReplyBox();
    Editor<PeerRequests> mutable_ProcessedRequestBox();
    Editor<PeerReplies> mutable_SentReplyBox();
    Editor<PeerRequests> mutable_SentRequestBox();
    Editor<storage::Threads> mutable_Threads();
    Editor<storage::PaymentWorkflows> mutable_PaymentWorkflows();
    Editor<storage::Txos> mutable_TXOs();

    std::string Alias() const;
    bool Load(
        const std::string& id,
        std::shared_ptr<proto::HDAccount>& output,
        const bool checking) const;
    bool Load(
        std::shared_ptr<proto::Nym>& output,
        std::string& alias,
        const bool checking) const;
    bool Load(
        const identifier::Server& notary,
        const identifier::UnitDefinition& unit,
        std::shared_ptr<proto::Purse>& output,
        const bool checking) const;
    bool Migrate(const opentxs::api::storage::Driver& to) const final;

    bool SetAlias(const std::string& alias);
    bool Store(const proto::ContactItemType type, const proto::HDAccount& data);
    bool Store(
        const proto::Nym& data,
        const std::string& alias,
        std::string& plaintext);
    bool Store(const proto::Purse& purse);

    ~Nym();

private:
    friend Nyms;

    using PurseID = std::pair<OTServerID, OTUnitID>;

    std::string alias_;
    std::string nymid_;
    std::string credentials_;

    mutable OTFlag checked_;
    mutable OTFlag private_;
    mutable std::atomic<std::uint64_t> revision_;

    mutable std::mutex bip47_lock_;
    mutable std::unique_ptr<storage::Bip47Channels> bip47_;
    std::string bip47_root_;
    mutable std::mutex sent_request_box_lock_;
    mutable std::unique_ptr<PeerRequests> sent_request_box_;
    std::string sent_peer_request_;
    mutable std::mutex incoming_request_box_lock_;
    mutable std::unique_ptr<PeerRequests> incoming_request_box_;
    std::string incoming_peer_request_;
    mutable std::mutex sent_reply_box_lock_;
    mutable std::unique_ptr<PeerReplies> sent_reply_box_;
    std::string sent_peer_reply_;
    mutable std::mutex incoming_reply_box_lock_;
    mutable std::unique_ptr<PeerReplies> incoming_reply_box_;
    std::string incoming_peer_reply_;
    mutable std::mutex finished_request_box_lock_;
    mutable std::unique_ptr<PeerRequests> finished_request_box_;
    std::string finished_peer_request_;
    mutable std::mutex finished_reply_box_lock_;
    mutable std::unique_ptr<PeerReplies> finished_reply_box_;
    std::string finished_peer_reply_;
    mutable std::mutex processed_request_box_lock_;
    mutable std::unique_ptr<PeerRequests> processed_request_box_;
    std::string processed_peer_request_;
    mutable std::mutex processed_reply_box_lock_;
    mutable std::unique_ptr<PeerReplies> processed_reply_box_;
    std::string processed_peer_reply_;
    mutable std::mutex mail_inbox_lock_;
    mutable std::unique_ptr<Mailbox> mail_inbox_;
    std::string mail_inbox_root_;
    mutable std::mutex mail_outbox_lock_;
    mutable std::unique_ptr<Mailbox> mail_outbox_;
    std::string mail_outbox_root_;
    mutable std::mutex threads_lock_;
    mutable std::unique_ptr<storage::Threads> threads_;
    std::string threads_root_;
    mutable std::mutex contexts_lock_;
    mutable std::unique_ptr<storage::Contexts> contexts_;
    std::string contexts_root_;
    mutable std::mutex blockchain_lock_;
    std::map<proto::ContactItemType, std::set<std::string>>
        blockchain_account_types_{};
    std::map<std::string, proto::ContactItemType> blockchain_account_index_;
    std::map<std::string, std::shared_ptr<proto::HDAccount>>
        blockchain_accounts_{};
    std::string issuers_root_;
    mutable std::mutex issuers_lock_;
    mutable std::unique_ptr<storage::Issuers> issuers_;
    std::string workflows_root_;
    mutable std::mutex workflows_lock_;
    mutable std::unique_ptr<storage::PaymentWorkflows> workflows_;
    std::map<PurseID, std::string> purse_id_;
    mutable std::mutex txo_lock_;
    mutable std::unique_ptr<storage::Txos> txo_;
    std::string txo_root_;

    template <typename T, typename... Args>
    T* construct(
        std::mutex& mutex,
        std::unique_ptr<T>& pointer,
        const std::string& root,
        Args&&... params) const;

    storage::Bip47Channels* bip47() const;
    PeerRequests* sent_request_box() const;
    PeerRequests* incoming_request_box() const;
    PeerReplies* sent_reply_box() const;
    PeerReplies* incoming_reply_box() const;
    PeerRequests* finished_request_box() const;
    PeerReplies* finished_reply_box() const;
    PeerRequests* processed_request_box() const;
    PeerReplies* processed_reply_box() const;
    Mailbox* mail_inbox() const;
    Mailbox* mail_outbox() const;
    storage::Threads* threads() const;
    storage::Contexts* contexts() const;
    storage::Issuers* issuers() const;
    storage::PaymentWorkflows* workflows() const;
    storage::Txos* txos() const
    {
        return construct<storage::Txos>(txo_lock_, txo_, txo_root_);
    }

    template <typename T>
    Editor<T> editor(
        std::string& root,
        std::mutex& mutex,
        T* (Nym::*get)() const);

    void init(const std::string& hash) final;
    bool save(const Lock& lock) const final;
    template <typename O>
    void _save(
        O* input,
        const Lock& lock,
        std::mutex& mutex,
        std::string& root);
    proto::StorageNym serialize() const;

    Nym(const opentxs::api::storage::Driver& storage,
        const std::string& id,
        const std::string& hash,
        const std::string& alias);
    Nym() = delete;
    Nym(const identity::Nym&) = delete;
    Nym(Nym&&) = delete;
    Nym operator=(const identity::Nym&) = delete;
    Nym operator=(Nym&&) = delete;
};
}  // namespace opentxs::storage
