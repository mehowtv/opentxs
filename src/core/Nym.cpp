// Copyright (c) 2018 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "stdafx.hpp"

#include "opentxs/core/Nym.hpp"

#include "opentxs/api/client/Wallet.hpp"
#include "opentxs/api/crypto/Crypto.hpp"
#include "opentxs/api/storage/Storage.hpp"
#include "opentxs/api/Activity.hpp"
#include "opentxs/api/Native.hpp"
#include "opentxs/api/Server.hpp"
#include "opentxs/consensus/ClientContext.hpp"
#include "opentxs/consensus/ServerContext.hpp"
#include "opentxs/contact/ContactData.hpp"
#include "opentxs/core/crypto/Credential.hpp"
#include "opentxs/core/crypto/NymParameters.hpp"
#include "opentxs/core/crypto/OTPassword.hpp"
#include "opentxs/core/crypto/OTPasswordData.hpp"
#include "opentxs/core/crypto/OTSignedFile.hpp"
#include "opentxs/core/util/Assert.hpp"
#include "opentxs/core/util/Common.hpp"
#include "opentxs/core/util/OTFolders.hpp"
#include "opentxs/core/util/Tag.hpp"
#include "opentxs/core/Armored.hpp"
#include "opentxs/core/Contract.hpp"
#include "opentxs/core/Data.hpp"
#include "opentxs/core/Identifier.hpp"
#include "opentxs/core/Item.hpp"
#include "opentxs/core/Ledger.hpp"
#include "opentxs/core/Message.hpp"
#include "opentxs/core/NymIDSource.hpp"
#include "opentxs/core/OTStorage.hpp"
#include "opentxs/core/OTStringXML.hpp"
#include "opentxs/core/OTTransaction.hpp"
#include "opentxs/core/String.hpp"
#include "opentxs/crypto/key/LegacySymmetric.hpp"
#if OT_CRYPTO_WITH_BIP39
#include "opentxs/crypto/Bip39.hpp"
#endif
#include "opentxs/ext/OTPayment.hpp"
#include "opentxs/OT.hpp"

#include <irrxml/irrXML.hpp>
#include <sodium/crypto_box.h>
#include <sys/types.h>

#include <array>
#include <fstream>
#include <functional>
#include <string>

#define NYMFILE_VERSION "1.1"

#define OT_METHOD "opentxs::Nym::"

namespace opentxs
{
Nym::Nym(
    const api::client::Wallet& wallet,
    const String& name,
    const String& filename,
    const Identifier& nymID,
    const proto::CredentialIndexMode mode)
    : version_(NYM_CREATE_VERSION)
    , index_(0)
    , m_lUsageCredits(0)
    , m_bMarkForDeletion(false)
    , alias_(name.Get())
    , revision_(1)
    , mode_(mode)
    , m_strNymfile(filename)
    , m_strVersion(NYMFILE_VERSION)
    , m_strDescription("")
    , m_nymID(Identifier::Factory(nymID))
    , source_(nullptr)
    , contact_data_(nullptr)
    , wallet_(wallet)
    , m_mapCredentialSets()
    , m_mapRevokedSets()
    , m_listRevokedIDs()
    , m_mapInboxHash()
    , m_mapOutboxHash()
    , m_dequeOutpayments()
    , m_setAccounts()
{
}

Nym::Nym(
    const api::client::Wallet& wallet,
    const String& name,
    const String& filename,
    const String& nymID)
    : Nym(wallet, name, filename, Identifier::Factory(nymID))
{
}

Nym::Nym(const api::client::Wallet& wallet, const Identifier& nymID)
    : Nym(wallet, String(), String(), nymID)
{
}

Nym::Nym(const api::client::Wallet& wallet, const String& strNymID)
    : Nym(wallet, Identifier::Factory(std::string(strNymID.Get())))
{
}

Nym::Nym(const api::client::Wallet& wallet, const NymParameters& nymParameters)
    : Nym(wallet,
          String(),
          String(),
          Identifier::Factory(),
          proto::CREDINDEX_PRIVATE)
{
    NymParameters revisedParameters = nymParameters;
#if OT_CRYPTO_SUPPORTED_KEY_HD
    revisedParameters.SetCredset(index_++);
    std::uint32_t nymIndex = 0;
    std::string fingerprint = nymParameters.Seed();
    auto seed = OT::App().Crypto().BIP39().Seed(fingerprint, nymIndex);

    OT_ASSERT(seed);

    const bool defaultIndex = nymParameters.UseAutoIndex();

    if (!defaultIndex) {
        otErr << __FUNCTION__ << ": Re-creating nym at specified path. "
              << std::endl;

        nymIndex = nymParameters.Nym();
    }

    const std::int32_t newIndex = nymIndex + 1;

    OT::App().Crypto().BIP39().UpdateIndex(fingerprint, newIndex);
    revisedParameters.SetEntropy(*seed);
    revisedParameters.SetSeed(fingerprint);
    revisedParameters.SetNym(nymIndex);
#endif
    CredentialSet* pNewCredentialSet =
        new CredentialSet(wallet_, revisedParameters, version_);

    OT_ASSERT(nullptr != pNewCredentialSet);

    source_ = std::make_shared<NymIDSource>(pNewCredentialSet->Source());
    const_cast<OTIdentifier&>(m_nymID) = source_->NymID();

    SetDescription(source_->Description());

    m_mapCredentialSets.insert(std::pair<std::string, CredentialSet*>(
        pNewCredentialSet->GetMasterCredID(), pNewCredentialSet));

    SaveSignedNymfile(*this);
}

bool Nym::add_contact_credential(
    const eLock& lock,
    const proto::ContactData& data)
{
    OT_ASSERT(verify_lock(lock));

    bool added = false;

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            if (it.second->hasCapability(NymCapability::SIGN_CHILDCRED)) {
                added = it.second->AddContactCredential(data);

                break;
            }
        }
    }

    return added;
}

bool Nym::add_verification_credential(
    const eLock& lock,
    const proto::VerificationSet& data)
{
    OT_ASSERT(verify_lock(lock));

    bool added = false;

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            if (it.second->hasCapability(NymCapability::SIGN_CHILDCRED)) {
                added = it.second->AddVerificationCredential(data);

                break;
            }
        }
    }

    return added;
}

std::string Nym::AddChildKeyCredential(
    const Identifier& masterID,
    const NymParameters& nymParameters)
{
    eLock lock(shared_lock_);

    std::string output;
    std::string master = masterID.str();
    auto it = m_mapCredentialSets.find(master);
    const bool noMaster = (it == m_mapCredentialSets.end());

    if (noMaster) {
        otErr << __FUNCTION__ << ": master ID not found." << std::endl;

        return output;
    }

    if (it->second) {
        output = it->second->AddChildKeyCredential(nymParameters);
    }

    return output;
}

