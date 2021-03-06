// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/Log.hpp"
#include "opentxs/network/zeromq/FrameSection.hpp"
#include "opentxs/network/zeromq/Frame.hpp"
#include "opentxs/network/zeromq/Message.hpp"
#include "opentxs/Proto.tpp"

#include "PairEventCallback.hpp"

template class opentxs::Pimpl<opentxs::network::zeromq::PairEventCallback>;

//#define OT_METHOD
//"opentxs::network::zeromq::implementation::PairEventCallback::"

namespace opentxs::network::zeromq
{
OTZMQPairEventCallback PairEventCallback::Factory(
    zeromq::PairEventCallback::ReceiveCallback callback)
{
    return OTZMQPairEventCallback(
        new implementation::PairEventCallback(callback));
}
}  // namespace opentxs::network::zeromq

namespace opentxs::network::zeromq::implementation
{
PairEventCallback::PairEventCallback(
    zeromq::PairEventCallback::ReceiveCallback callback)
    : callback_(callback)
{
}

PairEventCallback* PairEventCallback::clone() const
{
    return new PairEventCallback(callback_);
}

void PairEventCallback::Process(zeromq::Message& message) const
{
    OT_ASSERT(1 == message.Body().size());

    const auto event = proto::Factory<proto::PairEvent>(message.Body_at(0));
    callback_(event);
}

PairEventCallback::~PairEventCallback() {}
}  // namespace opentxs::network::zeromq::implementation
