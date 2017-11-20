/************************************************************
 *
 *                 OPEN TRANSACTIONS
 *
 *       Financial Cryptography and Digital Cash
 *       Library, Protocol, API, Server, CLI, GUI
 *
 *       -- Anonymous Numbered Accounts.
 *       -- Untraceable Digital Cash.
 *       -- Triple-Signed Receipts.
 *       -- Cheques, Vouchers, Transfers, Inboxes.
 *       -- Basket Currencies, Markets, Payment Plans.
 *       -- Signed, XML, Ricardian-style Contracts.
 *       -- Scripted smart contracts.
 *
 *  EMAIL:
 *  fellowtraveler@opentransactions.org
 *
 *  WEBSITE:
 *  http://www.opentransactions.org/
 *
 *  -----------------------------------------------------
 *
 *   LICENSE:
 *   This Source Code Form is subject to the terms of the
 *   Mozilla Public License, v. 2.0. If a copy of the MPL
 *   was not distributed with this file, You can obtain one
 *   at http://mozilla.org/MPL/2.0/.
 *
 *   DISCLAIMER:
 *   This program is distributed in the hope that it will
 *   be useful, but WITHOUT ANY WARRANTY; without even the
 *   implied warranty of MERCHANTABILITY or FITNESS FOR A
 *   PARTICULAR PURPOSE.  See the Mozilla Public License
 *   for more details.
 *
 ************************************************************/

#include "opentxs/stdafx.hpp"

#include "opentxs/client/OTAPI_Func.hpp"

#include "opentxs/client/OT_ME.hpp"
#include "opentxs/client/SwigWrap.hpp"
#include "opentxs/client/Utility.hpp"
#include "opentxs/consensus/ServerContext.hpp"
#include "opentxs/core/script/OTVariable.hpp"
#include "opentxs/core/util/Common.hpp"
#include "opentxs/core/Log.hpp"
#include "opentxs/core/Nym.hpp"
#include "opentxs/core/OTStorage.hpp"

