// Copyright (c) 2010-2020 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "OTTestEnvironment.hpp"
#include "integration/Helpers.hpp"

#define UNIT_DEFINITION_CONTRACT_VERSION 2
#define UNIT_DEFINITION_CONTRACT_NAME "Mt Gox USD"
#define UNIT_DEFINITION_TERMS "YOLO"
#define UNIT_DEFINITION_PRIMARY_UNIT_NAME "dollars"
#define UNIT_DEFINITION_SYMBOL "$"
#define UNIT_DEFINITION_TLA "USD"
#define UNIT_DEFINITION_POWER 2
#define UNIT_DEFINITION_FRACTIONAL_UNIT_NAME "cents"
#define UNIT_DEFINITION_UNIT_OF_ACCOUNT ot::proto::CITEMTYPE_USD

namespace
{
class Test_Pair : public ::testing::Test
{
public:
    static Callbacks cb_chris_;
    static Issuer issuer_data_;
    static const StateMap state_;
    static ot::OTUnitID unit_id_;

    const ot::api::client::Manager& api_issuer_;
    const ot::api::client::Manager& api_chris_;
    const ot::api::server::Manager& api_server_1_;
    ot::OTZMQListenCallback issuer_peer_request_cb_;
    ot::OTZMQListenCallback chris_rename_notary_cb_;
    ot::OTZMQSubscribeSocket chris_ui_update_listener_;
    ot::OTZMQSubscribeSocket issuer_peer_request_listener_;
    ot::OTZMQSubscribeSocket chris_rename_notary_listener_;

    Test_Pair()
        : api_issuer_(
              ot::Context().StartClient(OTTestEnvironment::test_args_, 0))
        , api_chris_(
              ot::Context().StartClient(OTTestEnvironment::test_args_, 1))
        , api_server_1_(
              ot::Context().StartServer(OTTestEnvironment::test_args_, 0, true))
        , issuer_peer_request_cb_(ot::network::zeromq::ListenCallback::Factory(
              [this](const auto& in) { issuer_peer_request(in); }))
        , chris_rename_notary_cb_(ot::network::zeromq::ListenCallback::Factory(
              [this](const auto& in) { chris_rename_notary(in); }))
        , chris_ui_update_listener_(
              api_chris_.ZeroMQ().SubscribeSocket(cb_chris_.callback_))
        , issuer_peer_request_listener_(
              api_issuer_.ZeroMQ().SubscribeSocket(issuer_peer_request_cb_))
        , chris_rename_notary_listener_(
              api_chris_.ZeroMQ().SubscribeSocket(chris_rename_notary_cb_))
    {
        subscribe_sockets();

        const_cast<Server&>(server_1_).init(api_server_1_);
        const_cast<User&>(issuer_).init(api_issuer_, server_1_);
        const_cast<User&>(chris_).init(api_chris_, server_1_);
    }

    void subscribe_sockets()
    {
        ASSERT_TRUE(chris_ui_update_listener_->Start(
            api_chris_.Endpoints().WidgetUpdate()));
        ASSERT_TRUE(issuer_peer_request_listener_->Start(
            api_issuer_.Endpoints().PeerRequestUpdate()));
        ASSERT_TRUE(chris_rename_notary_listener_->Start(
            api_chris_.Endpoints().PairEvent()));
    }

    void chris_rename_notary(const ot::network::zeromq::Message& in)
    {
        const auto body = in.Body();

        EXPECT_EQ(1, body.size());

        if (1 != body.size()) { return; }

        const auto event = ot::proto::Factory<ot::proto::PairEvent>(body.at(0));

        EXPECT_TRUE(ot::proto::Validate(event, ot::VERBOSE));
        EXPECT_EQ(1, event.version());
        EXPECT_EQ(ot::proto::PAIREVENT_RENAME, event.type());
        EXPECT_EQ(issuer_.nym_id_->str(), event.issuer());
        EXPECT_TRUE(api_chris_.Wallet().SetServerAlias(
            server_1_.id_, issuer_data_.new_notary_name_));

        const auto result = api_chris_.OTX().DownloadNym(
            chris_.nym_id_, server_1_.id_, issuer_.nym_id_);

        EXPECT_NE(0, result.first);

        if (0 == result.first) { return; }
    }