bool Nym::AddClaim(const Claim& claim)
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(contact_data_->AddItem(claim)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::AddContract(
    const Identifier& instrumentDefinitionID,
    const proto::ContactItemType currency,
    const bool primary,
    const bool active)
{
    const std::string id(instrumentDefinitionID.str());

    if (id.empty()) { return false; }

    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(
        contact_data_->AddContract(id, currency, primary, active)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::AddEmail(
    const std::string& value,
    const bool primary,
    const bool active)
{
    if (value.empty()) { return false; }

    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(
        new ContactData(contact_data_->AddEmail(value, primary, active)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

/// a payments message is a form of transaction, transported via Nymbox
/// Though the parameter is a reference (forcing you to pass a real object),
/// the Nym DOES take ownership of the object. Therefore it MUST be allocated
/// on the heap, NOT the stack, or you will corrupt memory with this call.
void Nym::AddOutpayments(Message& theMessage)
{
    eLock lock(shared_lock_);

    m_dequeOutpayments.push_front(&theMessage);
}

#if OT_CRYPTO_SUPPORTED_SOURCE_BIP47
bool Nym::AddPaymentCode(
    const class PaymentCode& code,
    const proto::ContactItemType currency,
    const bool primary,
    const bool active)
{
    const auto paymentCode = code.asBase58();

    if (paymentCode.empty()) { return false; }

    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(
        contact_data_->AddPaymentCode(paymentCode, currency, primary, active)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}
#endif

bool Nym::AddPhoneNumber(
    const std::string& value,
    const bool primary,
    const bool active)
{
    if (value.empty()) { return false; }

    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(
        new ContactData(contact_data_->AddPhoneNumber(value, primary, active)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::AddPreferredOTServer(const Identifier& id, const bool primary)
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_)

    contact_data_.reset(
        new ContactData(contact_data_->AddPreferredOTServer(id, primary)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::AddSocialMediaProfile(
    const std::string& value,
    const proto::ContactItemType type,
    const bool primary,
    const bool active)
{
    if (value.empty()) { return false; }

    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(
        contact_data_->AddSocialMediaProfile(value, type, primary, active)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

std::string Nym::Alias() const { return alias_; }

const serializedCredentialIndex Nym::asPublicNym() const
{
    return SerializeCredentialIndex(CREDENTIAL_INDEX_MODE_FULL_CREDS);
}

std::shared_ptr<const proto::Credential> Nym::ChildCredentialContents(
    const std::string& masterID,
    const std::string& childID) const
{
    sLock lock(shared_lock_);

    std::shared_ptr<const proto::Credential> output;
    auto credential = MasterCredential(String(masterID));

    if (nullptr != credential) {
        output = credential->GetChildCredential(String(childID))
                     ->Serialized(AS_PUBLIC, WITH_SIGNATURES);
    }

    return output;
}

std::string Nym::BestEmail() const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->BestEmail();
}

std::string Nym::BestPhoneNumber() const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->BestPhoneNumber();
}

std::string Nym::BestSocialMediaProfile(const proto::ContactItemType type) const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->BestSocialMediaProfile(type);
}

const class ContactData& Nym::Claims() const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return *contact_data_;
}

void Nym::ClearAll()
{
    eLock lock(shared_lock_);

    m_mapInboxHash.clear();
    m_mapOutboxHash.clear();
    m_setAccounts.clear();
    ClearOutpayments();
}

void Nym::ClearCredentials()
{
    eLock lock(shared_lock_);

    clear_credentials(lock);
}

void Nym::clear_credentials(const eLock& lock)
{
    OT_ASSERT(verify_lock(lock));

    m_listRevokedIDs.clear();

    while (!m_mapCredentialSets.empty()) {
        CredentialSet* pCredential = m_mapCredentialSets.begin()->second;
        m_mapCredentialSets.erase(m_mapCredentialSets.begin());
        delete pCredential;
        pCredential = nullptr;
    }

    while (!m_mapRevokedSets.empty()) {
        CredentialSet* pCredential = m_mapRevokedSets.begin()->second;
        m_mapRevokedSets.erase(m_mapRevokedSets.begin());
        delete pCredential;
        pCredential = nullptr;
    }
}

void Nym::ClearOutpayments()
{
    while (GetOutpaymentsCount() > 0) RemoveOutpaymentsByIndex(0, true);
}

bool Nym::CompareID(const Nym& rhs) const
{
    sLock lock(shared_lock_);

    return rhs.CompareID(m_nymID);
}

bool Nym::CompareID(const Identifier& rhs) const
{
    sLock lock(shared_lock_);

    return rhs == m_nymID;
}

std::set<OTIdentifier> Nym::Contracts(
    const proto::ContactItemType currency,
    const bool onlyActive) const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->Contracts(currency, onlyActive);
}

bool Nym::DeleteClaim(const Identifier& id)
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(contact_data_->Delete(id)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::DeserializeNymfile(
    const String& strNym,
    bool& converted,
    String::Map* pMapCredentials,  // pMapCredentials can be
                                   // passed,
    // if you prefer to use a specific
    // set, instead of just loading the
    // actual set from storage (such as
    // during registration, when the
    // credentials have been sent
    // inside a message.)
    String* pstrReason,
    const OTPassword* pImportPassword)
{
    sLock lock(shared_lock_);

    return deserialize_nymfile(
        lock, strNym, converted, pMapCredentials, pstrReason, pImportPassword);
}

template <typename T>
bool Nym::deserialize_nymfile(
    const T& lock,
    const String& strNym,
    bool& converted,
    String::Map* pMapCredentials,  // pMapCredentials can be
                                   // passed,
    // if you prefer to use a specific
    // set, instead of just loading the
    // actual set from storage (such as
    // during registration, when the
    // credentials have been sent
    // inside a message.)
    String* pstrReason,
    const OTPassword* pImportPassword)
{
    OT_ASSERT(verify_lock(lock));

    bool bSuccess = false;
    bool convert = false;
    converted = false;
    //?    ClearAll();  // Since we are loading everything up... (credentials
    // are NOT
    // cleared here. See note in OTPseudonym::ClearAll.)
    OTStringXML strNymXML(strNym);  // todo optimize
    irr::io::IrrXMLReader* xml = irr::io::createIrrXMLReader(strNymXML);
    OT_ASSERT(nullptr != xml);
    std::unique_ptr<irr::io::IrrXMLReader> theCleanup(xml);
    const auto serverMode = OT::App().ServerMode();
    OTIdentifier serverID{Identifier::Factory()};

    if (serverMode) { serverID = OT::App().Server().ID(); }

    // parse the file until end reached
    while (xml && xml->read()) {

        // strings for storing the data that we want to read out of the file
        //
        switch (xml->getNodeType()) {
            case irr::io::EXN_NONE:
            case irr::io::EXN_TEXT:
            case irr::io::EXN_COMMENT:
            case irr::io::EXN_ELEMENT_END:
            case irr::io::EXN_CDATA:
                break;
            case irr::io::EXN_ELEMENT: {
                const String strNodeName = xml->getNodeName();

                if (strNodeName.Compare("nymData")) {
                    m_strVersion = xml->getAttributeValue("version");
                    const String UserNymID = xml->getAttributeValue("nymID");

                    // Server-side only...
                    String strCredits = xml->getAttributeValue("usageCredits");

                    if (strCredits.GetLength() > 0)
                        m_lUsageCredits = strCredits.ToLong();
                    else
                        m_lUsageCredits = 0;  // This is the default anyway, but
                                              // just being safe...

                    if (UserNymID.GetLength())
                        otLog3 << "\nLoading user, version: " << m_strVersion
                               << " NymID:\n"
                               << UserNymID << "\n";
                    bSuccess = true;
                    convert = (String("1.0") == m_strVersion);

                    if (convert) {
                        otErr << __FUNCTION__
                              << ": Converting nymfile with version "
                              << m_strVersion << std::endl;
                    } else {
                        otWarn << __FUNCTION__
                               << ": Not converting nymfile because version is "
                               << m_strVersion << std::endl;
                    }
                } else if (strNodeName.Compare("nymIDSource")) {
                    //                  otLog3 << "Loading nymIDSource...\n");
                    Armored ascDescription =
                        xml->getAttributeValue("Description");  // optional.
                    if (ascDescription.Exists())
                        ascDescription.GetString(
                            m_strDescription,
                            false);  // bLineBreaks=true by default.

                    Armored stringSource;
                    if (!Contract::LoadEncodedTextField(xml, stringSource)) {
                        otErr
                            << "Error in " << __FILE__ << " line " << __LINE__
                            << ": failed loading expected nymIDSource field.\n";
                        return false;  // error condition
                    }
                    serializedNymIDSource source =
                        NymIDSource::ExtractArmoredSource(stringSource);
                    source_.reset(new NymIDSource(*source));
                } else if (strNodeName.Compare("inboxHashItem")) {
                    const String strAccountID =
                        xml->getAttributeValue("accountID");
                    const String strHashValue =
                        xml->getAttributeValue("hashValue");

                    otLog3 << "\nInboxHash is " << strHashValue
                           << " for Account ID: " << strAccountID << "\n";

                    // Make sure now that I've loaded this InboxHash, to add it
                    // to
                    // my
                    // internal map so that it is available for future lookups.
                    //
                    if (strAccountID.Exists() && strHashValue.Exists()) {
                        auto pID = Identifier::Factory(strHashValue);
                        OT_ASSERT(!pID->empty())
                        m_mapInboxHash.emplace(strAccountID.Get(), pID);
                    }
                } else if (strNodeName.Compare("outboxHashItem")) {
                    const String strAccountID =
                        xml->getAttributeValue("accountID");
                    const String strHashValue =
                        xml->getAttributeValue("hashValue");

                    otLog3 << "\nOutboxHash is " << strHashValue
                           << " for Account ID: " << strAccountID << "\n";

                    // Make sure now that I've loaded this OutboxHash, to add it
                    // to
                    // my
                    // internal map so that it is available for future lookups.
                    //
                    if (strAccountID.Exists() && strHashValue.Exists()) {
                        OTIdentifier pID = Identifier::Factory(strHashValue);
                        OT_ASSERT(!pID->empty())
                        m_mapOutboxHash.emplace(strAccountID.Get(), pID);
                    }
                } else if (strNodeName.Compare("MARKED_FOR_DELETION")) {
                    m_bMarkForDeletion = true;
                    otLog3 << "This nym has been MARKED_FOR_DELETION (at some "
                              "point prior.)\n";
                } else if (strNodeName.Compare("ownsAssetAcct")) {
                    String strID = xml->getAttributeValue("ID");

                    if (strID.Exists()) {
                        m_setAccounts.insert(strID.Get());
                        otLog3 << "This nym has an asset account with the ID: "
                               << strID << "\n";
                    } else
                        otLog3
                            << "This nym MISSING asset account ID when loading "
                               "nym record.\n";
                } else if (strNodeName.Compare("outpaymentsMessage")) {
                    Armored armorMail;
                    String strMessage;

                    xml->read();

                    if (irr::io::EXN_TEXT == xml->getNodeType()) {
                        String strNodeData = xml->getNodeData();

                        // Sometimes the XML reads up the data with a prepended
                        // newline.
                        // This screws up my own objects which expect a
                        // consistent
                        // in/out
                        // So I'm checking here for that prepended newline, and
                        // removing it.
                        char cNewline;
                        if (strNodeData.Exists() &&
                            strNodeData.GetLength() > 2 &&
                            strNodeData.At(0, cNewline)) {
                            if ('\n' == cNewline)
                                armorMail.Set(strNodeData.Get() + 1);
                            else
                                armorMail.Set(strNodeData.Get());

                            if (armorMail.GetLength() > 2) {
                                armorMail.GetString(
                                    strMessage,
                                    true);  // linebreaks == true.

                                if (strMessage.GetLength() > 2) {
                                    Message* pMessage = new Message;
                                    OT_ASSERT(nullptr != pMessage);

                                    if (pMessage->LoadContractFromString(
                                            strMessage))
                                        m_dequeOutpayments.push_back(
                                            pMessage);  // takes ownership
                                    else
                                        delete pMessage;
                                }
                            }
                        }  // strNodeData
                    }      // EXN_TEXT
                }          // outpayments message
                else {
                    // unknown element type
                    otErr << "Unknown element type in " << __FUNCTION__ << ": "
                          << xml->getNodeName() << "\n";
                    bSuccess = false;
                }
                break;
            }
            default: {
                otLog5 << "Unknown XML type in " << __FUNCTION__ << ": "
                       << xml->getNodeName() << "\n";
                break;
            }
        }  // switch
    }      // while

    if (converted) { m_strVersion = "1.1"; }

    return bSuccess;
}

void Nym::DisplayStatistics(String& strOutput) const
{
    sLock lock(shared_lock_);
    strOutput.Concatenate("Source for ID:\n%s\n", source_->asString().Get());
    strOutput.Concatenate("Description: %s\n\n", m_strDescription.Get());

    for (auto it : m_mapCredentialSets) {
        auto pCredentialSet = it.second;
        OT_ASSERT(nullptr != pCredentialSet);

        const String strCredType = Credential::CredentialTypeToString(
            pCredentialSet->GetMasterCredential().Type());
        strOutput.Concatenate(
            "%s Master Credential ID: %s \n",
            strCredType.Get(),
            pCredentialSet->GetMasterCredID().c_str());
        const std::size_t nChildCredentialCount =
            pCredentialSet->GetChildCredentialCount();

        if (nChildCredentialCount > 0) {
            for (std::size_t vvv = 0; vvv < nChildCredentialCount; ++vvv) {
                const Credential* pChild =
                    pCredentialSet->GetChildCredentialByIndex(vvv);
                const String strChildCredType =
                    Credential::CredentialTypeToString(pChild->Type());
                const std::string str_childcred_id(
                    pCredentialSet->GetChildCredentialIDByIndex(vvv));

                strOutput.Concatenate(
                    "   %s child credential ID: %s \n",
                    strChildCredType.Get(),
                    str_childcred_id.c_str());
            }
        }
    }
    strOutput.Concatenate("%s", "\n");

    strOutput.Concatenate(
        "==>      Name: %s   %s\n",
        alias_.c_str(),
        m_bMarkForDeletion ? "(MARKED FOR DELETION)" : "");
    strOutput.Concatenate("      Version: %s\n", m_strVersion.Get());
    strOutput.Concatenate(
        "Outpayments count: %" PRI_SIZE "\n", m_dequeOutpayments.size());

    String theStringID(m_nymID);
    strOutput.Concatenate("Nym ID: %s\n", theStringID.Get());
}

std::string Nym::EmailAddresses(bool active) const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->EmailAddresses(active);
}

const std::vector<OTIdentifier> Nym::GetChildCredentialIDs(
    const std::string& masterID) const
{
    sLock lock(shared_lock_);

    std::vector<OTIdentifier> ids;

    auto it = m_mapCredentialSets.find(masterID);
    if (m_mapCredentialSets.end() != it) {
        const CredentialSet* pMaster = it->second;

        OT_ASSERT(nullptr != pMaster);

        auto count = pMaster->GetChildCredentialCount();
        for (size_t i = 0; i < count; ++i) {
            auto pChild = pMaster->GetChildCredentialByIndex(i);

            OT_ASSERT(nullptr != pChild);

            ids.emplace_back(pChild->ID());
        }
    }

    return ids;
}

bool Nym::GetHash(
    const mapOfIdentifiers& the_map,
    const std::string& str_id,
    Identifier& theOutput) const  // client-side
{
    sLock lock(shared_lock_);

    bool bRetVal =
        false;  // default is false: "No, I didn't find a hash for that id."
    theOutput.Release();

    // The Pseudonym has a map of its recent hashes, one for each server
    // (nymbox) or account (inbox/outbox).
    // For Server Bob, with this Pseudonym, I might have hash lkjsd987345lkj.
    // For but Server Alice, I might have hash 98345jkherkjghdf98gy.
    // (Same Nym, but different hash for each server, as well as inbox/outbox
    // hashes for each asset acct.)
    //
    // So let's loop through all the hashes I have, and if the ID on the map
    // passed in
    // matches the [server|acct] ID that was passed in, then return TRUE.
    //
    for (const auto& it : the_map) {
        if (str_id == it.first) {
            // The call has succeeded
            bRetVal = true;
            theOutput.SetString(it.second->str());
            break;
        }
    }

    return bRetVal;
}
void Nym::GetIdentifier(Identifier& theIdentifier) const
{
    sLock lock(shared_lock_);

    theIdentifier.Assign(m_nymID);
}

// sets argument based on internal member
void Nym::GetIdentifier(String& theIdentifier) const
{
    sLock lock(shared_lock_);

    m_nymID->GetString(theIdentifier);
}

bool Nym::GetInboxHash(
    const std::string& acct_id,
    Identifier& theOutput) const  // client-side
{
    return GetHash(m_mapInboxHash, acct_id, theOutput);
}

CredentialSet* Nym::GetMasterCredential(const String& strID)
{
    sLock lock(shared_lock_);
    auto iter = m_mapCredentialSets.find(strID.Get());
    CredentialSet* pCredential = nullptr;

    if (m_mapCredentialSets.end() != iter)  // found it
        pCredential = iter->second;

    return pCredential;
}

const std::vector<OTIdentifier> Nym::GetMasterCredentialIDs() const
{
    sLock lock(shared_lock_);

    std::vector<OTIdentifier> ids;

    for (const auto& it : m_mapCredentialSets) {
        const CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        ids.emplace_back(pCredential->GetMasterCredential().ID());
    }

    return ids;
}

bool Nym::GetOutboxHash(
    const std::string& acct_id,
    Identifier& theOutput) const  // client-side
{
    return GetHash(m_mapOutboxHash, acct_id, theOutput);
}

// Look up a transaction by transaction number and see if it is in the ledger.
// If it is, return a pointer to it, otherwise return nullptr.
Message* Nym::GetOutpaymentsByIndex(std::int32_t nIndex) const
{
    sLock lock(shared_lock_);
    const std::uint32_t uIndex = nIndex;

    // Out of bounds.
    if (m_dequeOutpayments.empty() || (nIndex < 0) ||
        (uIndex >= m_dequeOutpayments.size())) {

        return nullptr;
    }

    return m_dequeOutpayments.at(nIndex);
}

Message* Nym::GetOutpaymentsByTransNum(
    const std::int64_t lTransNum,
    std::unique_ptr<OTPayment>* pReturnPayment /*=nullptr*/,
    std::int32_t* pnReturnIndex /*=nullptr*/) const
{
    if (nullptr != pnReturnIndex) { *pnReturnIndex = -1; }

    const std::int32_t nCount = GetOutpaymentsCount();

    for (std::int32_t nIndex = 0; nIndex < nCount; ++nIndex) {
        Message* pMsg = m_dequeOutpayments.at(nIndex);
        OT_ASSERT(nullptr != pMsg);
        String strPayment;
        std::unique_ptr<OTPayment> payment;
        std::unique_ptr<OTPayment>& pPayment(
            nullptr == pReturnPayment ? payment : *pReturnPayment);

        // There isn't any encrypted envelope this time, since it's my
        // outPayments box.
        //
        if (pMsg->m_ascPayload.Exists() &&
            pMsg->m_ascPayload.GetString(strPayment) && strPayment.Exists()) {
            pPayment.reset(new OTPayment(strPayment));

            // Let's see if it's the cheque we're looking for...
            //
            if (pPayment && pPayment->IsValid()) {
                if (pPayment->SetTempValues()) {
                    if (pPayment->HasTransactionNum(lTransNum)) {

                        if (nullptr != pnReturnIndex) {
                            *pnReturnIndex = nIndex;
                        }

                        return pMsg;
                    }
                }
            }
        }
    }
    return nullptr;
}

/// return the number of payments items available for this Nym.
std::int32_t Nym::GetOutpaymentsCount() const
{
    return static_cast<std::int32_t>(m_dequeOutpayments.size());
}

const crypto::key::Asymmetric& Nym::GetPrivateAuthKey() const
{
    sLock lock(shared_lock_);

    return get_private_auth_key(lock);
}

template <typename T>
const crypto::key::Asymmetric& Nym::get_private_auth_key(const T& lock) const
{
    OT_ASSERT(!m_mapCredentialSets.empty());

    OT_ASSERT(verify_lock(lock));
    const CredentialSet* pCredential = nullptr;

    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPrivateAuthKey(&m_listRevokedIDs);  // success
}

void Nym::GetPrivateCredentials(String& strCredList, String::Map* pmapCredFiles)
    const
{
    Tag tag("nymData");

    tag.add_attribute("version", m_strVersion.Get());

    String strNymID(m_nymID);

    tag.add_attribute("nymID", strNymID.Get());

    SerializeNymIDSource(tag);

    SaveCredentialsToTag(tag, nullptr, pmapCredFiles);

    std::string str_result;
    tag.output(str_result);

    strCredList.Concatenate("%s", str_result.c_str());
}

const crypto::key::Asymmetric& Nym::GetPrivateEncrKey() const
{
    sLock lock(shared_lock_);

    OT_ASSERT(!m_mapCredentialSets.empty());

    const CredentialSet* pCredential = nullptr;

    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPrivateEncrKey(&m_listRevokedIDs);  // success
}

const crypto::key::Asymmetric& Nym::GetPrivateSignKey() const
{
    sLock lock(shared_lock_);

    return get_private_sign_key(lock);
}

template <typename T>
const crypto::key::Asymmetric& Nym::get_private_sign_key(const T& lock) const
{
    OT_ASSERT(!m_mapCredentialSets.empty());

    OT_ASSERT(verify_lock(lock));

    const CredentialSet* pCredential = nullptr;

    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPrivateSignKey(&m_listRevokedIDs);  // success
}

const crypto::key::Asymmetric& Nym::GetPublicAuthKey() const
{
    sLock lock(shared_lock_);

    OT_ASSERT(!m_mapCredentialSets.empty());

    const CredentialSet* pCredential = nullptr;

    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPublicAuthKey(&m_listRevokedIDs);  // success
}

const crypto::key::Asymmetric& Nym::GetPublicEncrKey() const
{
    sLock lock(shared_lock_);

    OT_ASSERT(!m_mapCredentialSets.empty());

    const CredentialSet* pCredential = nullptr;
    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPublicEncrKey(&m_listRevokedIDs);  // success
}

// This is being called by:
// Contract::VerifySignature(const OTPseudonym& theNym, const OTSignature&
// theSignature, OTPasswordData * pPWData=nullptr)
//
// Note: Need to change Contract::VerifySignature so that it checks all of
// these keys when verifying.
//
// OT uses the signature's metadata to narrow down its search for the correct
// public key.
// Return value is the count of public keys found that matched the metadata on
// the signature.
std::int32_t Nym::GetPublicKeysBySignature(
    crypto::key::Keypair::Keys& listOutput,
    const OTSignature& theSignature,
    char cKeyType) const
{
    // Unfortunately, theSignature can only narrow the search down (there may be
    // multiple results.)
    std::int32_t nCount = 0;

    sLock lock(shared_lock_);

    for (const auto& it : m_mapCredentialSets) {
        const CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        const std::int32_t nTempCount = pCredential->GetPublicKeysBySignature(
            listOutput, theSignature, cKeyType);
        nCount += nTempCount;
    }

    return nCount;
}

const crypto::key::Asymmetric& Nym::GetPublicSignKey() const
{
    sLock lock(shared_lock_);

    return get_public_sign_key(lock);
}

template <typename T>
const crypto::key::Asymmetric& Nym::get_public_sign_key(const T& lock) const
{
    OT_ASSERT(!m_mapCredentialSets.empty());

    OT_ASSERT(verify_lock(lock));

    const CredentialSet* pCredential = nullptr;

    for (const auto& it : m_mapCredentialSets) {
        // Todo: If we have some criteria, such as which master or
        // child credential
        // is currently being employed by the user, we'll use that here to
        // skip
        // through this loop until we find the right one. Until then, I'm
        // just
        // going to return the first one that's valid (not null).

        pCredential = it.second;
        if (nullptr != pCredential) break;
    }
    if (nullptr == pCredential) OT_FAIL;

    return pCredential->GetPublicSignKey(&m_listRevokedIDs);  // success
}

CredentialSet* Nym::GetRevokedCredential(const String& strID)
{
    sLock lock(shared_lock_);

    auto iter = m_mapRevokedSets.find(strID.Get());
    CredentialSet* pCredential = nullptr;

    if (m_mapCredentialSets.end() != iter)  // found it
        pCredential = iter->second;

    return pCredential;
}

const std::vector<OTIdentifier> Nym::GetRevokedCredentialIDs() const
{
    sLock lock(shared_lock_);

    std::vector<OTIdentifier> ids;

    for (const auto& it : m_mapRevokedSets) {
        const CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        ids.emplace_back(pCredential->GetMasterCredential().ID());
    }

    return ids;
}

bool Nym::HasCapability(const NymCapability& capability) const
{
    eLock lock(shared_lock_);

    return has_capability(lock, capability);
}

bool Nym::has_capability(const eLock& lock, const NymCapability& capability)
    const
{
    OT_ASSERT(verify_lock(lock));

    for (auto& it : m_mapCredentialSets) {
        OT_ASSERT(nullptr != it.second);

        if (nullptr != it.second) {
            const CredentialSet& credSet = *it.second;

            if (credSet.hasCapability(capability)) { return true; }
        }
    }

    return false;
}

void Nym::init_claims(const eLock& lock) const
{
    OT_ASSERT(verify_lock(lock));

    const std::string nymID = String(m_nymID).Get();
    contact_data_.reset(new class ContactData(
        nymID,
        NYM_CONTACT_DATA_VERSION,
        NYM_CONTACT_DATA_VERSION,
        ContactData::SectionMap()));

    OT_ASSERT(contact_data_);

    std::unique_ptr<proto::ContactData> serialized{nullptr};

    for (auto& it : m_mapCredentialSets) {
        OT_ASSERT(nullptr != it.second);

        const auto& credSet = *it.second;
        credSet.GetContactData(serialized);

        if (serialized) {
            OT_ASSERT(
                proto::Validate(*serialized, VERBOSE, proto::CLAIMS_NORMAL));

            class ContactData claimCred(
                nymID, NYM_CONTACT_DATA_VERSION, *serialized);
            contact_data_.reset(
                new class ContactData(*contact_data_ + claimCred));
            serialized.reset();
        }
    }

    OT_ASSERT(contact_data_)
}

bool Nym::LoadCredentialIndex(const serializedCredentialIndex& index)
{
    eLock lock(shared_lock_);

    return load_credential_index(lock, index);
}

bool Nym::load_credential_index(
    const eLock& lock,
    const serializedCredentialIndex& index)
{
    if (!proto::Validate<proto::CredentialIndex>(index, VERBOSE)) {
        otErr << __FUNCTION__ << ": Unable to load invalid serialized"
              << " credential index." << std::endl;

        return false;
    }

    OT_ASSERT(verify_lock(lock));

    const auto nymID = Identifier::Factory(index.nymid());

    if (m_nymID != nymID) { return false; }

    version_ = index.version();
    index_ = index.index();
    revision_.store(index.revision());
    mode_ = index.mode();
    source_ = std::make_shared<NymIDSource>(index.source());
    proto::KeyMode mode = (proto::CREDINDEX_PRIVATE == mode_)
                              ? proto::KEYMODE_PRIVATE
                              : proto::KEYMODE_PUBLIC;
    contact_data_.reset();
    m_mapCredentialSets.clear();

    for (auto& it : index.activecredentials()) {
        CredentialSet* newSet = new CredentialSet(wallet_, mode, it);

        if (nullptr != newSet) {
            m_mapCredentialSets.emplace(
                std::make_pair(newSet->GetMasterCredID(), newSet));
        }
    }

    m_mapRevokedSets.clear();

    for (auto& it : index.revokedcredentials()) {
        CredentialSet* newSet = new CredentialSet(wallet_, mode, it);

        if (nullptr != newSet) {
            m_mapRevokedSets.emplace(
                std::make_pair(newSet->GetMasterCredID(), newSet));
        }
    }

    return true;
}

// Use this to load the keys for a Nym (whether public or private), and then
// call VerifyPseudonym, and then load the actual Nymfile using
// LoadSignedNymfile.
bool Nym::LoadCredentials(
    bool bLoadPrivate,
    const OTPasswordData* pPWData,
    const OTPassword* pImportPassword)
{
    eLock lock(shared_lock_);

    return load_credentials(lock);
}

bool Nym::load_credentials(
    const eLock& lock,
    bool bLoadPrivate,
    const OTPasswordData* pPWData,
    const OTPassword* pImportPassword)
{
    clear_credentials(lock);

    String strNymID(m_nymID);
    std::shared_ptr<proto::CredentialIndex> index;

    if (OT::App().DB().Load(strNymID.Get(), index)) {
        return load_credential_index(lock, *index);
    } else {
        otErr << __FUNCTION__
              << ": Failed trying to load credential list for nym: " << strNymID
              << std::endl;
    }

    return false;
}

Nym* Nym::LoadPrivateNym(
    const Identifier& NYM_ID,
    bool bChecking,
    const String* pstrName,
    const char* szFuncName,
    const OTPasswordData* pPWData,
    const OTPassword* pImportPassword)
{
    const char* szFunc =
        (nullptr != szFuncName) ? szFuncName : "OTPseudonym::LoadPrivateNym";

    if (NYM_ID.empty()) return nullptr;

    const String strNymID(NYM_ID);

    // If name is empty, construct one way,
    // else construct a different way.
    std::unique_ptr<Nym> pNym;
    pNym.reset(
        ((nullptr == pstrName) || !pstrName->Exists())
            ? (new Nym(OT::App().Wallet(), NYM_ID))
            : (new Nym(OT::App().Wallet(), *pstrName, strNymID, strNymID)));
    OT_ASSERT_MSG(
        nullptr != pNym,
        "OTPseudonym::LoadPrivateNym: Error allocating memory.\n");

    eLock lock(pNym->shared_lock_);

    OTPasswordData thePWData(OT_PW_DISPLAY);
    if (nullptr == pPWData) pPWData = &thePWData;

    bool bLoadedKey =
        pNym->load_credentials(lock, true, pPWData, pImportPassword);

    if (!bLoadedKey) {
        Log::vOutput(
            bChecking ? 1 : 0,
            "%s: %s: (%s: is %s).  Unable to load credentials, "
            "cert and private key for: %s (maybe this nym doesn't exist?)\n",
            __FUNCTION__,
            szFunc,
            "bChecking",
            bChecking ? "true" : "false",
            strNymID.Get());
        // success loading credentials
        // failure verifying pseudonym public key.
    } else if (!pNym->verify_pseudonym(lock)) {
        otErr << __FUNCTION__ << " " << szFunc
              << ": Failure verifying Nym public key: " << strNymID << "\n";
        // success verifying pseudonym public key.
        // failure loading signed nymfile.
    } else if (!pNym->load_signed_nymfile(lock, *pNym)) {  // Unlike with public
                                                           // key,
        // with private key we DO
        // expect nymfile to be
        // here.
        otErr << __FUNCTION__ << " " << szFunc
              << ": Failure calling LoadSignedNymfile: " << strNymID << "\n";
    } else {  // ultimate success.
        if (pNym->has_capability(lock, NymCapability::SIGN_MESSAGE)) {

            return pNym.release();
        }
        otErr << __FUNCTION__ << " " << szFunc << ": Loaded nym: " << strNymID
              << ", but it's not a private nym." << std::endl;
    }

    return nullptr;
}

// This version is run on the server side, and assumes only a Public Key.
// This code reads up the file, discards the bookends, and saves only the
// gibberish itself.
bool Nym::LoadPublicKey()
{
    eLock lock(shared_lock_);

    if (load_credentials(lock) && (m_mapCredentialSets.size() > 0)) {
        return true;
    }
    otInfo << __FUNCTION__ << ": Failure.\n";
    return false;
}

bool Nym::LoadSignedNymfile(const Nym& SIGNER_NYM)
{
    sLock lock(shared_lock_);

    return load_signed_nymfile(lock, SIGNER_NYM);
}

template <typename T>
bool Nym::load_signed_nymfile(const T& lock, const Nym& SIGNER_NYM)
{
    OT_ASSERT(verify_lock(lock));

    // Get the Nym's ID in string form
    String nymID(m_nymID);

    // Create an OTSignedFile object, giving it the filename (the ID) and the
    // local directory ("nyms")
    OTSignedFile theNymfile(OTFolders::Nym(), nymID);

    if (!theNymfile.LoadFile()) {
        otWarn << __FUNCTION__ << ": Failed loading a signed nymfile: " << nymID
               << std::endl;

        return false;
    }

    // We verify:
    //
    // 1. That the file even exists and loads.
    // 2. That the local subdir and filename match the versions inside the file.
    // 3. That the signature matches for the signer nym who was passed in.
    //
    if (!theNymfile.VerifyFile()) {
        otErr << __FUNCTION__ << ": Failed verifying nymfile: " << nymID
              << "\n\n";

        return false;
    }

    const crypto::key::Asymmetric* publicSignKey = nullptr;
    if (SIGNER_NYM.ID() == ID()) {
        publicSignKey = &get_public_sign_key(lock);
    } else {
        publicSignKey = &SIGNER_NYM.GetPublicSignKey();
    }

    OT_ASSERT(nullptr != publicSignKey);

    if (!theNymfile.VerifyWithKey(*publicSignKey)) {
        otErr << __FUNCTION__
              << ": Failed verifying signature on nymfile: " << nymID
              << "\n Signer Nym ID: " << SIGNER_NYM.ID().str() << "\n";

        return false;
    }

    otInfo << "Loaded and verified signed nymfile. Reading from string...\n";

    if (1 > theNymfile.GetFilePayload().GetLength()) {
        const auto lLength = theNymfile.GetFilePayload().GetLength();

        otErr << __FUNCTION__ << ": Bad length (" << lLength
              << ") while loading nymfile: " << nymID << "\n";
    }

    bool converted = false;
    const bool loaded = deserialize_nymfile(
        lock,
        theNymfile.GetFilePayload(),
        converted,
        nullptr,
        nullptr,
        nullptr);

    if (!loaded) { return false; }

    if (converted) {
        // This will ensure that none of the old tags will be present the next
        // time this nym is loaded.
        // Converting a nym more than once is likely to cause sync issues.
        save_signed_nymfile(lock, SIGNER_NYM);
    }

    return true;
}

const CredentialSet* Nym::MasterCredential(const String& strID) const
{
    auto iter = m_mapCredentialSets.find(strID.Get());
    CredentialSet* pCredential = nullptr;

    if (m_mapCredentialSets.end() != iter)  // found it
        pCredential = iter->second;

    return pCredential;
}

std::shared_ptr<const proto::Credential> Nym::MasterCredentialContents(
    const std::string& id) const
{
    eLock lock(shared_lock_);

    std::shared_ptr<const proto::Credential> output;
    auto credential = MasterCredential(String(id));

    if (nullptr != credential) {
        output = credential->GetMasterCredential().Serialized(
            AS_PUBLIC, WITH_SIGNATURES);
    }

    return output;
}

std::string Nym::Name() const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    std::string output = contact_data_->Name();

    if (false == output.empty()) { return output; }

    return alias_;
}

bool Nym::Path(proto::HDPath& output) const
{
    sLock lock(shared_lock_);

    for (const auto& it : m_mapCredentialSets) {
        OT_ASSERT(nullptr != it.second);
        const auto& set = *it.second;

        if (set.Path(output)) {
            output.mutable_child()->RemoveLast();
            return true;
        }
    }

    otErr << OT_METHOD << __FUNCTION__ << ": No credential set contains a path."
          << std::endl;

    return false;
}

std::string Nym::PaymentCode() const
{
#if OT_CRYPTO_SUPPORTED_SOURCE_BIP47
    if (!source_) { return ""; }

    if (proto::SOURCETYPE_BIP47 != source_->Type()) { return ""; }

    auto serialized = source_->Serialize();

    if (!serialized) { return ""; }

    auto paymentCode = PaymentCode::Factory(serialized->paymentcode());

    return paymentCode->asBase58();

#else
    return "";
#endif
}

std::string Nym::PhoneNumbers(bool active) const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->PhoneNumbers(active);
}

// Used when importing/exporting Nym into and out-of the sphere of the
// cached key in the wallet.
bool Nym::ReEncryptPrivateCredentials(
    bool bImporting,  // bImporting=true, or
                      // false if exporting.
    const OTPasswordData* pPWData,
    const OTPassword* pImportPassword) const
{
    eLock lock(shared_lock_);

    const OTPassword* pExportPassphrase = nullptr;
    std::unique_ptr<const OTPassword> thePasswordAngel;

    if (nullptr == pImportPassword) {

        // whether import/export, this display string is for the OUTSIDE OF
        // WALLET
        // portion of that process.
        //
        String strDisplay(
            nullptr != pPWData
                ? pPWData->GetDisplayString()
                : (bImporting ? "Enter passphrase for the Nym being imported."
                              : "Enter passphrase for exported Nym."));
        // Circumvents the cached key.
        pExportPassphrase = crypto::key::LegacySymmetric::GetPassphraseFromUser(
            &strDisplay, !bImporting);  // bAskTwice is true when exporting
                                        // (since the export passphrase is being
                                        // created at that time.)
        thePasswordAngel.reset(pExportPassphrase);

        if (nullptr == pExportPassphrase) {
            otErr << __FUNCTION__ << ": Failed in GetPassphraseFromUser.\n";
            return false;
        }
    } else {
        pExportPassphrase = pImportPassword;
    }

    for (auto& it : m_mapCredentialSets) {
        CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        if (false == pCredential->ReEncryptPrivateCredentials(
                         *pExportPassphrase, bImporting))
            return false;
    }

    return true;
}

// Sometimes for testing I need to clear out all the transaction numbers from a
// nym. So I added this method to make such a thing easy to do.
void Nym::RemoveAllNumbers(const String* pstrNotaryID)
{
    std::list<mapOfIdentifiers::iterator> listOfInboxHash;
    std::list<mapOfIdentifiers::iterator> listOfOutboxHash;

    // This is mapped to acct_id, not notary_id.
    // (So we just wipe them all.)
    for (auto it(m_mapInboxHash.begin()); it != m_mapInboxHash.end(); ++it) {
        listOfInboxHash.push_back(it);
    }

    // This is mapped to acct_id, not notary_id.
    // (So we just wipe them all.)
    for (auto it(m_mapOutboxHash.begin()); it != m_mapOutboxHash.end(); ++it) {
        listOfOutboxHash.push_back(it);
    }

    while (!listOfInboxHash.empty()) {
        m_mapInboxHash.erase(listOfInboxHash.back());
        listOfInboxHash.pop_back();
    }
    while (!listOfOutboxHash.empty()) {
        m_mapOutboxHash.erase(listOfOutboxHash.back());
        listOfOutboxHash.pop_back();
    }
}

// if this function returns false, outpayments index was bad.
bool Nym::RemoveOutpaymentsByIndex(const std::int32_t nIndex, bool bDeleteIt)
{
    const std::uint32_t uIndex = nIndex;

    // Out of bounds.
    if (m_dequeOutpayments.empty() || (nIndex < 0) ||
        (uIndex >= m_dequeOutpayments.size())) {
        otErr << __FUNCTION__
              << ": Error: Index out of bounds: signed: " << nIndex
              << " unsigned: " << uIndex << " (deque size is "
              << m_dequeOutpayments.size() << ").\n";
        return false;
    }

    Message* pMessage = m_dequeOutpayments.at(nIndex);
    OT_ASSERT(nullptr != pMessage);

    m_dequeOutpayments.erase(m_dequeOutpayments.begin() + uIndex);

    if (bDeleteIt) delete pMessage;

    return true;
}

bool Nym::RemoveOutpaymentsByTransNum(
    const std::int64_t lTransNum,
    bool bDeleteIt /*=true*/)
{
    std::int32_t nReturnIndex = -1;

    Message* pMsg =
        this->GetOutpaymentsByTransNum(lTransNum, nullptr, &nReturnIndex);
    const std::uint32_t uIndex = nReturnIndex;

    if ((nullptr != pMsg) && (nReturnIndex > (-1)) &&
        (uIndex < m_dequeOutpayments.size())) {
        m_dequeOutpayments.erase(m_dequeOutpayments.begin() + uIndex);
        if (bDeleteIt) delete pMsg;
        return true;
    }
    return false;
}

// ** ResyncWithServer **
//
// Not for normal use! (Since you should never get out of sync with the server
// in the first place.)
// However, in testing, or if some bug messes up some data, or whatever, and you
// absolutely need to
// re-sync with a server, and you trust that server not to lie to you, then this
// function will do the trick.
// NOTE: Before calling this, you need to do a getNymbox() to download the
// latest Nymbox, and you need to do
// a registerNym() to download the server's copy of your Nym. You then
// need to load that Nymbox from
// local storage, and you need to load the server's message Nym out of the
// registerNymResponse reply, so that
// you can pass both of those objects into this function, which must assume that
// those pieces were already done
// just prior to this call.
//
bool Nym::ResyncWithServer(const Ledger& theNymbox, const Nym& theMessageNym)
{
    eLock lock(shared_lock_);

    bool bSuccess = true;
    const Identifier& theNotaryID = theNymbox.GetRealNotaryID();
    const String strNotaryID(theNotaryID);
    const String strNymID(m_nymID);

    auto context =
        OT::App().Wallet().mutable_ServerContext(m_nymID, theNotaryID);

    // Remove all issued, transaction, and tentative numbers for a specific
    // server ID,
    // as well as all acknowledgedNums, and the highest transaction number for
    // that notaryID,
    // from *this nym. Leave our record of the highest trans num received from
    // that server,
    // since we will want to just keep it when re-syncing. (Server doesn't store
    // that anyway.)
    //
    RemoveAllNumbers(&strNotaryID);

    std::set<TransactionNumber> setTransNumbers;

    // loop through theNymbox and add Tentative numbers to *this based on each
    // successNotice in the Nymbox. This way, when the notices are processed,
    // they will succeed because the Nym will believe he was expecting them.
    for (auto& it : theNymbox.GetTransactionMap()) {
        OTTransaction* pTransaction = it.second;
        OT_ASSERT(nullptr != pTransaction);
        //        OTString strTransaction(*pTransaction);
        //        otErr << "TRANSACTION CONTENTS:\n%s\n", strTransaction.Get());

        // (a new; ALREADY just added transaction number.)
        if ((OTTransaction::successNotice !=
             pTransaction->GetType()))  // if !successNotice
            continue;

        const std::int64_t lNum =
            pTransaction->GetReferenceToNum();  // successNotice is inRefTo the
                                                // new transaction # that should
                                                // be on my tentative list.

        // Add to list of tentatively-being-added numbers.
        if (!context.It().AddTentativeNumber(lNum)) {
            otErr << "OTPseudonym::ResyncWithServer: Failed trying to add "
                     "TentativeNum ("
                  << lNum << ") onto *this nym: " << strNymID
                  << ", for server: " << strNotaryID << "\n";
            bSuccess = false;
        } else
            otWarn << "OTPseudonym::ResyncWithServer: Added TentativeNum ("
                   << lNum << ") onto *this nym: " << strNymID
                   << ", for server: " << strNotaryID << " \n";
        // There's no "else insert to setTransNumbers" here, like the other two
        // blocks above.
        // Why not? Because setTransNumbers is for updating the Highest Trans
        // Num record on this Nym,
        // and the Tentative Numbers aren't figured into that record until AFTER
        // they are accepted
        // from the Nymbox. So I leave them out, since this function is
        // basically setting us up to
        // successfully process the Nymbox, which will then naturally update the
        // highest num record
        // based on the tentatives, as it's removing them from the tentative
        // list and adding them to
        // the "available" transaction list (and issued.)
    }

    std::set<TransactionNumber> notUsed;
    context.It().UpdateHighest(setTransNumbers, notUsed, notUsed);

    return (save_signed_nymfile(lock, *this) && bSuccess);
}

std::uint64_t Nym::Revision() const { return revision_.load(); }

void Nym::revoke_contact_credentials(const eLock& lock)
{
    OT_ASSERT(verify_lock(lock));

    std::list<std::string> revokedIDs;

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            it.second->RevokeContactCredentials(revokedIDs);
        }
    }

    for (auto& it : revokedIDs) { m_listRevokedIDs.push_back(it); }
}

void Nym::revoke_verification_credentials(const eLock& lock)
{
    OT_ASSERT(verify_lock(lock));

    std::list<std::string> revokedIDs;

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            it.second->RevokeVerificationCredentials(revokedIDs);
        }
    }

    for (auto& it : revokedIDs) { m_listRevokedIDs.push_back(it); }
}

std::shared_ptr<const proto::Credential> Nym::RevokedCredentialContents(
    const std::string& id) const
{
    eLock lock(shared_lock_);

    std::shared_ptr<const proto::Credential> output;

    auto iter = m_mapRevokedSets.find(id);

    if (m_mapRevokedSets.end() != iter) {
        output = iter->second->GetMasterCredential().Serialized(
            AS_PUBLIC, WITH_SIGNATURES);
    }

    return output;
}

void Nym::SaveCredentialsToTag(
    Tag& parent,
    String::Map* pmapPubInfo,
    String::Map* pmapPriInfo) const
{
    // IDs for revoked child credentials are saved here.
    for (auto& it : m_listRevokedIDs) {
        std::string str_revoked_id = it;
        TagPtr pTag(new Tag("revokedCredential"));
        pTag->add_attribute("ID", str_revoked_id);
        parent.add_tag(pTag);
    }

    // Serialize master and sub-credentials here.
    for (auto& it : m_mapCredentialSets) {
        CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        pCredential->SerializeIDs(
            parent,
            m_listRevokedIDs,
            pmapPubInfo,
            pmapPriInfo,
            true);  // bShowRevoked=false by default (true here), bValid=true
    }

    // Serialize Revoked master credentials here, including their child key
    // credentials.
    for (auto& it : m_mapRevokedSets) {
        CredentialSet* pCredential = it.second;
        OT_ASSERT(nullptr != pCredential);

        pCredential->SerializeIDs(
            parent,
            m_listRevokedIDs,
            pmapPubInfo,
            pmapPriInfo,
            true,
            false);  // bShowRevoked=false by default. (Here it's true.)
                     // bValid=true by default. Here is for revoked, so false.
    }
}

// Save the Pseudonym to a string...
bool Nym::SerializeNymfile(String& strNym) const
{
    sLock lock(shared_lock_);

    return serialize_nymfile(lock, strNym);
}

template <typename T>
bool Nym::serialize_nymfile(const T& lock, String& strNym) const
{
    OT_ASSERT(verify_lock(lock));

    Tag tag("nymData");

    String nymID(m_nymID);

    tag.add_attribute("version", m_strVersion.Get());
    tag.add_attribute("nymID", nymID.Get());

    if (m_lUsageCredits != 0)
        tag.add_attribute("usageCredits", formatLong(m_lUsageCredits));

    SerializeNymIDSource(tag);

    // When you delete a Nym, it just marks it.
    // Actual deletion occurs during maintenance sweep
    // (targeting marked nyms...)
    //
    if (m_bMarkForDeletion) {
        tag.add_tag(
            "MARKED_FOR_DELETION",
            "THIS NYM HAS BEEN MARKED "
            "FOR DELETION AT ITS OWN REQUEST");
    }

    if (!(m_dequeOutpayments.empty())) {
        for (std::uint32_t i = 0; i < m_dequeOutpayments.size(); i++) {
            Message* pMessage = m_dequeOutpayments.at(i);
            OT_ASSERT(nullptr != pMessage);

            String strOutpayments(*pMessage);

            Armored ascOutpayments;

            if (strOutpayments.Exists())
                ascOutpayments.SetString(strOutpayments);

            if (ascOutpayments.Exists()) {
                tag.add_tag("outpaymentsMessage", ascOutpayments.Get());
            }
        }
    }

    // These are used on the server side.
    // (That's why you don't see the server ID saved here.)
    //
    if (!(m_setAccounts.empty())) {
        for (auto& it : m_setAccounts) {
            std::string strID(it);
            TagPtr pTag(new Tag("ownsAssetAcct"));
            pTag->add_attribute("ID", strID);
            tag.add_tag(pTag);
        }
    }

    // client-side
    for (auto& it : m_mapInboxHash) {
        std::string strAcctID = it.first;
        const Identifier& theID = it.second;

        if ((strAcctID.size() > 0) && !theID.empty()) {
            const String strHash(theID);
            TagPtr pTag(new Tag("inboxHashItem"));
            pTag->add_attribute("accountID", strAcctID);
            pTag->add_attribute("hashValue", strHash.Get());
            tag.add_tag(pTag);
        }
    }  // for

    // client-side
    for (auto& it : m_mapOutboxHash) {
        std::string strAcctID = it.first;
        const Identifier& theID = it.second;

        if ((strAcctID.size() > 0) && !theID.empty()) {
            const String strHash(theID);
            TagPtr pTag(new Tag("outboxHashItem"));
            pTag->add_attribute("accountID", strAcctID);
            pTag->add_attribute("hashValue", strHash.Get());
            tag.add_tag(pTag);
        }
    }  // for

    std::string str_result;
    tag.output(str_result);

    strNym.Concatenate("%s", str_result.c_str());

    return true;
}

bool Nym::SerializeNymfile(const char* szFoldername, const char* szFilename)
{
    OT_ASSERT(nullptr != szFoldername);
    OT_ASSERT(nullptr != szFilename);

    sLock lock(shared_lock_);

    String strNym;
    serialize_nymfile(lock, strNym);

    bool bSaved =
        OTDB::StorePlainString(strNym.Get(), szFoldername, szFilename);
    if (!bSaved)
        otErr << __FUNCTION__ << ": Error saving file: " << szFoldername
              << Log::PathSeparator() << szFilename << "\n";

    return bSaved;
}

bool Nym::SavePseudonymWallet(Tag& parent) const
{
    sLock lock(shared_lock_);

    String nymID(m_nymID);

    // Name is in the clear in memory,
    // and base64 in storage.
    Armored ascName;
    if (!alias_.empty()) {
        ascName.SetString(String(alias_), false);  // linebreaks == false
    }

    TagPtr pTag(new Tag("pseudonym"));

    pTag->add_attribute("name", !alias_.empty() ? ascName.Get() : "");
    pTag->add_attribute("nymID", nymID.Get());

    parent.add_tag(pTag);

    return true;
}

bool Nym::SaveSignedNymfile(const Nym& SIGNER_NYM)
{
    eLock lock(shared_lock_);

    return save_signed_nymfile(lock, SIGNER_NYM);
}

template <typename T>
bool Nym::save_signed_nymfile(const T& lock, const Nym& SIGNER_NYM)
{
    OT_ASSERT(verify_lock(lock));

    // Get the Nym's ID in string form
    String strNymID(m_nymID);

    // Create an OTSignedFile object, giving it the filename (the ID) and the
    // local directory ("nyms")
    OTSignedFile theNymfile(OTFolders::Nym().Get(), strNymID);
    theNymfile.GetFilename(m_strNymfile);

    otInfo << "Saving nym to: " << m_strNymfile << "\n";

    // First we save this nym to a string...
    // Specifically, the file payload string on the OTSignedFile object.
    serialize_nymfile(lock, theNymfile.GetFilePayload());

    // Now the OTSignedFile contains the path, the filename, AND the
    // contents of the Nym itself, saved to a string inside the OTSignedFile
    // object.

    const crypto::key::Asymmetric* privateSignKey = nullptr;
    if (SIGNER_NYM.ID() == ID()) {
        privateSignKey = &get_private_sign_key(lock);
    } else {
        privateSignKey = &SIGNER_NYM.GetPrivateSignKey();
    }

    OT_ASSERT(nullptr != privateSignKey);

    if (theNymfile.SignWithKey(*privateSignKey) && theNymfile.SaveContract()) {
        const bool bSaved = theNymfile.SaveFile();

        if (!bSaved) {
            otErr << __FUNCTION__
                  << ": Failed while calling theNymfile.SaveFile() for Nym "
                  << strNymID << " using Signer Nym " << SIGNER_NYM.ID().str()
                  << "\n";
        }

        return bSaved;
    } else {
        otErr << __FUNCTION__
              << ": Failed trying to sign and save Nymfile for Nym " << strNymID
              << " using Signer Nym " << SIGNER_NYM.ID().str() << "\n";
    }

    return false;
}

serializedCredentialIndex Nym::SerializeCredentialIndex(
    const CredentialIndexModeFlag mode) const
{
    sLock lock(shared_lock_);

    serializedCredentialIndex index;

    index.set_version(version_);
    String nymID(m_nymID);
    index.set_nymid(nymID.Get());

    if (CREDENTIAL_INDEX_MODE_ONLY_IDS == mode) {
        index.set_mode(mode_);

        if (proto::CREDINDEX_PRIVATE == mode_) { index.set_index(index_); }
    } else {
        index.set_mode(proto::CREDINDEX_PUBLIC);
    }

    index.set_revision(revision_.load());
    *(index.mutable_source()) = *(source_->Serialize());

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            SerializedCredentialSet credset = it.second->Serialize(mode);
            auto pCredSet = index.add_activecredentials();
            *pCredSet = *credset;
            pCredSet = nullptr;
        }
    }

    for (auto& it : m_mapRevokedSets) {
        if (nullptr != it.second) {
            SerializedCredentialSet credset = it.second->Serialize(mode);
            auto pCredSet = index.add_revokedcredentials();
            *pCredSet = *credset;
            pCredSet = nullptr;
        }
    }

    return index;
}

