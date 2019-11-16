// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "OpenDHT.hpp"

#include "opentxs/core/Log.hpp"
#include "opentxs/Types.hpp"

#include "DhtConfig.hpp"

#if OT_DHT
#include <opendht.h>
#include <opendht/value.h>
#endif

#include <iostream>
#include <stdexcept>
#include <vector>

#if OT_DHT
#define OT_METHOD "opentxs::OpenDHT::"
#endif

namespace opentxs::network::implementation
{
#if OT_DHT

auto OpenDHT::init_node() noexcept -> std::unique_ptr<dht::DhtRunner>
{
    try {
        return std::unique_ptr<dht::DhtRunner>(new dht::DhtRunner);
    } catch (...) {
        return {};
    }
}

OpenDHT::OpenDHT(const DhtConfig& config)
    : config_(new DhtConfig(config))
    , node_(init_node())
    , loaded_(Flag::Factory(false))
    , ready_(Flag::Factory(false))
    , init_()
{
    Init();
}

bool OpenDHT::Init() const
{
    Lock initLock(init_);

    if (!config_->enable_dht_) { return true; }

    if (ready_.get()) { return true; }

    if (!node_) { return false; }

    std::int64_t listenPort = config_->listen_port_;

    if ((listenPort <= 1000) || (listenPort >= 65535)) {
        listenPort = config_->default_port_;
    }

    if (!loaded_.get()) {
        try {
            node_->run(
                static_cast<unsigned short>(listenPort),
                dht::crypto::generateIdentity(),
                true);
        } catch (std::exception& e) {
            std::cout << e.what() << std::endl;

            return false;
        }

        loaded_->On();
    }

    try {
        node_->bootstrap(
            config_->bootstrap_url_.c_str(), config_->bootstrap_port_.c_str());
        ready_->On();
    } catch (std::exception& e) {
        std::cout << e.what() << std::endl;

        return false;
    }

    return true;
}

void OpenDHT::Insert(
    const std::string& key,
    const std::string& value,
    DhtDoneCallback cb) const
{
    if (!ready_.get()) {
        if (!Init()) { return; }
    }

    dht::InfoHash infoHash = dht::InfoHash::get(
        reinterpret_cast<const std::uint8_t*>(key.c_str()), key.size());

    std::shared_ptr<dht::Value> pValue = std::make_shared<dht::Value>(
        reinterpret_cast<const std::uint8_t*>(value.c_str()), value.size());

    if (!pValue) { return; }

    if (value.size() > dht::MAX_VALUE_SIZE) {
        LogOutput(OT_METHOD)(__FUNCTION__)(
            ": Error: data size exceeds DHT limits.")
            .Flush();
        return;
    }

    try {
        node_->put(infoHash, pValue, cb);
    } catch (...) {
        return;
    }
}

void OpenDHT::Retrieve(
    const std::string& key,
    DhtResultsCallback vcb,
    DhtDoneCallback dcb) const
{
    if (!ready_.get()) {
        if (!Init()) { return; }
    }

    // The OpenDHT get method wants a lambda function that accepts an
    // OpenDHT-specific type as an argument. I don't consumers of this class
    // to need to include OpenDHT headers. Solution: this lambda performs
    // the translation
    dht::GetCallback cb(
        [vcb](const std::vector<std::shared_ptr<dht::Value>> results) -> bool {
            DhtResults input;

            for (const auto& it : results) {
                if (nullptr == it) { continue; }

                input.emplace(
                    input.end(),
                    new std::string(it->data.begin(), it->data.end()));
            }

            return vcb(input);
        });

    dht::InfoHash infoHash = dht::InfoHash::get(
        reinterpret_cast<const std::uint8_t*>(key.c_str()), key.size());

    try {
        node_->get(infoHash, cb, dcb, dht::Value::AllFilter());
    } catch (...) {
        return;
    }
}

OpenDHT::~OpenDHT()
{
    if (node_) { node_->join(); }
}
#endif
}  // namespace opentxs::network::implementation
