// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Feefilter.hpp"

#include <boost/endian/buffers.hpp>

//#define OT_METHOD " opentxs::blockchain::p2p::bitcoin::message::Feefilter::"

namespace opentxs
{
using FeeRateField = be::little_uint64_buf_t;

blockchain::p2p::bitcoin::message::Feefilter* Factory::BitcoinP2PFeefilter(
    const api::internal::Core& api,
    std::unique_ptr<blockchain::p2p::bitcoin::Header> pHeader,
    const blockchain::p2p::bitcoin::ProtocolVersion version,
    const void* payload,
    const std::size_t size)
{
    namespace be = boost::endian;
    namespace bitcoin = blockchain::p2p::bitcoin;
    using ReturnType = bitcoin::message::Feefilter;

    if (false == bool(pHeader)) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": Invalid header")
            .Flush();

        return nullptr;
    }

    auto expectedSize = sizeof(FeeRateField);

    if (expectedSize > size) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(
            ": Size below minimum for Feefilter 1")
            .Flush();

        return nullptr;
    }
    auto* it{static_cast<const std::byte*>(payload)};

    FeeRateField raw_rate;
    std::memcpy(reinterpret_cast<std::byte*>(&raw_rate), it, sizeof(raw_rate));
    it += sizeof(raw_rate);

    const std::uint64_t fee_rate = raw_rate.value();

    try {
        return new ReturnType(api, std::move(pHeader), fee_rate);
    } catch (...) {
        LogOutput("opentxs::Factory::")(__FUNCTION__)(": Checksum failure")
            .Flush();

        return nullptr;
    }
}

blockchain::p2p::bitcoin::message::Feefilter* Factory::BitcoinP2PFeefilter(
    const api::internal::Core& api,
    const blockchain::Type network,
    const std::uint64_t fee_rate)
{
    namespace bitcoin = blockchain::p2p::bitcoin;
    using ReturnType = bitcoin::message::Feefilter;

    return new ReturnType(api, network, fee_rate);
}
}  // namespace opentxs

namespace opentxs::blockchain::p2p::bitcoin::message
{
Feefilter::Feefilter(
    const api::internal::Core& api,
    const blockchain::Type network,
    const std::uint64_t fee_rate) noexcept
    : Message(api, network, bitcoin::Command::feefilter)
    , fee_rate_(fee_rate)
{
    init_hash();
}

Feefilter::Feefilter(
    const api::internal::Core& api,
    std::unique_ptr<Header> header,
    const std::uint64_t fee_rate) noexcept(false)
    : Message(api, std::move(header))
    , fee_rate_(fee_rate)
{
    verify_checksum();
}

OTData Feefilter::payload() const noexcept
{
    FeeRateField fee_rate(fee_rate_);
    OTData output(Data::Factory(&fee_rate, sizeof(fee_rate)));

    return output;
}
}  // namespace  opentxs::blockchain::p2p::bitcoin::message