void Nym::SerializeNymIDSource(Tag& parent) const
{
    // We encode these before storing.
    if (source_) {

        TagPtr pTag(new Tag("nymIDSource", source_->asString().Get()));

        if (m_strDescription.Exists()) {
            Armored ascDescription;
            ascDescription.SetString(
                m_strDescription,
                false);  // bLineBreaks=true by default.

            pTag->add_attribute("Description", ascDescription.Get());
        }
        parent.add_tag(pTag);
    }
}

bool Nym::set_contact_data(const eLock& lock, const proto::ContactData& data)
{
    OT_ASSERT(verify_lock(lock));

    auto version = proto::NymRequiredVersion(data.version(), version_);

    if (!version || version > NYM_UPGRADE_VERSION) {
        otErr << OT_METHOD << __FUNCTION__
              << ": Contact data version not supported by this nym."
              << std::endl;
        return false;
    }

    if (false == has_capability(lock, NymCapability::SIGN_CHILDCRED)) {
        otErr << OT_METHOD << __FUNCTION__ << ": This nym can not be modified."
              << std::endl;

        return false;
    }

    if (false == proto::Validate(data, VERBOSE, false)) {
        otErr << OT_METHOD << __FUNCTION__ << ": Invalid contact data."
              << std::endl;

        return false;
    }

    revoke_contact_credentials(lock);

    if (add_contact_credential(lock, data)) {

        return update_nym(lock, version);
    }

    return false;
}