#include <stdint.h>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace opentxs
{
OTAPI_Func::OTAPI_Func(
    opentxs::ServerContext& context,
    OT_API& otapi,
    const OTAPI_Func_Type type)
    : funcType(type)
    , nymID2("")
    , instrumentDefinitionID("")
    , instrumentDefinitionID2("")
    , accountID("")
    , accountID2("")
    , basket("")
    , strData("")
    , strData2("")
    , strData3("")
    , strData4("")
    , strData5("")
    , bBool(false)
    , nData(0)
    , lData(0)
    , tData(OT_TIME_ZERO)
    , nTransNumsNeeded(0)
    , nRequestNum(-1)
    , context_(context)
    , otapi_(otapi)
{
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi)
    : OTAPI_Func(context, otapi, theType)
{
    if (theType == DELETE_NYM) {
        nTransNumsNeeded = 0;            // Is this true?
    } else if (theType == REGISTER_NYM)  // FYI.
    {
        nTransNumsNeeded = 0;
    } else if (theType == GET_MARKET_LIST)  // FYI
    {
        nTransNumsNeeded = 0;
    } else if (theType == GET_NYM_MARKET_OFFERS)  // FYI
    {
        nTransNumsNeeded = 0;
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_strParam)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    switch (theType) {
        case (ISSUE_BASKET): {
            basket = p_strParam;
        } break;
        case (CREATE_ASSET_ACCT): {
            nTransNumsNeeded = 1;  // So it's done at least one "transaction
                                   // statement" before it can ever processInbox
                                   // on an account.
            instrumentDefinitionID = p_strParam;
        } break;
        case (GET_MINT):
        case (GET_CONTRACT):
        case (REGISTER_CONTRACT_UNIT): {
            instrumentDefinitionID = p_strParam;
        } break;
        case (CHECK_NYM):
        case (REGISTER_CONTRACT_NYM): {
            nymID2 = p_strParam;
        } break;
        case (DELETE_ASSET_ACCT): {
            accountID = p_strParam;
        } break;
        case (ISSUE_ASSET_TYPE):
        case (GET_MARKET_RECENT_TRADES):
        case (REGISTER_CONTRACT_SERVER):
        case (REQUEST_ADMIN): {
            strData = p_strParam;
        } break;
        default: {
            otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func()"
                  << std::endl;
        }
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_strParam,
    const std::string& p_strData)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    if (!VerifyStringVal(p_strData)) {
        otErr << strError << "p_strData" << std::endl;
    }

    funcType = theType;
    nTransNumsNeeded = 1;
    bBool = false;

    if ((theType == KILL_MARKET_OFFER) || (theType == KILL_PAYMENT_PLAN) ||
        (theType == PROCESS_INBOX) || (theType == DEPOSIT_CASH) ||
        (theType == DEPOSIT_CHEQUE) || (theType == DEPOSIT_PAYMENT_PLAN)) {
        accountID = p_strParam;
        strData = p_strData;
    } else if (theType == ADJUST_USAGE_CREDITS) {
        nTransNumsNeeded = 0;
        nymID2 = p_strParam;  // target nym ID
        strData = p_strData;  // adjustment (up or down.)
    } else if (theType == EXCHANGE_CASH) {
        instrumentDefinitionID = p_strParam;
        strData = p_strData;
    } else if (theType == INITIATE_BAILMENT) {
        nTransNumsNeeded = 0;
        nymID2 = p_strParam;
        instrumentDefinitionID = p_strData;
        strData = SwigWrap::initiateBailment(
            String(context_.Server()).Get(),
            String(context_.Nym()->ID()).Get(),
            instrumentDefinitionID);
    } else {
        otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func()\n";
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_strParam,
    std::int64_t p_lData)
    : OTAPI_Func(context, otapi, theType)
{
    lData = p_lData;

    const std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    switch (theType) {
        case (WITHDRAW_CASH): {
            nTransNumsNeeded = 1;
            accountID = p_strParam;
        } break;
        case (GET_MARKET_OFFERS): {
            strData = p_strParam;
        } break;
        case (REQUEST_CONNECTION): {
            nymID2 = p_strParam;
            strData = SwigWrap::requestConnection(
                String(context_.Nym()->ID()).Get(),
                nymID2,
                String(context_.Server()).Get(),
                lData);
        } break;
        default: {
            otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func()"
                  << std::endl;
        }
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_nymID2,
    const std::string& p_strData,
    const bool p_Bool)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_nymID2)) {
        otErr << strError << "p_nymID2" << std::endl;
    }

    if (!VerifyStringVal(p_strData)) {
        otErr << strError << "p_strData" << std::endl;
    }

    if (theType == ACKNOWLEDGE_NOTICE) {
        funcType = theType;
        nymID2 = p_nymID2;
        instrumentDefinitionID = p_strData;
        strData = SwigWrap::acknowledgeNotice(
            String(context_.Nym()->ID()).Get(),
            instrumentDefinitionID,
            String(context_.Server()).Get(),
            p_Bool);
        nTransNumsNeeded = 0;
    } else {
        otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func() "
                 "ERROR!!!!!!\n";
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_nymID2,
    const std::string& p_strData,
    const std::string& p_strData2)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_nymID2)) {
        otErr << strError << "p_nymID2" << std::endl;
    }

    if (!VerifyStringVal(p_strData)) {
        otErr << strError << "p_strData" << std::endl;
    }

    if (!VerifyStringVal(p_strData2)) {
        otErr << strError << "p_strData2" << std::endl;
    }

    nTransNumsNeeded = 1;

    if ((theType == SEND_USER_MESSAGE) || (theType == SEND_USER_INSTRUMENT)) {
        nTransNumsNeeded = 0;
        nymID2 = p_nymID2;
        strData = p_strData;
        strData2 = p_strData2;
    } else if (theType == TRIGGER_CLAUSE) {
        strData = p_nymID2;
        strData2 = p_strData;
        strData3 = p_strData2;
    } else if (theType == ACTIVATE_SMART_CONTRACT) {

        accountID = p_nymID2;   // the "official" asset account of the party
                                // activating the contract.;
        strData = p_strData;    // the agent's name for that party, as listed on
                                // the contract.;
        strData2 = p_strData2;  // the smart contract itself.;

        std::int32_t nNumsNeeded =
            SwigWrap::SmartContract_CountNumsNeeded(p_strData2, p_strData);

        if (nNumsNeeded > 0) {
            nTransNumsNeeded = nNumsNeeded;
        }
    } else if (theType == GET_BOX_RECEIPT) {
        nTransNumsNeeded = 0;
        accountID = p_nymID2;  // accountID (inbox/outbox) or NymID (nymbox) is
                               // passed here.;
        nData = stol(p_strData);
        strData = p_strData2;  // transaction number passed here as std::string;
    } else if (theType == ACKNOWLEDGE_BAILMENT) {
        nTransNumsNeeded = 0;
        nymID2 = p_nymID2;
        instrumentDefinitionID = p_strData;
        strData = SwigWrap::acknowledgeBailment(
            String(context_.Nym()->ID()).Get(),
            instrumentDefinitionID,
            String(context_.Server()).Get(),
            p_strData2);
    } else if (theType == ACKNOWLEDGE_OUTBAILMENT) {
        nTransNumsNeeded = 0;
        nymID2 = p_nymID2;
        instrumentDefinitionID = p_strData;
        strData = SwigWrap::acknowledgeOutBailment(
            String(context_.Nym()->ID()).Get(),
            instrumentDefinitionID,
            String(context_.Server()).Get(),
            p_strData2);
    } else if (theType == NOTIFY_BAILMENT) {
        nTransNumsNeeded = 0;
        nymID2 = p_nymID2;
        instrumentDefinitionID = p_strData;
        strData = SwigWrap::notifyBailment(
            String(context_.Server()).Get(),
            String(context_.Nym()->ID()).Get(),
            nymID2,
            instrumentDefinitionID,
            p_strData2);
    } else {
        otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func() "
                 "ERROR!!!!!!\n";
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_accountID,
    const std::string& p_strParam,
    std::int64_t p_lData,
    const std::string& p_strData2)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_accountID)) {
        otErr << strError << "p_accountID" << std::endl;
    }

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    lData = p_lData;  // std::int64_t Amount;
    nTransNumsNeeded = 0;

    if (theType == SEND_TRANSFER) {
        if (!VerifyStringVal(p_strData2)) {
            otErr << strError << "p_strData2" << std::endl;
        }
        nTransNumsNeeded = 1;
        accountID = p_accountID;
        accountID2 = p_strParam;
        strData = p_strData2;  // str  Note;
    } else if (theType == INITIATE_OUTBAILMENT) {
        nymID2 = p_accountID;
        instrumentDefinitionID = p_strParam;
        strData = SwigWrap::initiateOutBailment(
            String(context_.Server()).Get(),
            String(context_.Nym()->ID()).Get(),
            instrumentDefinitionID,
            lData,
            p_strData2);
    } else {
        otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func() "
                 "ERROR!!!!!!\n";
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_accountID,
    const std::string& p_strParam,
    const std::string& p_strData,
    std::int64_t p_lData2)
    : OTAPI_Func(context, otapi, theType)
{
    strData = p_strData;
    lData = p_lData2;
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_accountID)) {
        otErr << strError << "p_accountID" << std::endl;
    }

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    if (!VerifyStringVal(p_strData) && (STORE_SECRET != theType)) {
        otErr << strError << "p_strData" << std::endl;
    }

    switch (theType) {
        case (WITHDRAW_VOUCHER): {
            accountID = p_accountID;
            nymID2 = p_strParam;
        } break;
        case (PAY_DIVIDEND): {
            accountID = p_accountID;
            instrumentDefinitionID = p_strParam;
        } break;
        case (STORE_SECRET): {
            nTransNumsNeeded = 0;
            nymID2 = p_accountID;
            strData2 = SwigWrap::storeSecret(
                String(context_.Nym()->ID()).Get(),
                nymID2,
                String(context_.Server()).Get(),
                lData,
                p_strParam,
                strData);
        } break;
        default: {
            otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func() "
                     "ERROR!!!!!!\n";
        }
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_accountID,
    const std::string& p_strParam,
    const std::string& p_strData,
    const std::string& p_strData2)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_accountID)) {
        otErr << strError << "p_accountID" << std::endl;
    }

    if (!VerifyStringVal(p_strParam)) {
        otErr << strError << "p_strParam" << std::endl;
    }

    nTransNumsNeeded = 1;
    accountID = p_accountID;

    if (theType == SEND_USER_INSTRUMENT) {
        if (!VerifyStringVal(p_accountID)) {
            otErr << strError << "p_accountID" << std::endl;
        }
        if (!VerifyStringVal(p_strParam)) {
            otErr << strError << "p_strParam" << std::endl;
        }
        if (!VerifyStringVal(p_strData)) {
            otErr << strError << "p_strData" << std::endl;
        }
        nTransNumsNeeded = 0;
        nymID2 = p_accountID;  // Recipient Nym;
        strData = p_strParam;  // Recipient pubkey;
        strData2 = p_strData;  // Instrument for recipient.;
        accountID =
            p_strData2;  // sender_instrument is attached here. (Optional.);
    } else {
        otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func() "
                 "ERROR!!!!!!\n";
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    bool boolInput,
    const std::string& data,
    const std::string& data2,
    const std::string& data3)
    : OTAPI_Func(context, otapi, theType)
{
    strData = data;
    strData2 = data2;
    strData3 = data3;
    bBool = boolInput;
    const std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(strData)) {
        otErr << strError << "strData" << std::endl;
    }

    if (!VerifyStringVal(strData2)) {
        otErr << strError << "strData2" << std::endl;
    }

    if (!VerifyStringVal(strData3)) {
        otErr << strError << "strData3" << std::endl;
    }

    switch (theType) {
        case (SERVER_ADD_CLAIM): {
        } break;
        default: {
            otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func()"
                  << std::endl;
        }
    }
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& p_instrumentDefinitionID,
    const std::string& p_basket,
    const std::string& p_accountID,
    bool p_bBool,
    std::int32_t p_nTransNumsNeeded)
    : OTAPI_Func(context, otapi, theType)
{
    std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(p_instrumentDefinitionID)) {
        otErr << strError << "p_instrumentDefinitionID" << std::endl;
    }

    if (!VerifyStringVal(p_accountID)) {
        otErr << strError << "p_accountID" << std::endl;
    }

    if (!VerifyStringVal(p_basket)) {
        otErr << strError << "p_basket" << std::endl;
    }

    if (EXCHANGE_BASKET == theType) {
        // FYI. This is a transaction.
    }

    nTransNumsNeeded = p_nTransNumsNeeded;
    bBool = p_bBool;
    instrumentDefinitionID = p_instrumentDefinitionID;
    basket = p_basket;
    accountID = p_accountID;
}

