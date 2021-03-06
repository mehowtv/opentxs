// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "Crypto.hpp"

#include "opentxs/api/crypto/Config.hpp"
#include "opentxs/api/crypto/Encode.hpp"
#include "opentxs/api/crypto/Hash.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/crypto/key/Symmetric.hpp"
#include "opentxs/crypto/library/AsymmetricProvider.hpp"
#if OT_CRYPTO_USING_OPENSSL
#include "opentxs/crypto/library/OpenSSL.hpp"
#endif
#include "opentxs/crypto/library/SymmetricProvider.hpp"
#if OT_CRYPTO_USING_LIBSECP256K1
#include "opentxs/crypto/library/Secp256k1.hpp"
#endif
#include "opentxs/crypto/library/Ripemd160.hpp"
#include "opentxs/crypto/library/Sodium.hpp"
#include "opentxs/crypto/Bip32.hpp"
#include "opentxs/crypto/Bip39.hpp"

#include "crypto/Bip32.hpp"
#include "internal/api/crypto/Crypto.hpp"

#include <functional>
#include <ostream>
#include <vector>

extern "C" {
#ifdef _WIN32
#else
#include <sys/resource.h>
#endif
}

#define OT_METHOD "opentxs::Crypto::"

template class opentxs::Pimpl<opentxs::crypto::key::Symmetric>;

namespace opentxs
{
api::Crypto* Factory::Crypto(const api::Settings& settings)
{
    return new api::implementation::Crypto(settings);
}
}  // namespace opentxs

namespace opentxs::api::implementation
{
Crypto::Crypto(const api::Settings& settings)
    : config_(opentxs::Factory::CryptoConfig(settings))
    , sodium_(opentxs::Factory::Sodium(*this))
#if OT_CRYPTO_USING_OPENSSL
    , ssl_(opentxs::Factory::OpenSSL(*this))
#endif
    , util_(*sodium_)
#if OT_CRYPTO_USING_LIBSECP256K1
    , secp256k1_p_(opentxs::Factory::Secp256k1(*this, util_))
#endif  // OT_CRYPTO_USING_LIBSECP256K1
    , bip39_p_(opentxs::Factory::Bip39(*this))
    , ripemd160_(*ssl_)
    , bip32_p_(std::make_unique<opentxs::crypto::implementation::Bip32>(*this))
    , bip32_(*bip32_p_)
    , bip39_(*bip39_p_)
#if OT_CRYPTO_SUPPORTED_KEY_SECP256K1
#if OT_CRYPTO_USING_LIBSECP256K1
    , secp256k1_(*secp256k1_p_)
#endif  // OT_CRYPTO_USING_LIBSECP256K1
#endif  // OT_CRYPTO_SUPPORTED_KEY_SECP256K1
    , encode_(opentxs::Factory::Encode(*this))
    , hash_(
          opentxs::Factory::Hash(*encode_, *ssl_, *sodium_, *ssl_, ripemd160_))
{
    OT_ASSERT(bip32_p_)
    OT_ASSERT(sodium_)
#if OT_CRYPTO_USING_OPENSSL
    OT_ASSERT(ssl_)
#endif
#if OT_CRYPTO_USING_LIBSECP256K1
    OT_ASSERT(secp256k1_p_)
#endif

    Init();
}

const opentxs::crypto::Bip32& Crypto::BIP32() const { return bip32_; }

const opentxs::crypto::Bip39& Crypto::BIP39() const { return bip39_; }

void Crypto::Cleanup()
{
    hash_.reset();
    encode_.reset();
#if OT_CRYPTO_USING_LIBSECP256K1
    secp256k1_p_.reset();
#endif
    sodium_.reset();
    bip32_p_.reset();
    config_.reset();
}

const crypto::Config& Crypto::Config() const
{
    OT_ASSERT(config_);

    return *config_;
}

#if OT_CRYPTO_SUPPORTED_KEY_ED25519
const opentxs::crypto::EcdsaProvider& Crypto::ED25519() const
{
    OT_ASSERT(sodium_);

    return *sodium_;
}
#endif  // OT_CRYPTO_SUPPORTED_KEY_ED25519

const crypto::Encode& Crypto::Encode() const
{
    OT_ASSERT(encode_);

    return *encode_;
}

const crypto::Hash& Crypto::Hash() const
{
    OT_ASSERT(hash_);

    return *hash_;
}

void Crypto::Init()
{
    LogDetail(OT_METHOD)(__FUNCTION__)(
        ": Setting up rlimits, and crypto libraries...")
        .Flush();

// Here is a security measure intended to make it more difficult to
// capture a core dump. (Not used in debug mode, obviously.)
//
#if !defined(PREDEF_MODE_DEBUG) && defined(PREDEF_PLATFORM_UNIX)
    struct rlimit rlim;
    getrlimit(RLIMIT_CORE, &rlim);
    rlim.rlim_max = rlim.rlim_cur = 0;
    if (setrlimit(RLIMIT_CORE, &rlim)) {
        OT_FAIL_MSG("Crypto::Init: ASSERT: setrlimit failed. (Used for "
                    "preventing core dumps.)\n");
    }
#endif

#if OT_CRYPTO_USING_LIBSECP256K1
    secp256k1_p_->Init();
#endif
}

#if OT_CRYPTO_SUPPORTED_KEY_RSA
const opentxs::crypto::AsymmetricProvider& Crypto::RSA() const
{
    OT_ASSERT(nullptr != ssl_);

    return *ssl_;
}
#endif

#if OT_CRYPTO_SUPPORTED_KEY_SECP256K1
const opentxs::crypto::EcdsaProvider& Crypto::SECP256K1() const
{
    return secp256k1_;
}
#endif

const opentxs::crypto::SymmetricProvider& Crypto::Sodium() const
{
    OT_ASSERT(sodium_);

    return *sodium_;
}

const crypto::Util& Crypto::Util() const { return util_; }

Crypto::~Crypto() { Cleanup(); }
}  // namespace opentxs::api::implementation