void Nym::SetAlias(const std::string& alias)
{
    eLock lock(shared_lock_);

    alias_ = alias;
    revision_++;
}

bool Nym::SetCommonName(const std::string& name)
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    contact_data_.reset(new ContactData(contact_data_->SetCommonName(name)));

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::SetContactData(const proto::ContactData& data)
{
    eLock lock(shared_lock_);

    contact_data_.reset(
        new ContactData(m_nymID->str(), NYM_CONTACT_DATA_VERSION, data));

    return set_contact_data(lock, data);
}

bool Nym::SetHash(
    mapOfIdentifiers& the_map,
    const std::string& str_id,
    const Identifier& theInput)  // client-side
{
    bool bSuccess = false;

    auto find_it = the_map.find(str_id);

    if (the_map.end() != find_it)  // found something for that str_id
    {
        // The call has succeeded
        the_map.erase(find_it);
        OTIdentifier pID = Identifier::Factory(theInput);
        OT_ASSERT(!pID->empty())
        the_map.emplace(str_id, pID);
        bSuccess = true;
    }

    // If I didn't find it in the list above (whether the list is empty or
    // not....)
    // that means it does not exist. (So create it.)
    //
    if (!bSuccess) {
        OTIdentifier pID = Identifier::Factory(theInput);
        OT_ASSERT(!pID->empty())
        the_map.emplace(str_id, pID);
    }
    //    if (bSuccess)
    //    {
    //        SaveSignedNymfile(SIGNER_NYM);
    //    }

    return bSuccess;
}