OTAPI_Func::OTAPI_Func(
    OTAPI_Func_Type theType,
    opentxs::ServerContext& context,
    OT_API& otapi,
    const std::string& account,
    const std::string& account2,
    const std::string& data,
    const std::string& data2,
    const std::string& data3,
    const std::string& data4,
    bool boolInput)
    : OTAPI_Func(context, otapi, theType)
{
    accountID = account;
    accountID2 = account2;
    strData = data;
    strData2 = data2;
    strData3 = data3;
    strData4 = data4;
    bBool = boolInput;
    const std::string strError =
        "Warning: Empty std::string passed to OTAPI_Func.OTAPI_Func() as: ";

    if (!VerifyStringVal(accountID)) {
        otErr << strError << "accountID" << std::endl;
    }

    if (!VerifyStringVal(accountID2)) {
        otErr << strError << "accountID2" << std::endl;
    }

    if (!VerifyStringVal(strData)) {
        otErr << strError << "strData" << std::endl;
    }

    if (!VerifyStringVal(strData2)) {
        otErr << strError << "strData2" << std::endl;
    }

    if (!VerifyStringVal(strData3)) {
        otErr << strError << "strData3" << std::endl;
    }

    if (!VerifyStringVal(strData4)) {
        otErr << strError << "strData4" << std::endl;
    }

    switch (theType) {
        case (CREATE_MARKET_OFFER): {
            nTransNumsNeeded = 3;
        } break;
        case (ACKNOWLEDGE_CONNECTION): {
            strData5 = SwigWrap::acknowledge_connection(
                String(context_.Nym()->ID()).Get(),
                accountID2,
                String(context_.Server()).Get(),
                bBool,
                strData,
                strData2,
                strData3,
                strData4);
        } break;
        default: {
            otOut << "ERROR! WRONG TYPE passed to OTAPI_Func.OTAPI_Func()"
                  << std::endl;
        }
    }
}

std::int32_t OTAPI_Func::Run() const
{
    // -1 means error, no message was sent.
    //  0 means NO error, yet still no message was sent.
    // >0 means (usually) the request number is being returned.
    //
    switch (funcType) {
        case CHECK_NYM:
            return SwigWrap::checkNym(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                nymID2);
        case REGISTER_NYM:
            return SwigWrap::registerNym(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get());
        case DELETE_NYM:
            return SwigWrap::unregisterNym(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get());
        case SEND_USER_MESSAGE:
            return SwigWrap::sendNymMessage(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                nymID2,
                strData2);
        case SEND_USER_INSTRUMENT:
            // accountID stores here the sender's copy of the instrument, which
            // is
            // used only in the case of a cash purse.
            return SwigWrap::sendNymInstrument(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                nymID2,
                strData2,
                accountID);
        case GET_NYM_MARKET_OFFERS:
            return SwigWrap::getNymMarketOffers(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get());
        case CREATE_ASSET_ACCT:
            return SwigWrap::registerAccount(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                instrumentDefinitionID);
        case DELETE_ASSET_ACCT:
            return SwigWrap::deleteAssetAccount(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID);
        case ACTIVATE_SMART_CONTRACT:
            return SwigWrap::activateSmartContract(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData2);
        case TRIGGER_CLAUSE:
            return SwigWrap::triggerClause(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                stoll(strData),
                strData2,
                strData3);
        case EXCHANGE_BASKET:
            return SwigWrap::exchangeBasket(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                instrumentDefinitionID,
                basket,
                bBool);
        case GET_CONTRACT:
            return SwigWrap::getInstrumentDefinition(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                instrumentDefinitionID);
        case GET_MINT:
            return SwigWrap::getMint(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                instrumentDefinitionID);
        case ISSUE_ASSET_TYPE:
            return SwigWrap::registerInstrumentDefinition(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData);
        case ISSUE_BASKET:
            return SwigWrap::issueBasket(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                basket);
        case EXCHANGE_CASH:
#if OT_CASH
            return SwigWrap::exchangePurse(
                String(context_.Server()).Get(),
                instrumentDefinitionID,
                String(context_.Nym()->ID()).Get(),
                strData);
#else
            return -1;
#endif  // OT_CASH
        case KILL_MARKET_OFFER:
            return SwigWrap::killMarketOffer(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                stoll(strData));
        case KILL_PAYMENT_PLAN:
            return SwigWrap::killPaymentPlan(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                stoll(strData));
        case GET_BOX_RECEIPT:
            return SwigWrap::getBoxReceipt(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                nData,
                stoll(strData));
        case PROCESS_INBOX:
            return SwigWrap::processInbox(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                strData);
        case DEPOSIT_CASH:
#if OT_CASH
            return SwigWrap::notarizeDeposit(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                strData);
#else
            return -1;
#endif  // OT_CASH
        case DEPOSIT_CHEQUE:
            return SwigWrap::depositCheque(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                strData);
        case DEPOSIT_PAYMENT_PLAN:
            return SwigWrap::depositPaymentPlan(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData);
        case WITHDRAW_CASH:
#if OT_CASH
            return SwigWrap::notarizeWithdrawal(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                lData);
#else
            return -1;
#endif  // OT_CASH
        case WITHDRAW_VOUCHER:
            return SwigWrap::withdrawVoucher(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                nymID2,
                strData,
                lData);
        case PAY_DIVIDEND:
            return SwigWrap::payDividend(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                instrumentDefinitionID,
                strData,
                lData);
        case SEND_TRANSFER:
            return SwigWrap::notarizeTransfer(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                accountID,
                accountID2,
                lData,
                strData);
        case GET_MARKET_LIST:
            return SwigWrap::getMarketList(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get());
        case GET_MARKET_OFFERS:
            return SwigWrap::getMarketOffers(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData,
                lData);
        case GET_MARKET_RECENT_TRADES:
            return SwigWrap::getMarketRecentTrades(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData);
        case CREATE_MARKET_OFFER:
            return SwigWrap::issueMarketOffer(
                accountID,
                accountID2,
                stoll(strData),
                stoll(strData2),
                stoll(strData3),
                stoll(strData4),
                bBool,
                tData,
                strData5,
                lData);
        case ADJUST_USAGE_CREDITS:
            return SwigWrap::usageCredits(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                nymID2,
                stoll(strData));
        case INITIATE_BAILMENT:
        case INITIATE_OUTBAILMENT:
        case NOTIFY_BAILMENT:
        case REQUEST_CONNECTION:
            return SwigWrap::initiatePeerRequest(
                String(context_.Nym()->ID()).Get(),
                nymID2,
                String(context_.Server()).Get(),
                strData);
        case STORE_SECRET:
            return SwigWrap::initiatePeerRequest(
                String(context_.Nym()->ID()).Get(),
                nymID2,
                String(context_.Server()).Get(),
                strData2);
        case ACKNOWLEDGE_BAILMENT:
        case ACKNOWLEDGE_OUTBAILMENT:
        case ACKNOWLEDGE_NOTICE:
            return SwigWrap::initiatePeerReply(
                String(context_.Nym()->ID()).Get(),
                nymID2,
                String(context_.Server()).Get(),
                instrumentDefinitionID,
                strData);
        case ACKNOWLEDGE_CONNECTION:
            return SwigWrap::initiatePeerReply(
                String(context_.Nym()->ID()).Get(),
                accountID,
                String(context_.Server()).Get(),
                accountID2,
                strData5);
        case REGISTER_CONTRACT_NYM:
            return SwigWrap::registerContractNym(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                nymID2);
        case REGISTER_CONTRACT_SERVER:
            return SwigWrap::registerContractServer(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData);
        case REGISTER_CONTRACT_UNIT:
            return SwigWrap::registerContractUnit(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                instrumentDefinitionID);
        case REQUEST_ADMIN:
            return SwigWrap::requestAdmin(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData);
        case SERVER_ADD_CLAIM:
            return SwigWrap::serverAddClaim(
                String(context_.Server()).Get(),
                String(context_.Nym()->ID()).Get(),
                strData,
                strData2,
                strData3,
                bBool);
        default:
            break;
    }

    otOut << "ERROR! OTAPI_Func.Send() activated, with bad function type: "
          << funcType << "\n";

    return -1;
}