    void issuer_peer_request(const ot::network::zeromq::Message& in)
    {
        const auto body = in.Body();

        EXPECT_EQ(2, body.size());

        if (2 != body.size()) { return; }

        EXPECT_EQ(issuer_.nym_id_->str(), std::string(body.at(0)));

        const auto request =
            ot::proto::Factory<ot::proto::PeerRequest>(body.at(1));

        EXPECT_TRUE(ot::proto::Validate(request, ot::VERBOSE));
        EXPECT_EQ(std::string(body.at(0)), request.recipient());
        EXPECT_EQ(server_1_.id_->str(), request.server());

        switch (request.type()) {
            case ot::proto::PEERREQUEST_BAILMENT: {
                EXPECT_EQ(request.bailment().serverid(), request.server());
                EXPECT_EQ(request.bailment().unitid(), unit_id_->str());

                api_issuer_.OTX().AcknowledgeBailment(
                    issuer_.nym_id_,
                    api_issuer_.Factory().ServerID(request.server()),
                    api_issuer_.Factory().NymID(request.initiator()),
                    api_issuer_.Factory().Identifier(request.id()),
                    std::to_string(++issuer_data_.bailment_counter_));

                if (issuer_data_.expected_bailments_ ==
                    issuer_data_.bailment_counter_) {
                    issuer_data_.bailment_promise_.set_value(true);
                }
            } break;
            case ot::proto::PEERREQUEST_STORESECRET: {
                // TODO
            } break;
            case ot::proto::PEERREQUEST_CONNECTIONINFO: {
                // TODO
            } break;
            default: {
                OT_FAIL;
            }
        }
    }
};

ot::OTUnitID Test_Pair::unit_id_{ot::identifier::UnitDefinition::Factory()};
Callbacks Test_Pair::cb_chris_{chris_.name_};
const std::string Issuer::new_notary_name_{"Chris's Notary"};
Issuer Test_Pair::issuer_data_{};
const StateMap Test_Pair::state_{
    {chris_.name_,
     {
         {Widget::AccountSummaryUSD,
          {
              {0,
               []() -> bool {
                   const auto& widget = chris_.api_->UI().AccountSummary(
                       chris_.nym_id_, ot::proto::CITEMTYPE_USD);
                   auto row = widget.First();

                   EXPECT_FALSE(row->Valid());

                   return true;
               }},
              {1,
               []() -> bool {
                   const auto& widget = chris_.api_->UI().AccountSummary(
                       chris_.nym_id_, ot::proto::CITEMTYPE_USD);
                   auto row = widget.First();

                   EXPECT_TRUE(row->Valid());

                   if (false == row->Valid()) { return false; }

                   EXPECT_TRUE(row->ConnectionState());
                   EXPECT_EQ(row->Name(), "localhost");
                   EXPECT_FALSE(row->Trusted());

                   {
                       const auto subrow = row->First();

                       EXPECT_TRUE(subrow->Valid());

                       if (false == subrow->Valid()) { return false; }

                       EXPECT_FALSE(subrow->AccountID().empty());
                       EXPECT_EQ(subrow->Balance(), 0);
                       EXPECT_EQ(subrow->DisplayBalance(), "dollars 0.00");
                       EXPECT_FALSE(subrow->AccountID().empty());
                       EXPECT_TRUE(subrow->Last());

                       chris_.SetAccount("USD", subrow->AccountID());
                   }

                   EXPECT_TRUE(row->Last());

                   return true;
               }},
              {2,
               []() -> bool {
                   const auto& widget = chris_.api_->UI().AccountSummary(
                       chris_.nym_id_, ot::proto::CITEMTYPE_USD);
                   auto row = widget.First();

                   EXPECT_TRUE(row->Valid());

                   if (false == row->Valid()) { return false; }

                   EXPECT_TRUE(row->ConnectionState());
                   EXPECT_EQ(row->Name(), issuer_data_.new_notary_name_);
                   EXPECT_TRUE(row->Trusted());

                   {
                       const auto subrow = row->First();

                       EXPECT_TRUE(subrow->Valid());

                       if (false == subrow->Valid()) { return false; }

                       EXPECT_FALSE(subrow->AccountID().empty());
                       EXPECT_EQ(subrow->Balance(), 0);
                       EXPECT_EQ(subrow->DisplayBalance(), "dollars 0.00");
                       EXPECT_FALSE(subrow->AccountID().empty());
                       EXPECT_TRUE(subrow->Last());
                   }

                   EXPECT_TRUE(row->Last());

                   return true;
               }},
          }},
     }},
};

TEST_F(Test_Pair, init_ot) {}

TEST_F(Test_Pair, init_ui)
{
    {
        ot::Lock lock{cb_chris_.callback_lock_};
        cb_chris_.RegisterWidget(
            lock,
            Widget::AccountSummaryUSD,
            api_chris_.UI()
                .AccountSummary(chris_.nym_id_, ot::proto::CITEMTYPE_USD)
                .WidgetID());

        EXPECT_EQ(1, cb_chris_.Count());
    }

    EXPECT_TRUE(state_.at(chris_.name_).at(Widget::AccountSummaryUSD).at(0)());
}

TEST_F(Test_Pair, issue_dollars)
{
    const auto contract = api_issuer_.Wallet().UnitDefinition(
        issuer_.nym_id_->str(),
        UNIT_DEFINITION_CONTRACT_NAME,
        UNIT_DEFINITION_TERMS,
        UNIT_DEFINITION_PRIMARY_UNIT_NAME,
        UNIT_DEFINITION_SYMBOL,
        UNIT_DEFINITION_TLA,
        UNIT_DEFINITION_POWER,
        UNIT_DEFINITION_FRACTIONAL_UNIT_NAME,
        UNIT_DEFINITION_UNIT_OF_ACCOUNT,
        issuer_.Reason());

    EXPECT_EQ(UNIT_DEFINITION_CONTRACT_VERSION, contract->Version());
    EXPECT_EQ(ot::proto::UNITTYPE_CURRENCY, contract->Type());
    EXPECT_EQ(UNIT_DEFINITION_UNIT_OF_ACCOUNT, contract->UnitOfAccount());
    EXPECT_TRUE(unit_id_->empty());

    unit_id_->Assign(contract->ID());

    EXPECT_FALSE(unit_id_->empty());

    {
        auto issuer =
            api_issuer_.Wallet().mutable_Nym(issuer_.nym_id_, issuer_.Reason());
        issuer.AddPreferredOTServer(
            server_1_.id_->str(), true, issuer_.Reason());
    }

    auto task = api_issuer_.OTX().IssueUnitDefinition(
        issuer_.nym_id_, server_1_.id_, unit_id_, ot::proto::CITEMTYPE_USD);
    auto& [taskID, future] = task;
    const auto result = future.get();

    EXPECT_NE(0, taskID);
    EXPECT_EQ(ot::proto::LASTREPLYSTATUS_MESSAGESUCCESS, result.first);
    ASSERT_TRUE(result.second);

    EXPECT_TRUE(issuer_.SetAccount(
        UNIT_DEFINITION_TLA, result.second->m_strAcctID->Get()));
    EXPECT_FALSE(issuer_.Account(UNIT_DEFINITION_TLA).empty());

    api_issuer_.OTX().ContextIdle(issuer_.nym_id_, server_1_.id_).get();

    {
        const auto pNym = api_issuer_.Wallet().Nym(issuer_.nym_id_);

        ASSERT_TRUE(pNym);

        const auto& nym = *pNym;
        const auto& claims = nym.Claims();
        const auto pSection =
            claims.Section(ot::proto::CONTACTSECTION_CONTRACT);

        ASSERT_TRUE(pSection);

        const auto& section = *pSection;
        const auto pGroup = section.Group(ot::proto::CITEMTYPE_USD);

        ASSERT_TRUE(pGroup);

        const auto& group = *pGroup;
        const auto& pClaim = group.PrimaryClaim();

        EXPECT_EQ(1, group.Size());
        ASSERT_TRUE(pClaim);

        const auto& claim = *pClaim;

        EXPECT_EQ(claim.Value(), unit_id_->str());
    }
}

TEST_F(Test_Pair, pair_untrusted)
{
    auto future = cb_chris_.SetCallback(
        Widget::AccountSummaryUSD,
        5,
        state_.at(chris_.name_).at(Widget::AccountSummaryUSD).at(1));

    ASSERT_TRUE(
        api_chris_.Pair().AddIssuer(chris_.nym_id_, issuer_.nym_id_, ""));
    EXPECT_TRUE(issuer_data_.bailment_.get());

    api_chris_.Pair().Wait().get();

    {
        const auto pIssuer =
            api_chris_.Wallet().Issuer(chris_.nym_id_, issuer_.nym_id_);

        ASSERT_TRUE(pIssuer);

        const auto& issuer = *pIssuer;

        EXPECT_EQ(
            1, issuer.AccountList(ot::proto::CITEMTYPE_USD, unit_id_).size());
        EXPECT_FALSE(issuer.BailmentInitiated(unit_id_));
        EXPECT_EQ(3, issuer.BailmentInstructions(unit_id_).size());
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITCOIN).size(), 0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BTCRPC).size(), 0);
        EXPECT_EQ(

            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITMESSAGE).size(),
            0);
        EXPECT_EQ(

            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITMESSAGERPC)
                .size(),
            0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_SSH).size(), 0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_CJDNS).size(), 0);
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_BITCOIN));
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_BTCRPC));
        EXPECT_FALSE(issuer.ConnectionInfoInitiated(
            ot::proto::CONNECTIONINFO_BITMESSAGE));
        EXPECT_FALSE(issuer.ConnectionInfoInitiated(
            ot::proto::CONNECTIONINFO_BITMESSAGERPC));
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_SSH));
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_CJDNS));
        EXPECT_EQ(issuer_.nym_id_, issuer.IssuerID());
        EXPECT_EQ(chris_.nym_id_, issuer.LocalNymID());
        EXPECT_FALSE(issuer.Paired());
        EXPECT_TRUE(issuer.PairingCode().empty());
        EXPECT_EQ(server_1_.id_, issuer.PrimaryServer());
        EXPECT_FALSE(issuer.StoreSecretComplete());
        EXPECT_FALSE(issuer.StoreSecretInitiated());
    }

    EXPECT_TRUE(test_future(future));
}