bool Nym::SetInboxHash(
    const std::string& acct_id,
    const Identifier& theInput)  // client-side
{
    eLock lock(shared_lock_);

    return SetHash(m_mapInboxHash, acct_id, theInput);
}

bool Nym::SetOutboxHash(
    const std::string& acct_id,
    const Identifier& theInput)  // client-side
{
    eLock lock(shared_lock_);

    return SetHash(m_mapOutboxHash, acct_id, theInput);
}

bool Nym::SetScope(
    const proto::ContactItemType type,
    const std::string& name,
    const bool primary)
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    if (proto::CITEMTYPE_UNKNOWN != contact_data_->Type()) {
        contact_data_.reset(
            new ContactData(contact_data_->SetName(name, primary)));
    } else {
        contact_data_.reset(
            new ContactData(contact_data_->SetScope(type, name)));
    }

    OT_ASSERT(contact_data_);

    return set_contact_data(lock, contact_data_->Serialize());
}

bool Nym::SetVerificationSet(const proto::VerificationSet& data)
{
    eLock lock(shared_lock_);

    if (false == has_capability(lock, NymCapability::SIGN_CHILDCRED)) {
        otErr << OT_METHOD << __FUNCTION__ << ": This nym can not be modified."
              << std::endl;

        return false;
    }

    revoke_verification_credentials(lock);

    if (add_verification_credential(lock, data)) {

        return update_nym(lock, version_);
    }

    return false;
}

