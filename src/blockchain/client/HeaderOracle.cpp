// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Internal.hpp"

#include "opentxs/blockchain/block/Header.hpp"
#include "opentxs/blockchain/client/HeaderOracle.hpp"
#include "opentxs/blockchain/NumericHash.hpp"
#include "opentxs/blockchain/Work.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Log.hpp"

#include "blockchain/client/UpdateTransaction.hpp"
#include "internal/api/Api.hpp"
#include "internal/blockchain/Blockchain.hpp"

#include <algorithm>
#include <map>
#include <mutex>
#include <tuple>

#include "HeaderOracle.hpp"

#define OT_METHOD "opentxs::blockchain::client::implementation::HeaderOracle::"

namespace opentxs
{
auto Factory::HeaderOracle(
    const api::internal::Core& api,
    const blockchain::client::internal::Network& network,
    const blockchain::client::internal::HeaderDatabase& database,
    const blockchain::Type type) noexcept
    -> std::unique_ptr<blockchain::client::internal::HeaderOracle>
{
    using ReturnType = blockchain::client::implementation::HeaderOracle;

    return std::make_unique<ReturnType>(api, network, database, type);
}
}  // namespace opentxs

namespace opentxs::blockchain::client
{
const std::map<Type, block::pHash> genesis_hashes_{
    {Type::Bitcoin,
     Data::Factory(
         "0x6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000",
         Data::Mode::Hex)},
    {Type::Bitcoin_testnet3,
     Data::Factory(
         "0x43497fd7f826957108f4a30fd9cec3aeba79972084e90ead01ea330900000000",
         Data::Mode::Hex)},
    {Type::BitcoinCash,
     Data::Factory(
         "0x6fe28c0ab6f1b372c1a6a246ae63f74f931e8365e15a089c68d6190000000000",
         Data::Mode::Hex)},
    {Type::BitcoinCash_testnet3,
     Data::Factory(
         "0x43497fd7f826957108f4a30fd9cec3aeba79972084e90ead01ea330900000000",
         Data::Mode::Hex)},
    {Type::Ethereum_frontier,
     Data::Factory(
         "0xd4e56740f876aef8c010b86a40d5f56745a118d0906a34e69aec8c0db1cb8fa3",
         Data::Mode::Hex)},
    {Type::Ethereum_ropsten,
     Data::Factory(
         "0x41941023680923e0fe4d74a34bdac8141f2540e3ae90623718e47d66d1ca4a2d",
         Data::Mode::Hex)},
};

auto HeaderOracle::GenesisBlockHash(const blockchain::Type type)
    -> const block::Hash&
{
    return genesis_hashes_.at(type);
}
}  // namespace opentxs::blockchain::client

namespace opentxs::blockchain::client::implementation
{
const std::map<blockchain::Type, std::pair<block::Height, std::string>>
    HeaderOracle::checkpoints_{
        {blockchain::Type::BitcoinCash,
         {609136,
          "b1c55b4f69aa2e3209c91ae413c355c65aacfa07b28bb4000000000000000000"}},
        {blockchain::Type::BitcoinCash_testnet3,
         {1341712,
          "5ba3af2992073940ed9e5a9d9eef9194bbfba905d92b202eea44fcff00000000"}},
    };

HeaderOracle::HeaderOracle(
    const api::internal::Core& api,
    const internal::Network& network,
    const internal::HeaderDatabase& database,
    const blockchain::Type type) noexcept
    : internal::HeaderOracle()
    , api_(api)
    , network_(network)
    , database_(database)
    , chain_(type)
    , lock_()
{
}

auto HeaderOracle::AddCheckpoint(
    const block::Height position,
    const block::Hash& requiredHash) noexcept -> bool
{
    Lock lock(lock_);
    auto update = UpdateTransaction{api_, database_};

    if (update.EffectiveCheckpoint()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Checkpoint already exists")
            .Flush();

        return false;
    }

    if (2 > position) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid position").Flush();

        return false;
    }

    update.SetCheckpoint({position, requiredHash});

    if (apply_checkpoint(lock, position, update)) {

        return database_.ApplyUpdate(update);
    } else {

        return false;
    }
}