std::int32_t OTAPI_Func::SendRequestLowLevel(
    OTAPI_Func& theFunction,
    const std::string& IN_FUNCTION) const
{
    Utility MsgUtil(context_, otapi_);
    std::string strLocation = "OTAPI_Func::SendRequestLowLevel: " + IN_FUNCTION;

    SwigWrap::FlushMessageBuffer();

    std::int32_t nRun =
        theFunction.Run();  // <===== ATTEMPT TO SEND THE MESSAGE HERE...;

    if (nRun == -1)  // if the requestNumber returned by the send-attempt is -1,
                     // that means it DIDN'T SEND (error)
    {
        otOut << strLocation << ": Failed to send message due to error.\n";

        theFunction.nRequestNum = -1;
    } else if (nRun == 0)  // if the requestNumber returned by the send-attempt
                           // is 0, it means no error, but nothing was sent.
                           // (Like processing an empty box.)
    {
        otOut << strLocation << ": Didn't send this message, but NO error "
                                "occurred, either. (For example, a request to "
                                "process an empty Nymbox will return 0, "
                                "meaning, nothing was sent, but also no error "
                                "occurred.)\n";

        theFunction.nRequestNum = 0;
    } else {
        theFunction.nRequestNum = nRun;
    }

    // BY this point, we definitely have the request number, which means the
    // message was actually SENT. (At least.) This also means we can use nRun
    // later to query for a copy of that sent message (like if we need to
    // clawback
    // the transaction numbers from it later, once we confirm whether the server
    // actually never got it.)
    //
    return theFunction.nRequestNum;
}

std::string OTAPI_Func::SendTransaction(
    OTAPI_Func& theFunction,
    const std::string& IN_FUNCTION)
{
    std::int32_t nTotalRetries = 2;
    return SendTransaction(theFunction, IN_FUNCTION, nTotalRetries);
}

std::string OTAPI_Func::SendTransaction(
    OTAPI_Func& theFunction,
    const std::string& IN_FUNCTION,
    std::int32_t nTotalRetries) const
{
    Utility MsgUtil(context_, otapi_);
    std::string strLocation = "OTAPI_Func::SendTransaction: " + IN_FUNCTION;

    if (!MsgUtil.getIntermediaryFiles(
            String(theFunction.context_.Server()).Get(),
            String(theFunction.context_.Nym()->ID()).Get(),
            theFunction.accountID,
            false))  // bForceDownload=false))
    {
        otOut << strLocation << ", getIntermediaryFiles returned false. (It "
                                "couldn't download files that it needed.)\n";
        return "";
    }

    // GET TRANSACTION NUMBERS HERE IF NECESSARY.
    //
    std::int32_t getnym_trnsnum_count = SwigWrap::GetNym_TransactionNumCount(
        String(theFunction.context_.Server()).Get(),
        String(theFunction.context_.Nym()->ID()).Get());
    std::int32_t configTxnCount = MsgUtil.getNbrTransactionCount();
    bool b1 = (theFunction.nTransNumsNeeded > configTxnCount);
    std::int32_t comparative = 0;

    if (b1) {
        comparative = theFunction.nTransNumsNeeded;
    } else {
        comparative = configTxnCount;
    }

    if (getnym_trnsnum_count < comparative) {
        otOut << strLocation << ", I don't have enough transaction numbers to "
                                "perform this transaction. Grabbing more "
                                "now...\n";
        MsgUtil.setNbrTransactionCount(comparative);
        MsgUtil.getTransactionNumbers(
            String(theFunction.context_.Server()).Get(),
            String(theFunction.context_.Nym()->ID()).Get());
        MsgUtil.setNbrTransactionCount(configTxnCount);
    }

    // second try
    getnym_trnsnum_count = SwigWrap::GetNym_TransactionNumCount(
        String(context_.Server()).Get(), String(context_.Nym()->ID()).Get());
    if (getnym_trnsnum_count < comparative) {
        otOut
            << strLocation
            << ", failure: MsgUtil.getTransactionNumbers. (Trying again...)\n";

        // (the final parameter, the 'false' is us telling getTransNumbers that
        // it can skip the first call to getTransNumLowLevel)
        //
        MsgUtil.setNbrTransactionCount(comparative);
        MsgUtil.getTransactionNumbers(
            String(context_.Server()).Get(),
            String(context_.Nym()->ID()).Get(),
            false);
        MsgUtil.setNbrTransactionCount(configTxnCount);
    }

    // third try
    getnym_trnsnum_count = SwigWrap::GetNym_TransactionNumCount(
        String(context_.Server()).Get(), String(context_.Nym()->ID()).Get());
    if (getnym_trnsnum_count < comparative) {
        otOut
            << strLocation
            << ", failure: MsgUtil.getTransactionNumbers. (Trying again...)\n";

        // (the final parameter, the 'false' is us telling getTransNumbers that
        // it can skip the first call to getTransNumLowLevel)
        //
        MsgUtil.setNbrTransactionCount(comparative);
        MsgUtil.getTransactionNumbers(
            String(context_.Server()).Get(),
            String(context_.Nym()->ID()).Get(),
            false);
        MsgUtil.setNbrTransactionCount(configTxnCount);
    }

    // Giving up, if still a failure by this point.
    //
    getnym_trnsnum_count = SwigWrap::GetNym_TransactionNumCount(
        String(theFunction.context_.Server()).Get(),
        String(theFunction.context_.Nym()->ID()).Get());

    if (getnym_trnsnum_count < comparative) {
        otOut
            << strLocation
            << ", third failure: MsgUtil.getTransactionNumbers. (Giving up.)\n";
        return "";
    }

    bool bCanRetryAfterThis = false;

    std::string strResult = SendRequestOnce(
        theFunction, IN_FUNCTION, true, true, bCanRetryAfterThis);

    if (VerifyStringVal(strResult)) {
        otOut << " Getting Intermediary files.. \n";

        if (!MsgUtil.getIntermediaryFiles(
                String(theFunction.context_.Server()).Get(),
                String(theFunction.context_.Nym()->ID()).Get(),
                theFunction.accountID,
                true)) {
            otOut << strLocation << ", getIntermediaryFiles returned false. "
                                    "(After a success sending the transaction. "
                                    "Strange...)\n";
            return "";
        }

        return strResult;
    }

    //
    // Maybe we have an old Inbox or something.
    //

    // TODO!!  SECURITY:  This is where a GOOD CLIENT (vs. a test client)
    // will verify these std::intermediary files against your LAST SIGNED
    // RECEIPT,
    // using SwigWrap::VerifySomethingorother().
    // See verifyFiles() at the bottom of this file.
    // Add some kind of warning Modal Dialog here, since it's actually
    // normal for a NEW account (never had any receipts yet.) But for
    // any other account, this should ALWAYS VERIFY!
    //
    // Another note: this should happen INSIDE the getIntermediaryFiles call
    // itself,
    // and all similar calls.  You simply should not download those files,
    // without verifying them also. Otherwise you could end up signing
    // a future bad receipt, based on malicious, planted std::intermediary
    // files.

    std::int32_t nRetries = nTotalRetries;

    while ((nRetries > 0) && !VerifyStringVal(strResult) &&
           bCanRetryAfterThis) {
        --nRetries;

        bool bWillRetryAfterThis = true;

        if ((nRetries == 0) || !bCanRetryAfterThis) {
            bWillRetryAfterThis = false;
        }

        if (SwigWrap::CheckConnection(
                String(theFunction.context_.Server()).Get())) {
            strResult = SendRequestOnce(
                theFunction,
                IN_FUNCTION,
                true,
                bWillRetryAfterThis,
                bCanRetryAfterThis);
        }

        // In case of failure, we want to get these before we re-try.
        // But in case of success, we also want to get these, so we can
        // see the results of our success. So we get these either way...
        //
        if (VerifyStringVal(strResult)) {
            if (!MsgUtil.getIntermediaryFiles(
                    String(theFunction.context_.Server()).Get(),
                    String(theFunction.context_.Nym()->ID()).Get(),
                    theFunction.accountID,
                    true)) {
                otOut << strLocation
                      << ", getIntermediaryFiles (loop) returned false even "
                         "after successfully sending the transaction.\n";
                return "";
            }
            break;
        }
    }

    return strResult;
}

