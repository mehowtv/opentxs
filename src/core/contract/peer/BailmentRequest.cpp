// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/contract/peer/BailmentRequest.hpp"

#include "opentxs/api/Core.hpp"
#include "opentxs/api/Factory.hpp"
#include "opentxs/core/String.hpp"

#include "internal/api/Api.hpp"

#define CURRENT_VERSION 4

namespace opentxs
{
BailmentRequest::BailmentRequest(
    const api::internal::Core& api,
    const Nym_p& nym,
    const proto::PeerRequest& serialized)
    : ot_super(api, nym, serialized)
    , unit_(api_.Factory().UnitID(serialized.bailment().unitid()))
    , server_(api_.Factory().ServerID(serialized.bailment().serverid()))
{
}

BailmentRequest::BailmentRequest(
    const api::internal::Core& api,
    const Nym_p& nym,
    const identifier::Nym& recipientID,
    const identifier::UnitDefinition& unitID,
    const identifier::Server& serverID)
    : ot_super(
          api,
          nym,
          CURRENT_VERSION,
          recipientID,
          serverID,
          proto::PEERREQUEST_BAILMENT)
    , unit_(unitID)
    , server_(serverID)
{
}

proto::PeerRequest BailmentRequest::IDVersion(const Lock& lock) const
{
    auto contract = ot_super::IDVersion(lock);
    auto& bailment = *contract.mutable_bailment();
    bailment.set_version(version_);
    bailment.set_unitid(String::Factory(unit_)->Get());
    bailment.set_serverid(String::Factory(server_)->Get());

    return contract;
}
}  // namespace opentxs