auto HeaderOracle::AddHeader(std::unique_ptr<block::Header> header) noexcept
    -> bool
{
    auto headers = std::vector<std::unique_ptr<block::Header>>{};
    headers.emplace_back(std::move(header));

    return AddHeaders(headers);
}

auto HeaderOracle::AddHeaders(
    std::vector<std::unique_ptr<block::Header>>& headers) noexcept -> bool
{
    if (0 == headers.size()) { return false; }

    Lock lock(lock_);
    auto update = UpdateTransaction{api_, database_};

    for (auto& header : headers) {
        if (false == bool(header)) {
            LogOutput(OT_METHOD)(__FUNCTION__)(": Invalid header").Flush();

            return false;
        }

        if (false == add_header(lock, update, std::move(header))) {

            return false;
        }
    }

    return database_.ApplyUpdate(update);
}

auto HeaderOracle::add_header(
    const Lock& lock,
    UpdateTransaction& update,
    std::unique_ptr<block::Header> pHeader) noexcept -> bool
{
    if (update.EffectiveHeaderExists(pHeader->Hash())) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Header already processed")
            .Flush();

        return true;
    }

    auto& header = update.Stage(std::move(pHeader));
    const auto& current = update.Stage();
    const auto [currentHeight, currentHash] = current.Position();
    const auto* pParent = is_disconnected(header.ParentHash(), update);

    if (nullptr == pParent) {
        LogVerbose(OT_METHOD)(__FUNCTION__)(": Adding disconnected header")
            .Flush();
        header.SetDisconnectedState();
        update.DisconnectBlock(header);

        return true;
    }

    OT_ASSERT(nullptr != pParent);

    const auto& parent = *pParent;

    if (update.EffectiveIsSibling(header.ParentHash())) {
        update.RemoveSibling(header.ParentHash());
    }

    auto candidates = Candidates{};

    try {
        auto& candidate = initialize_candidate(
            lock, current, parent, update, candidates, header);
        connect_children(lock, header, candidates, candidate, update);
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to connect children")
            .Flush();

        return false;
    }

    return choose_candidate(current, candidates, update).first;
}

auto HeaderOracle::apply_checkpoint(
    const Lock& lock,
    const block::Height position,
    UpdateTransaction& update) noexcept -> bool
{
    auto& best = update.Stage();

    if (position > best.Height()) { return true; }

    try {
        const auto& ancestor = update.Stage(position - 1);
        auto candidates = Candidates{};
        stage_candidate(lock, ancestor, candidates, update, best);

        for (const auto& hash : update.EffectiveSiblingHashes()) {
            stage_candidate(
                lock, ancestor, candidates, update, update.Stage(hash));
        }

        for (auto& [invalid, chain] : candidates) {
            const block::Header* pParent = &ancestor;

            for (const auto& [height, hash] : chain) {
                auto& child = update.Header(hash);
                invalid = connect_to_parent(lock, update, *pParent, child);
                pParent = &child;
            }
        }

        const auto [success, found] =
            choose_candidate(ancestor, candidates, update);

        if (false == success) { return false; }

        if (false == found) {
            const auto fallback = ancestor.Position();
            update.SetReorgParent(fallback);
            update.AddToBestChain(fallback);
        }

        return true;
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Failed to process sibling chains")
            .Flush();

        return false;
    }
}

auto HeaderOracle::best_chain(const Lock& lock) const noexcept
    -> block::Position
{
    return database_.CurrentBest()->Position();
}

auto HeaderOracle::BestChain() const noexcept -> block::Position
{
    Lock lock(lock_);

    return best_chain(lock);
}

auto HeaderOracle::BestHash(const block::Height height) const noexcept
    -> block::pHash
{
    Lock lock(lock_);

    try {
        return database_.BestBlock(height);
    } catch (...) {
        return make_blank<block::pHash>::value(api_);
    }
}