std::string Nym::SocialMediaProfiles(
    const proto::ContactItemType type,
    bool active) const
{
    eLock lock(shared_lock_);

    if (false == bool(contact_data_)) { init_claims(lock); }

    OT_ASSERT(contact_data_);

    return contact_data_->SocialMediaProfiles(type, active);
}

const std::set<proto::ContactItemType> Nym::SocialMediaProfileTypes() const
{
    sLock lock(shared_lock_);

    return contact_data_->SocialMediaProfileTypes();
}

std::unique_ptr<OTPassword> Nym::TransportKey(Data& pubkey) const
{
    bool found{false};
    auto privateKey = std::make_unique<OTPassword>();

    OT_ASSERT(privateKey);

    sLock lock(shared_lock_);

    for (auto& it : m_mapCredentialSets) {
        OT_ASSERT(nullptr != it.second);

        if (nullptr != it.second) {
            const CredentialSet& credSet = *it.second;
            found = credSet.TransportKey(pubkey, *privateKey);

            if (found) { break; }
        }
    }

    OT_ASSERT(found);

    return privateKey;
}

bool Nym::update_nym(const eLock& lock, const std::int32_t version)
{
    OT_ASSERT(verify_lock(lock));

    if (verify_pseudonym(lock)) {
        // Upgrade version
        if (version > version_) { version_ = version; }

        ++revision_;

        return true;
    }

    return false;
}