std::string OTAPI_Func::SendRequest(
    OTAPI_Func& theFunction,
    const std::string& IN_FUNCTION) const
{
    Utility MsgUtil(context_, otapi_);

    bool bCanRetryAfterThis = false;

    std::string strResult = SendRequestOnce(
        theFunction, IN_FUNCTION, false, true, bCanRetryAfterThis);

    if (!VerifyStringVal(strResult) && bCanRetryAfterThis) {
        if (SwigWrap::CheckConnection(
                String(theFunction.context_.Server()).Get())) {
            strResult = SendRequestOnce(
                theFunction, IN_FUNCTION, false, false, bCanRetryAfterThis);
        }
    }
    return strResult;
}

std::string OTAPI_Func::SendRequestOnce(
    OTAPI_Func& theFunction,
    const std::string& IN_FUNCTION,
    bool bIsTransaction,
    bool bWillRetryAfterThis,
    bool& bCanRetryAfterThis) const
{
    Utility MsgUtil(context_, otapi_);
    std::string strLocation = "OTAPI_Func::SendRequestOnce: " + IN_FUNCTION;

    bCanRetryAfterThis = false;

    std::string strReply = "";
    std::int32_t nlocalRequestNum = SendRequestLowLevel(
        theFunction, IN_FUNCTION);  // <========   FIRST ATTEMPT!!!!!!;

    if ((nlocalRequestNum == -1) || (nlocalRequestNum == 0)) {
        // bCanRetryAfterThis is already false here.
        return "";
    } else  // 1 and default.
    {
        if (nlocalRequestNum < -1) {
            return "";
        }

        strReply = MsgUtil.ReceiveReplyLowLevel(
            String(theFunction.context_.Server()).Get(),
            String(theFunction.context_.Nym()->ID()).Get(),
            nlocalRequestNum,
            IN_FUNCTION);  // <==== Here we RECEIVE the REPLY...;
    }

    // Below this point, we definitely have a request number.
    // (But not yet necessarily a reply...)
    //

    // nlocalRequestNum is positive and contains the request number from
    // sending.
    //
    // nReplySuccess contains status of the REPLY to that sent message.
    // nReplySuccess contains:
    //   0 == FAILURE reply msg from server,
    //   1 == SUCCESS reply msg from server (transaction could be success or
    // fail.)
    //  -1 == (ERROR)
    //
    // strReply contains the reply itself (or null.)
    //
    std::int32_t nReplySuccess = VerifyMessageSuccess(strReply);

    bool bMsgReplyError = (!VerifyStringVal(strReply) || (nReplySuccess < 0));

    bool bMsgReplySuccess = (!bMsgReplyError && (nReplySuccess > 0));
    bool bMsgReplyFailure = (!bMsgReplyError && (nReplySuccess == 0));

    bool bMsgTransSuccess;
    bool bMsgTransFailure;

    bool bMsgAnyError;
    bool bMsgAnyFailure;

    bool bMsgAllSuccess;

    // If you EVER are in a situation where you have to harvest numbers
    // back, it will ONLY be for transactions, not normal messages. (Those
    // are the only ones that USE transaction numbers.)
    //
    if (bIsTransaction)  // This request contains a TRANSACTION...
    {
        std::int32_t nTransSuccess;
        std::int32_t nBalanceSuccess;
        if (bMsgReplySuccess)  // If message was success, then let's see if the
                               // transaction was, too.
        {
            nBalanceSuccess = SwigWrap::Message_GetBalanceAgreementSuccess(
                String(theFunction.context_.Server()).Get(),
                String(theFunction.context_.Nym()->ID()).Get(),
                theFunction.accountID,
                strReply);

            if (nBalanceSuccess > 0) {
                // Sometimes a transaction is sent that is meant to "fail" in
                // order to cancel itself from ever being run in the future.
                // It's being cancelled. In that case, whether the server reply
                // itself is acknowledged or rejection, either way,
                // IsCancelled() will be set to TRUE.
                // This is used when cancelling a cheque, or a payment plan, or
                // a smart contract, so that it can never be activated at some
                // future time.
                //
                // Therefore when we see that IsCancelled is set to TRUE, we
                // std::interpret it as a "success" as far as the UI is
                // concerned,
                // even though behind the scenes, it is still "rejected" and
                // transaction numbers were harvested from it.
                //
                std::int32_t nTransCancelled =
                    SwigWrap::Message_IsTransactionCanceled(
                        String(theFunction.context_.Server()).Get(),
                        String(theFunction.context_.Nym()->ID()).Get(),
                        theFunction.accountID,
                        strReply);

                // If it's not cancelled, then we assume it's a normal
                // transaction (versus a cancellation)
                // and we check for success/failure as normal...
                //
                if (1 != nTransCancelled) {
                    nTransSuccess = SwigWrap::Message_GetTransactionSuccess(
                        String(theFunction.context_.Server()).Get(),
                        String(theFunction.context_.Nym()->ID()).Get(),
                        theFunction.accountID,
                        strReply);
                } else  // If it WAS cancelled, then for the UI we say "Success"
                        // even though OT behind the scenes is harvesting as
                        // though it failed.
                {       // (Which is what we want to happen, in the case that a
                    // cancellation was performed.)
                    // This way, the UI won't go off doing a bunch of
                    // unnecessary retries for a "failed" transaction.
                    // (After all, if it was cancelled, then we know for a fact
                    // that all future retries will fail anyway.)
                    //
                    nTransSuccess = 1;
                }
            } else {
                nTransSuccess = -1;
            }
        } else {
            nBalanceSuccess = -1;
            nTransSuccess = -1;
        }
        // All of these booleans (except "error") represent RECEIVED ANSWERS
        // from the server.
        // In other words, "failure" does not mean "failed to find message."
        // Rather, it means "DEFINITELY got a reply, and that reply says
        // status==failure."
        //

        bool bMsgBalanceError =
            (!VerifyStringVal(strReply) || (nBalanceSuccess < 0));
        bool bMsgBalanceSuccess =
            (!bMsgReplyError && !bMsgBalanceError && (nBalanceSuccess > 0));
        bool bMsgBalanceFailure =
            (!bMsgReplyError && !bMsgBalanceError && (nBalanceSuccess == 0));

        bool bMsgTransError =
            (!VerifyStringVal(strReply) || (nTransSuccess < 0));
        bMsgTransSuccess =
            (!bMsgReplyError && !bMsgBalanceError && !bMsgTransError &&
             (nTransSuccess > 0));
        bMsgTransFailure =
            (!bMsgReplyError && !bMsgBalanceError && !bMsgTransError &&
             (nTransSuccess == 0));

        bMsgAnyError = (bMsgReplyError || bMsgBalanceError || bMsgTransError);
        bMsgAnyFailure =
            (bMsgReplyFailure || bMsgBalanceFailure || bMsgTransFailure);

        bMsgAllSuccess =
            (bMsgReplySuccess && bMsgBalanceSuccess && bMsgTransSuccess);

    } else  // it's NOT a transaction, but a normal message..
    {
        bMsgTransSuccess = false;
        bMsgTransFailure = false;

        bMsgAnyError = (bMsgReplyError);
        bMsgAnyFailure = (bMsgReplyFailure);

        bMsgAllSuccess = (bMsgReplySuccess);
    }

    // We know the message SENT. The above logic is about figuring out whether
    // the reply message,
    // the transaction inside it, and the balance agreement inside that
    // transaction, whether
    // any of those three things is a definite error, a definite failure, or a
    // definite success.
    // (Any one of those things could be true, OR NOT, and we can only act as if
    // they are, if we
    // have definitive proof in any of those cases.)
    //
    // The below logic is about what sort of REPLY we may have gotten (if
    // anything.)
    // Without a definite reply we cannot claw back. But the Nymbox can show us
    // this answer,
    // either now, or later...
    //
    if (bMsgAllSuccess) {
        // the Msg was a complete success, including the message
        // AND the transaction AND the balance agreement.
        // Therefore, there's DEFINITELY nothing to clawback.
        //
        // (Thus I RemoveSentMessage for the message, since
        // I'm totally done with it now. NO NEED TO HARVEST anything, either.)
        //
        //          var nRemoved =
        // SwigWrap::RemoveSentMessage(Integer.toString(nlocalRequestNum),
        // String(theFunction.context_.Server()).Get(),
        // String(theFunction.context_.Nym()->ID()).Get());
        //
        // NOTE: The above call is unnecessary, since a successful reply means
        // we already received the successful server reply, and OT's
        // "ProcessServerReply"
        // already removed the sent message from the sent buffer (so no need to
        // do that here.)

        //          otOut << strLocation << ", SendRequestOnce():
        // OT_API_RemoveSentMessage: " + nRemoved);

        return strReply;

    }
    // In this case we want to grab the Nymbox and see if the replyNotice is
    // there.
    // If it IS, then OT server DEFINITELY replied to it (the MESSAGE was a
    // success,
    // whether the transaction inside of it was success or fail.) So we know
    // bMsgReplySuccess
    // is true, if we find a replyNotice.
    // From there, we can also check for transaction success.
    //
    else if (bMsgAnyError || bMsgAnyFailure)  // let's resync, and clawback
                                              // whatever transaction numbers we
                                              // might have used on the
                                              // Request...
    {
        bool bWasGetReqSent = false;
        const auto nGetRequestNumber =
            context_.UpdateRequestNumber(bWasGetReqSent);

        // GET REQUEST WAS A SUCCESS.
        //
        if (bWasGetReqSent && (nGetRequestNumber > 0)) {
            bCanRetryAfterThis = true;

            // But--if it was a TRANSACTION, then we're not done syncing yet!
            //
            if (bIsTransaction) {
                bCanRetryAfterThis = false;

                //
                // Maybe we have an old Inbox or something.
                // NEW CODE HERE FOR DEBUGGING (THIS BLOCK)
                //
                bool bForceDownload = true;
                if (!MsgUtil.getIntermediaryFiles(
                        String(theFunction.context_.Server()).Get(),
                        String(theFunction.context_.Nym()->ID()).Get(),
                        theFunction.accountID,
                        bForceDownload)) {
                    otOut << strLocation << ", getIntermediaryFiles returned "
                                            "false. (After a failure to send "
                                            "the transaction. Thus, I give "
                                            "up.)\n";
                    return "";
                }

                bool bWasFound = false;
                bool bWasSent = false;

                OTfourbool the_foursome = {bMsgReplySuccess,
                                           bMsgReplyFailure,
                                           bMsgTransSuccess,
                                           bMsgTransFailure};

                bForceDownload = false;

                std::int32_t nProcessNymboxResult =
                    MsgUtil.getAndProcessNymbox_8(
                        String(theFunction.context_.Server()).Get(),
                        String(theFunction.context_.Nym()->ID()).Get(),
                        bWasSent,
                        bForceDownload,
                        nlocalRequestNum,
                        bWasFound,
                        bWillRetryAfterThis,
                        the_foursome);

                // bHarvestingForRetry,// bHarvestingForRetry is INPUT, in the
                // case nlocalRequestNum needs to be harvested before a flush
                // occurs.

                //  bMsgReplySuccess,    // bMsgReplySuccess is INPUT, and is in
                // case nlocalRequestNum needs to be HARVESTED before a FLUSH
                // happens.
                //  bMsgReplyFailure,    // bMsgReplyFailure is INPUT, and is in
                // case nlocalRequestNum needs to be HARVESTED before a FLUSH
                // happens.
                //  bMsgTransSuccess,    // bMsgTransSuccess is INPUT, and is in
                // case nlocalRequestNum needs to be HARVESTED before a FLUSH
                // happens.
                //  bMsgTransFailure)    // Etc.

                // FIX: Add '(' and ')' here to silence warning. But where?
                if ((bWasSent && (1 == nProcessNymboxResult)) ||
                    (!bWasSent && (0 == nProcessNymboxResult)))  // success
                                                                 // processing
                                                                 // Nymbox.
                {
                    bCanRetryAfterThis = true;
                }

                // This means a request number was returned, since the
                // processNymbox failed,
                // and hasn't been properly harvested, so we either need to
                // harvest it for a re-try,
                // or flush it.
                //
                else if (bWasSent && (nProcessNymboxResult > 1)) {
                    std::string strNymbox = SwigWrap::LoadNymboxNoVerify(
                        String(theFunction.context_.Server()).Get(),
                        String(theFunction.context_.Nym()->ID())
                            .Get());  // FLUSH SENT MESSAGES!!!!  (AND
                                      // HARVEST.);

                    if (VerifyStringVal(strNymbox)) {
                        SwigWrap::FlushSentMessages(
                            false,
                            String(theFunction.context_.Server()).Get(),
                            String(theFunction.context_.Nym()->ID()).Get(),
                            strNymbox);
                    }
                }
            }  // if (bIsTransaction)

        }  // if getRequestNumber was success.
        else {
            otOut << strLocation
                  << ", Failure: Never got reply from server, "
                     "so tried getRequestNumber, and that failed too. "
                     "(I give up.)\n";

            // Note: cannot harvest transaction nums here because I do NOT know
            // for sure
            // whether the server has replied to the message or not! (Not until
            // I successfully
            // download my Nymbox.) Therefore, do NOT harvest or flush, but hold
            // back and wait
            // until the next attempt does a successful getNymbox and THEN do a
            // "flush sent" after
            // that. (That's the time we'll know for SURE what happened to the
            // original reply.)
            //
            // (Therefore LEAVE the sent message in the sent queue.)

            return "";
        }
    }  // else if (bMsgAnyError || bMsgAnyFailure)

    // Returning an empty std::string.

    return "";
}