TEST_F(Test_Pair, pair_trusted)
{
    auto future = cb_chris_.SetCallback(
        Widget::AccountSummaryUSD,
        7,
        state_.at(chris_.name_).at(Widget::AccountSummaryUSD).at(2));

    ASSERT_TRUE(api_chris_.Pair().AddIssuer(
        chris_.nym_id_, issuer_.nym_id_, server_1_.password_));
    EXPECT_TRUE(test_future(future));

    api_chris_.Pair().Wait().get();

    {
        const auto pIssuer =
            api_chris_.Wallet().Issuer(chris_.nym_id_, issuer_.nym_id_);

        ASSERT_TRUE(pIssuer);

        const auto& issuer = *pIssuer;

        EXPECT_EQ(
            1, issuer.AccountList(ot::proto::CITEMTYPE_USD, unit_id_).size());
        EXPECT_FALSE(issuer.BailmentInitiated(unit_id_));
        EXPECT_EQ(3, issuer.BailmentInstructions(unit_id_).size());
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITCOIN).size(), 0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BTCRPC).size(), 0);
        EXPECT_EQ(

            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITMESSAGE).size(),
            0);
        EXPECT_EQ(

            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_BITMESSAGERPC)
                .size(),
            0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_SSH).size(), 0);
        EXPECT_EQ(
            issuer.ConnectionInfo(ot::proto::CONNECTIONINFO_CJDNS).size(), 0);
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_BITCOIN));
        EXPECT_TRUE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_BTCRPC));
        EXPECT_FALSE(issuer.ConnectionInfoInitiated(
            ot::proto::CONNECTIONINFO_BITMESSAGE));
        EXPECT_FALSE(issuer.ConnectionInfoInitiated(
            ot::proto::CONNECTIONINFO_BITMESSAGERPC));
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_SSH));
        EXPECT_FALSE(
            issuer.ConnectionInfoInitiated(ot::proto::CONNECTIONINFO_CJDNS));
        EXPECT_EQ(issuer_.nym_id_, issuer.IssuerID());
        EXPECT_EQ(chris_.nym_id_, issuer.LocalNymID());
        EXPECT_TRUE(issuer.Paired());
        EXPECT_EQ(issuer.PairingCode(), server_1_.password_);
        EXPECT_EQ(server_1_.id_, issuer.PrimaryServer());
        EXPECT_FALSE(issuer.StoreSecretComplete());
#if OT_CRYPTO_WITH_BIP32
        EXPECT_TRUE(issuer.StoreSecretInitiated());
#else
        EXPECT_FALSE(issuer.StoreSecretInitiated());
#endif  // OT_CRYPTO_WITH_BIP32
    }
}

TEST_F(Test_Pair, shutdown)
{
    api_issuer_.OTX().ContextIdle(issuer_.nym_id_, server_1_.id_).get();
    api_chris_.OTX().ContextIdle(chris_.nym_id_, server_1_.id_).get();
}
}  // namespace
