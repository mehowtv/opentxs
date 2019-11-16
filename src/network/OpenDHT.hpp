// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include "Internal.hpp"

#if OT_DHT
#include "opentxs/network/OpenDHT.hpp"

#include "opentxs/core/Flag.hpp"

#include <atomic>
#include <memory>
#include <mutex>

namespace dht
{
class DhtRunner;
}

namespace opentxs::network::implementation
{
class OpenDHT final : virtual public network::OpenDHT
{
public:
    void Insert(
        const std::string& key,
        const std::string& value,
        DhtDoneCallback cb = {}) const final;
    void Retrieve(
        const std::string& key,
        DhtResultsCallback vcb,
        DhtDoneCallback dcb = {}) const final;

    ~OpenDHT() final;

private:
    friend class api::network::implementation::Dht;

    std::unique_ptr<const DhtConfig> config_;
    std::unique_ptr<dht::DhtRunner> node_;
    mutable OTFlag loaded_;
    mutable OTFlag ready_;
    mutable std::mutex init_;

    bool Init() const;

    auto init_node() noexcept -> std::unique_ptr<dht::DhtRunner>;

    OpenDHT(const DhtConfig& config);
    OpenDHT() = delete;
    OpenDHT(const OpenDHT&) = delete;
    OpenDHT(OpenDHT&&) = delete;
    OpenDHT& operator=(const OpenDHT&) = delete;
    OpenDHT& operator=(OpenDHT&&) = delete;
};
}  // namespace opentxs::network::implementation
#endif  // OT_DHT