// used for passing and returning values when giving a
// lambda function to a loop function.
//
// cppcheck-suppress uninitMemberVar
the_lambda_struct::the_lambda_struct() {}

OTDB::OfferListNym* loadNymOffers(
    const std::string& notaryID,
    const std::string& nymID)
{
    OTDB::OfferListNym* offerList = nullptr;

    if (OTDB::Exists("nyms", notaryID, "offers", nymID + ".bin")) {
        otWarn << "Offers file exists... Querying nyms...\n";
        OTDB::Storable* storable = OTDB::QueryObject(
            OTDB::STORED_OBJ_OFFER_LIST_NYM,
            "nyms",
            notaryID,
            "offers",
            nymID + ".bin");

        if (nullptr == storable) {
            otOut << "Unable to verify storable object. Probably doesn't "
                     "exist.\n";
            return nullptr;
        }

        otWarn << "QueryObject worked. Now dynamic casting from storable to a "
                  "(nym) offerList...\n";
        offerList = dynamic_cast<OTDB::OfferListNym*>(storable);

        if (nullptr == offerList) {
            otOut
                << "Unable to dynamic cast a storable to a (nym) offerList.\n";
            return nullptr;
        }
    }

    return offerList;
}

MapOfMaps* convert_offerlist_to_maps(OTDB::OfferListNym& offerList)
{
    std::string strLocation = "convert_offerlist_to_maps";

    MapOfMaps* map_of_maps = nullptr;

    // LOOP THROUGH THE OFFERS and sort them std::into a map_of_maps, key is:
    // scale-instrumentDefinitionID-currencyID
    // the value for each key is a sub-map, with the key: transaction ID and
    // value: the offer data itself.
    //
    std::int32_t nCount = offerList.GetOfferDataNymCount();
    std::int32_t nTemp = nCount;

    if (nCount > 0) {
        for (std::int32_t nIndex = 0; nIndex < nCount; ++nIndex) {

            nTemp = nIndex;
            OTDB::OfferDataNym* offerDataPtr = offerList.GetOfferDataNym(nTemp);

            if (!offerDataPtr) {
                otOut << strLocation << ": Unable to reference (nym) offerData "
                                        "on offerList, at index: "
                      << nIndex << "\n";
                return map_of_maps;
            }

            OTDB::OfferDataNym& offerData = *offerDataPtr;
            std::string strScale = offerData.scale;
            std::string strInstrumentDefinitionID =
                offerData.instrument_definition_id;
            std::string strCurrencyTypeID = offerData.currency_type_id;
            std::string strSellStatus = offerData.selling ? "SELL" : "BUY";
            std::string strTransactionID = offerData.transaction_id;

            std::string strMapKey = strScale + "-" + strInstrumentDefinitionID +
                                    "-" + strCurrencyTypeID;

            SubMap* sub_map = nullptr;
            if (nullptr != map_of_maps && !map_of_maps->empty() &&
                (map_of_maps->count(strMapKey) > 0)) {
                sub_map = (*map_of_maps)[strMapKey];
            }

            if (nullptr != sub_map) {
                otWarn << strLocation << ": The sub-map already exists!\n";

                // Let's just add this offer to the existing submap
                // (There must be other offers already there for the same
                // market, since the submap already exists.)
                //
                // the sub_map for this market is mapped by BUY/SELL ==> the
                // actual offerData.
                //

                (*sub_map)[strTransactionID] = &offerData;
            } else  // submap does NOT already exist for this market. (Create
                    // it...)
            {
                otWarn << strLocation
                       << ": The sub-map does NOT already exist!\n";
                //
                // Let's create the submap with this new offer, and add it
                // to the main map.
                //
                sub_map = new SubMap;
                (*sub_map)[strTransactionID] = &offerData;

                if (nullptr == map_of_maps) {
                    map_of_maps = new MapOfMaps;
                }

                (*map_of_maps)[strMapKey] = sub_map;
            }

            // Supposedly by this point I have constructed a map keyed by the
            // market, which returns a sub_map for each market. Each sub map
            // uses the key "BUY" or "SELL" and that points to the actual
            // offer data. (Like a Multimap.)
            //
            // Therefore we have sorted out all the buys and sells for each
            // market. Later on, we can loop through the main map, and for each
            // market, we can loop through all the buys and sells.
        }  // for (constructing the map_of_maps and all the sub_maps, so that
           // the
           // offers are sorted
           // by market and buy/sell status.
    }

    return map_of_maps;
}