auto HeaderOracle::choose_candidate(
    const block::Header& current,
    const Candidates& candidates,
    UpdateTransaction& update) noexcept(false) -> std::pair<bool, bool>
{
    auto output = std::pair<bool, bool>{false, false};
    auto& [success, found] = output;

    try {
        const block::Header* pBest{&current};

        for (const auto& candidate : candidates) {
            if (candidate.blacklisted_) { continue; }

            OT_ASSERT(0 < candidate.chain_.size());

            const auto& position = *candidate.chain_.crbegin();
            const auto& tip = update.Header(position.second);

            if (evaluate_candidate(*pBest, tip)) { pBest = &tip; }
        }

        OT_ASSERT(nullptr != pBest);

        const auto& best = *pBest;

        for (const auto& candidate : candidates) {
            OT_ASSERT(0 < candidate.chain_.size());

            const auto& position = *candidate.chain_.crbegin();
            const auto& tip = update.Header(position.second);

            if (tip.Hash() == best.Hash()) {
                found = true;
                auto reorg{false};

                for (const auto& segment : candidate.chain_) {
                    const auto& [height, hash] = segment;

                    if ((height <= current.Height()) && (false == reorg)) {
                        if (hash.get() == update.EffectiveBestBlock(height)) {
                            continue;
                        } else {
                            reorg = true;
                            const auto parent = block::Position{
                                height - 1,
                                update.EffectiveBestBlock(height - 1)};
                            update.SetReorgParent(parent);
                            update.AddToBestChain(segment);
                            update.AddSibling(current.Position());
                        }
                    } else {
                        update.AddToBestChain(segment);
                    }
                }
            } else {
                update.AddSibling(tip.Position());
            }
        }
    } catch (...) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": Error evaluating candidates")
            .Flush();

        return output;
    }

    success = true;

    return output;
}

auto HeaderOracle::CommonParent(const block::Position& position) const noexcept
    -> std::pair<block::Position, block::Position>
{
    Lock lock(lock_);
    const auto& database = database_;
    std::pair<block::Position, block::Position> output{
        {0, GenesisBlockHash(chain_)}, best_chain(lock)};
    auto& [parent, best] = output;
    auto test{position};
    auto pHeader = database.TryLoadHeader(test.second);

    if (false == bool(pHeader)) { return output; }

    while (0 < test.first) {
        if (is_in_best_chain(lock, test.second)) {
            parent = test;

            return output;
        }

        pHeader = database.TryLoadHeader(pHeader->ParentHash());

        if (pHeader) {
            test = pHeader->Position();
        } else {
            return output;
        }
    }

    return output;
}

auto HeaderOracle::connect_children(
    const Lock& lock,
    block::Header& parent,
    Candidates& candidates,
    Candidate& candidate,
    UpdateTransaction& update) -> void
{
    auto& chain = candidate.chain_;
    auto& end = *chain.crbegin();

    OT_ASSERT(end.first + 1 == parent.Position().first);

    chain.emplace_back(parent.Position());

    if (false == update.EffectiveHasDisconnectedChildren(parent.Hash())) {
        return;
    }

    const auto disconnected = update.EffectiveDisconnectedHashes();
    const auto [first, last] = disconnected.equal_range(parent.Hash());
    std::atomic<bool> firstChild{true};
    const auto original{candidate};
    std::for_each(first, last, [&](const auto& in) -> void {
        const auto& [parentHash, childHash] = in;
        update.ConnectBlock({parentHash, childHash});
        auto& child = update.Stage(childHash);
        candidate.blacklisted_ = connect_to_parent(lock, update, parent, child);
        // The first child block extends the current candidate. Subsequent child
        // blocks create a new candidate to extend. This transforms the tree
        // of disconnected blocks into a table of candidates.
        auto& chainToExtend = firstChild.exchange(false)
                                  ? candidate
                                  : candidates.emplace_back(original);
        connect_children(lock, child, candidates, chainToExtend, update);
    });
}

auto HeaderOracle::connect_to_parent(
    const Lock& lock,
    const UpdateTransaction& update,
    const block::Header& parent,
    block::Header& child) noexcept -> bool
{
    child.InheritWork(parent.Work());
    child.InheritState(parent);
    child.InheritHeight(parent);
    child.CompareToCheckpoint(update.Checkpoint());

    return child.IsBlacklisted();
}

