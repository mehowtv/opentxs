// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/contract/SecurityContract.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/Proto.hpp"

#include "core/contract/UnitDefinition.hpp"

#include "SecurityContract.hpp"

namespace opentxs
{
using ReturnType = contract::unit::implementation::Security;

auto Factory::SecurityContract(
    const api::internal::Core& api,
    const Nym_p& nym,
    const std::string& shortname,
    const std::string& name,
    const std::string& symbol,
    const std::string& terms,
    const proto::ContactItemType unitOfAccount,
    const VersionNumber version,
    const opentxs::PasswordPrompt& reason) noexcept
    -> std::shared_ptr<contract::unit::Security>
{
    auto output = std::make_shared<ReturnType>(
        api, nym, shortname, name, symbol, terms, unitOfAccount, version);

    if (false == bool(output)) { return {}; }

    auto& contract = *output;
    Lock lock(contract.lock_);

    if (contract.nym_) {
        auto serialized = contract.SigVersion(lock);
        auto sig = std::make_shared<proto::Signature>();

        if (!contract.update_signature(lock, reason)) { return {}; }
    }

    if (!contract.validate(lock)) { return {}; }

    return std::move(output);
}

auto Factory::SecurityContract(
    const api::internal::Core& api,
    const Nym_p& nym,
    const proto::UnitDefinition serialized) noexcept
    -> std::shared_ptr<contract::unit::Security>
{
    if (false == proto::Validate<ReturnType::SerializedType>(
                     serialized, VERBOSE, true)) {

        return {};
    }

    auto output = std::make_shared<ReturnType>(api, nym, serialized);

    if (false == bool(output)) { return {}; }

    auto& contract = *output;
    Lock lock(contract.lock_);

    if (!contract.validate(lock)) { return {}; }

    return std::move(output);
}
}  // namespace opentxs

namespace opentxs::contract::unit::implementation
{
Security::Security(
    const api::internal::Core& api,
    const Nym_p& nym,
    const std::string& shortname,
    const std::string& name,
    const std::string& symbol,
    const std::string& terms,
    const proto::ContactItemType unitOfAccount,
    const VersionNumber version)
    : Unit(api, nym, shortname, name, symbol, terms, unitOfAccount, version)
{
    Lock lock(lock_);
    first_time_init(lock);
}

Security::Security(
    const api::internal::Core& api,
    const Nym_p& nym,
    const proto::UnitDefinition serialized)
    : Unit(api, nym, serialized)
{
    Lock lock(lock_);
    init_serialized(lock);
}

Security::Security(const Security& rhs)
    : Unit(rhs)
{
}

proto::UnitDefinition Security::IDVersion(const Lock& lock) const
{
    auto contract = Unit::IDVersion(lock);
    contract.set_type(Type());
    auto& security = *contract.mutable_security();
    security.set_version(1);
    security.set_type(proto::EQUITYTYPE_SHARES);

    return contract;
}
}  // namespace opentxs::contract::unit::implementation