std::int32_t output_nymoffer_data(
    const OTDB::OfferDataNym& offer_data,
    std::int32_t nIndex,
    const MapOfMaps&,
    const SubMap&,
    the_lambda_struct&)  // if 10 offers are printed for the
                         // SAME market, nIndex will be 0..9
{  // extra_vals unused in this function, but not in others that share this
    // parameter profile.
    // (It's used as a lambda.)

    std::string strScale = offer_data.scale;
    std::string strInstrumentDefinitionID = offer_data.instrument_definition_id;
    std::string strCurrencyTypeID = offer_data.currency_type_id;
    std::string strSellStatus = offer_data.selling ? "SELL" : "BUY";
    std::string strTransactionID = offer_data.transaction_id;
    std::string strAvailableAssets = std::to_string(
        std::stoll(offer_data.total_assets) -
        std::stoll(offer_data.finished_so_far));

    if (0 == nIndex)  // first iteration! (Output a header.)
    {
        otOut << "\nScale:\t\t" << strScale << "\n";
        otOut << "Asset:\t\t" << strInstrumentDefinitionID << "\n";
        otOut << "Currency:\t" << strCurrencyTypeID << "\n";
        otOut << "\nIndex\tTrans#\tType\tPrice\tAvailable\n";
    }

    //
    // Okay, we have the offer_data, so let's output it!
    //
    std::cout << (nIndex) << "\t" << offer_data.transaction_id << "\t"
              << strSellStatus << "\t" << offer_data.price_per_scale << "\t"
              << strAvailableAssets << "\n";

    return 1;
}

// If you have a buy offer, to buy silver for $30, and to sell silver for $35,
// what happens tomorrow when the market shifts, and you want to buy for $40
// and sell for $45 ?
//
// Well, now you need to cancel certain sell orders from yesterday! Because why
// on earth would you want to sell silver for $35 while buying it for $40?
// (knotwork raised ) That would be buy-high, sell-low.
//
// Any rational trader would cancel the old $35 sell order before placing a new
// $40 buy order!
//
// Similarly, if the market went DOWN such that my old offers were $40 buy / $45
// sell, and my new offers are going to be $30 buy / $35 sell, then I want to
// cancel certain buy orders for yesterday. After all, why on earth would you
// want to buy silver for $40 meanwhile putting up a new sell order at $35!
// You would immediately just turn around, after buying something, and sell it
// for LESS?
//
// Since the user would most likely be forced anyway to do this, for reasons of
// self-interest, it will probably end up as the default behavior here.
//

// RETURN VALUE: extra_vals will contain a list of offers that need to be
// removed AFTER