std::unique_ptr<proto::VerificationSet> Nym::VerificationSet() const
{
    std::unique_ptr<proto::VerificationSet> verificationSet;

    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            it.second->GetVerificationSet(verificationSet);
        }
    }

    return verificationSet;
}

bool Nym::Verify(const Data& plaintext, const proto::Signature& sig) const
{
    for (auto& it : m_mapCredentialSets) {
        if (nullptr != it.second) {
            if (it.second->Verify(plaintext, sig)) { return true; }
        }
    }

    return false;
}

bool Nym::VerifyPseudonym() const
{
    eLock lock(shared_lock_);

    return verify_pseudonym(lock);
}

bool Nym::verify_pseudonym(const eLock& lock) const
{
    // If there are credentials, then we verify the Nym via his credentials.
    if (!m_mapCredentialSets.empty()) {
        // Verify Nym by his own credentials.
        for (const auto& it : m_mapCredentialSets) {
            const CredentialSet* pCredential = it.second;
            OT_ASSERT(nullptr != pCredential);

            const OTIdentifier theCredentialNymID =
                Identifier::Factory(pCredential->GetNymID());
            if (m_nymID != theCredentialNymID) {
                otOut << __FUNCTION__ << ": Credential NymID ("
                      << pCredential->GetNymID()
                      << ") doesn't match actual NymID: " << m_nymID->str()
                      << "\n";
                return false;
            }

            // Verify all Credentials in the CredentialSet, including source
            // verification for the master credential.
            if (!pCredential->VerifyInternally()) {
                otOut << __FUNCTION__ << ": Credential ("
                      << pCredential->GetMasterCredID()
                      << ") failed its own internal verification." << std::endl;
                return false;
            }
        }
        return true;
    }
    otErr << "No credentials.\n";
    return false;
}

bool Nym::WriteCredentials() const
{
    sLock lock(shared_lock_);

    for (auto& it : m_mapCredentialSets) {
        if (!it.second->WriteCredentials()) {
            otErr << __FUNCTION__ << ": Failed to save credentials."
                  << std::endl;

            return false;
        }
    }

    return true;
}

Nym::~Nym()
{
    ClearAll();
    ClearCredentials();
}
}  // namespace opentxs
