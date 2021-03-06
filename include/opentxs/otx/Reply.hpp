// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef OPENTXS_OTX_REPLY_HPP
#define OPENTXS_OTX_REPLY_HPP

#include "opentxs/Forward.hpp"

#include "opentxs/core/contract/Signable.hpp"
#include "opentxs/Proto.hpp"
#include "opentxs/Types.hpp"

#include <memory>

namespace opentxs
{
using OTXReply = Pimpl<otx::Reply>;

namespace api
{
namespace internal
{
struct Core;
}  // namespace internal
}  // namespace api

namespace otx
{
class Reply : virtual public opentxs::contract::Signable
{
public:
    OPENTXS_EXPORT static const VersionNumber DefaultVersion;
    OPENTXS_EXPORT static const VersionNumber MaxVersion;

    OPENTXS_EXPORT static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const Nym_p signer,
        const identifier::Nym& recipient,
        const identifier::Server& server,
        const proto::ServerReplyType type,
        const RequestNumber number,
        const bool success,
        const PasswordPrompt& reason,
        std::shared_ptr<const proto::OTXPush>&& push = {});
    OPENTXS_EXPORT static Pimpl<opentxs::otx::Reply> Factory(
        const api::internal::Core& api,
        const proto::ServerReply serialized);

    OPENTXS_EXPORT virtual proto::ServerReply Contract() const = 0;
    OPENTXS_EXPORT virtual RequestNumber Number() const = 0;
    OPENTXS_EXPORT virtual std::shared_ptr<const proto::OTXPush> Push()
        const = 0;
    OPENTXS_EXPORT virtual const identifier::Nym& Recipient() const = 0;
    OPENTXS_EXPORT virtual const identifier::Server& Server() const = 0;
    OPENTXS_EXPORT virtual bool Success() const = 0;
    OPENTXS_EXPORT virtual proto::ServerReplyType Type() const = 0;

    OPENTXS_EXPORT ~Reply() override = default;

protected:
    Reply() = default;

private:
    friend OTXReply;

#ifndef _WIN32
    Reply* clone() const noexcept override = 0;
#endif

    Reply(const Reply&) = delete;
    Reply(Reply&&) = delete;
    Reply& operator=(const Reply&) = delete;
    Reply& operator=(Reply&&) = delete;
};
}  // namespace otx
}  // namespace opentxs
#endif