std::int32_t find_strange_offers(
    const OTDB::OfferDataNym& offer_data,
    const std::int32_t,
    const MapOfMaps&,
    const SubMap&,
    the_lambda_struct& extra_vals)  // if 10 offers are
                                    // printed
                                    // for the SAME market,
                                    // nIndex will be 0..9
{
    std::string strLocation = "find_strange_offers";
    /*
    me: How about this — when you do "opentxs newoffer" I can alter that
    script to automatically cancel any sell offers for a lower amount
    than my new buy offer, if they're on the same market at the same scale.
    and vice versa. Vice versa meaning, cancel any bid offers for a higher
    amount than my new sell offer.

    knotwork: yeah that would work.

    So when placing a buy offer, check all the other offers I already have at
    the same scale,
    same asset and currency ID. (That is, the same "market" as denoted by
    strMapKey in "opentxs showmyoffers")
    For each, see if it's a sell offer and if so, if the amount is lower than
    the amount on
    the new buy offer, then cancel that sell offer from the market. (Because I
    don't want to buy-high, sell low.)

    Similarly, if placing a sell offer, then check all the other offers I
    already have at the
    same scale, same asset and currency ID, (the same "market" as denoted by
    strMapKey....) For
    each, see if it's a buy offer and if so, if the amount is higher than the
    amount of my new
    sell offer, then cancel that buy offer from the market. (Because I don't
    want some old buy offer
    for $10 laying around for the same stock that I'm SELLING for $8! If I dump
    100 shares, I'll receive
    $800--I don't want my software to automatically turn around and BUY those
    same shares again for $1000!
    That would be a $200 loss.)

    This is done here. This function gets called once for each offer that's
    active for this Nym.
    extra_vals contains the relevant info we're looking for, and offer_data
    contains the current
    offer (as we loop through ALL this Nym's offers, this function gets called
    for each one.)
    So here we just need to compare once, and add to the list if the comparison
    matches.
    */
    /*
    attr the_lambda_struct::the_vector        // used for returning a list of
    something.
    attr the_lambda_struct::the_asset_acct    // for newoffer, we want to remove
    existing offers for the same accounts in certain cases.
    attr the_lambda_struct::the_currency_acct // for newoffer, we want to remove
    existing offers for the same accounts in certain cases.
    attr the_lambda_struct::the_scale         // for newoffer as well.
    attr the_lambda_struct::the_price         // for newoffer as well.
    attr the_lambda_struct::bSelling          // for newoffer as well.
    */
    otLog4 << strLocation << ": About to compare the new potential offer "
                             "against one of the existing ones...";

    if ((extra_vals.the_asset_acct == offer_data.asset_acct_id) &&
        (extra_vals.the_currency_acct == offer_data.currency_acct_id) &&
        (extra_vals.the_scale == offer_data.scale)) {
        otLog4 << strLocation << ": the account IDs and the scale match...";

        // By this point we know the current offer_data has the same asset acct,
        // currency acct, and scale
        // as the offer we're comparing to all the rest.
        //
        // But that's not enough: we also need to compare some prices:
        //
        // So when placing a buy offer, check all the other offers I already
        // have.
        // For each, see if it's a sell offer and if so, if the amount is lower
        // than the amount on
        // the new buy offer, then cancel that sell offer from the market.
        // (Because I don't want to buy-high, sell low.)
        //
        if (!extra_vals.bSelling && offer_data.selling &&
            (stoll(offer_data.price_per_scale) < stoll(extra_vals.the_price))) {
            extra_vals.the_vector.push_back(offer_data.transaction_id);
        }
        // Similarly, when placing a sell offer, check all the other offers I
        // already have.
        // For each, see if it's a buy offer and if so, if the amount is higher
        // than the amount of my new
        // sell offer, then cancel that buy offer from the market.
        //
        else if (
            extra_vals.bSelling && !offer_data.selling &&
            (stoll(offer_data.price_per_scale) > stoll(extra_vals.the_price))) {
            extra_vals.the_vector.push_back(offer_data.transaction_id);
        }
    }
    // We don't actually do the removing here, since we are still looping
    // through the maps.
    // So we just add the IDs to a vector so that the caller can do the removing
    // once this loop is over.

    return 1;
}

std::int32_t iterate_nymoffers_sub_map(
    const MapOfMaps& map_of_maps,
    SubMap& sub_map,
    LambdaFunc the_lambda)
{
    the_lambda_struct extra_vals;
    return iterate_nymoffers_sub_map(
        map_of_maps, sub_map, the_lambda, extra_vals);
}

// low level. map_of_maps and sub_map must be good. (assumed.)
//
// extra_vals allows you to pass any extra data you want std::into your
// lambda, for when it is called. (Like a functor.)
//
std::int32_t iterate_nymoffers_sub_map(
    const MapOfMaps& map_of_maps,
    SubMap& sub_map,
    LambdaFunc the_lambda,
    the_lambda_struct& extra_vals)
{
    // the_lambda must be good (assumed) and must have the parameter profile
    // like this sample:
    // def the_lambda(offer_data, nIndex, map_of_maps, sub_map, extra_vals)
    //
    // if 10 offers are printed for the SAME market, nIndex will be 0..9

    std::string strLocation = "iterate_nymoffers_sub_map";

    // Looping through the map_of_maps, we are now on a valid sub_map in this
    // iteration.
    // Therefore let's loop through the offers on that sub_map and output them!
    //
    // var range_sub_map = sub_map.range();

    SubMap* sub_mapPtr = &sub_map;
    if (!sub_mapPtr) {
        otOut << strLocation << ": No range retrieved from sub_map. It must be "
                                "non-existent, I guess.\n";
        return -1;
    }
    if (sub_map.empty()) {
        // Should never happen since we already made sure all the sub_maps
        // have data on them. Therefore if this range is empty now, it's a
        // chaiscript
        // bug (extremely unlikely.)
        //
        otOut << strLocation << ": Error: A range was retrieved for the "
                                "sub_map, but the range is empty.\n";
        return -1;
    }

    std::int32_t nIndex = -1;
    for (auto it = sub_map.begin(); it != sub_map.end(); ++it) {
        ++nIndex;
        // var offer_data_pair = range_sub_map.front();

        if (nullptr == it->second) {
            otOut << strLocation << ": Looping through range_sub_map range, "
                                    "and first offer_data_pair fails to "
                                    "verify.\n";
            return -1;
        }

        OTDB::OfferDataNym& offer_data = *it->second;
        std::int32_t nLambda = (*the_lambda)(
            offer_data,
            nIndex,
            map_of_maps,
            sub_map,
            extra_vals);  // if 10 offers are printed for the SAME
                          // market, nIndex will be 0..9;
        if (-1 == nLambda) {
            otOut << strLocation << ": Error: the_lambda failed.\n";
            return -1;
        }
    }
    sub_map.clear();

    return 1;
}

std::int32_t iterate_nymoffers_maps(
    MapOfMaps& map_of_maps,
    LambdaFunc the_lambda)  // low level. map_of_maps
                            // must be
                            // good. (assumed.)
{
    the_lambda_struct extra_vals;
    return iterate_nymoffers_maps(map_of_maps, the_lambda, extra_vals);
}

// extra_vals allows you to pass any extra data you want std::into your
// lambda, for when it is called. (Like a functor.)
//
std::int32_t iterate_nymoffers_maps(
    MapOfMaps& map_of_maps,
    LambdaFunc the_lambda,
    the_lambda_struct& extra_vals)  // low level.
                                    // map_of_maps
                                    // must be good.
                                    // (assumed.)
{
    // the_lambda must be good (assumed) and must have the parameter profile
    // like this sample:
    // def the_lambda(offer_data, nIndex, map_of_maps, sub_map, extra_vals)
    // //
    // if 10 offers are printed for the SAME market, nIndex will be 0..9

    std::string strLocation = "iterate_nymoffers_maps";

    // Next let's loop through the map_of_maps and output the offers for each
    // market therein...
    //
    // var range_map_of_maps = map_of_maps.range();
    MapOfMaps* map_of_mapsPtr = &map_of_maps;
    if (!map_of_mapsPtr) {
        otOut << strLocation << ": No range retrieved from map_of_maps.\n";
        return -1;
    }
    if (map_of_maps.empty()) {
        otOut << strLocation << ": A range was retrieved for the map_of_maps, "
                                "but the range is empty.\n";
        return -1;
    }

    for (auto it = map_of_maps.begin(); it != map_of_maps.end(); ++it) {
        // var sub_map_pair = range_map_of_maps.front();
        if (nullptr == it->second) {
            otOut << strLocation << ": Looping through map_of_maps range, and "
                                    "first sub_map_pair fails to verify.\n";
            return -1;
        }

        std::string strMapKey = it->first;

        SubMap& sub_map = *it->second;
        if (sub_map.empty()) {
            otOut << strLocation << ": Error: Sub_map is empty (Then how is it "
                                    "even here?? Submaps are only added based "
                                    "on existing offers.)\n";
            return -1;
        }

        std::int32_t nSubMap = iterate_nymoffers_sub_map(
            map_of_maps, sub_map, the_lambda, extra_vals);
        if (-1 == nSubMap) {
            otOut << strLocation
                  << ": Error: while trying to iterate_nymoffers_sub_map.\n";
            return -1;
        }
    }
    map_of_maps.clear();

    return 1;
}
}  // namespace opentxs