auto HeaderOracle::DeleteCheckpoint() noexcept -> bool
{
    Lock lock(lock_);
    auto update = UpdateTransaction{api_, database_};

    if (false == update.EffectiveCheckpoint()) {
        LogOutput(OT_METHOD)(__FUNCTION__)(": No checkpoint").Flush();

        return false;
    }

    const auto position = update.Checkpoint().first;
    update.ClearCheckpoint();

    if (apply_checkpoint(lock, position, update)) {

        return database_.ApplyUpdate(update);
    } else {

        return false;
    }
}

auto HeaderOracle::evaluate_candidate(
    const block::Header& current,
    const block::Header& candidate) noexcept -> bool
{
    return candidate.Work() > current.Work();
}

auto HeaderOracle::GetCheckpoint() const noexcept -> block::Position
{
    Lock lock(lock_);

    return database_.CurrentCheckpoint();
}

auto HeaderOracle::initialize_candidate(
    const Lock& lock,
    const block::Header& best,
    const block::Header& parent,
    UpdateTransaction& update,
    Candidates& candidates,
    block::Header& child,
    const block::Hash& stopHash) noexcept(false) -> Candidate&
{
    const auto blacklisted = connect_to_parent(lock, update, parent, child);
    auto position{parent.Position()};
    auto& output = candidates.emplace_back(Candidate{blacklisted, {}});
    auto& chain = output.chain_;
    const block::Header* grandparent = &parent;
    using StopFunction = std::function<bool(const block::Position&)>;
    auto run =
        stopHash.empty() ? StopFunction{[&update](const auto& in) -> bool {
            return update.EffectiveBestBlock(in.first) != in.second;
        }}
                         : StopFunction{[&stopHash](const auto& in) -> bool {
                               return stopHash != in.second;
                           }};

    while (run(position)) {
        OT_ASSERT(0 <= position.first);
        OT_ASSERT(grandparent);

        chain.insert(chain.begin(), position);
        grandparent = &update.Stage(grandparent->ParentHash());
        position = grandparent->Position();
    }

    if (0 == chain.size()) { chain.emplace_back(position); }

    OT_ASSERT(0 < chain.size());

    return output;
}

auto HeaderOracle::IsInBestChain(const block::Hash& hash) const noexcept -> bool
{
    Lock lock(lock_);

    return is_in_best_chain(lock, hash);
}

auto HeaderOracle::is_disconnected(
    const block::Hash& parent,
    UpdateTransaction& update) noexcept -> const block::Header*
{
    try {
        const auto& header = update.Stage(parent);

        if (header.IsDisconnected()) {

            return nullptr;
        } else {

            return &header;
        }
    } catch (...) {

        return nullptr;
    }
}

auto HeaderOracle::is_in_best_chain(const Lock& lock, const block::Hash& hash)
    const noexcept -> bool
{
    const auto pHeader = database_.TryLoadHeader(hash);

    if (false == bool(pHeader)) { return false; }

    const auto& header = *pHeader;

    try {
        return hash == database_.BestBlock(header.Height());

    } catch (...) {

        return false;
    }
}

auto HeaderOracle::LoadHeader(const block::Hash& hash) const noexcept
    -> std::unique_ptr<block::Header>
{
    return database_.TryLoadHeader(hash);
}

auto HeaderOracle::stage_candidate(
    const Lock& lock,
    const block::Header& best,
    Candidates& candidates,
    UpdateTransaction& update,
    block::Header& child) noexcept(false) -> void
{
    const auto position = best.Height() + 1;

    if (child.Height() < position) {

        return;
    } else if (child.Height() == position) {
        candidates.emplace_back(Candidate{false, {child.Position()}});
    } else {
        auto& candidate = initialize_candidate(
            lock,
            best,
            update.Stage(child.ParentHash()),
            update,
            candidates,
            child,
            best.Hash());
        candidate.chain_.emplace_back(child.Position());
        const auto first = candidate.chain_.cbegin()->first;

        OT_ASSERT(position == first);
    }
}

auto HeaderOracle::Siblings() const noexcept -> std::set<block::pHash>
{
    Lock lock(lock_);

    return database_.SiblingHashes();
}
}  // namespace opentxs::blockchain::client::implementation
