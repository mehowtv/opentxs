// Copyright (c) 2010-2019 The Open-Transactions developers
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include "OTTestEnvironment.hpp"

using namespace opentxs;

TEST(Message, Factory)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    ASSERT_NE(nullptr, &multipartMessage.get());
}

TEST(Message, AddFrame)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    auto& message = multipartMessage->AddFrame();
    ASSERT_EQ(1, multipartMessage->size());
    ASSERT_NE(nullptr, message.data());
    ASSERT_EQ(message.size(), 0);
}

TEST(Message, AddFrame_Data)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    auto& message = multipartMessage->AddFrame(Data::Factory("testString", 10));
    ASSERT_EQ(multipartMessage->size(), 1);
    ASSERT_NE(nullptr, message.data());
    ASSERT_EQ(message.size(), 10);

    std::string messageString = message;
    ASSERT_STREQ("testString", messageString.c_str());
}

TEST(Message, AddFrame_string)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    auto& message = multipartMessage->AddFrame(std::string{"testString"});
    ASSERT_EQ(multipartMessage->size(), 1);
    ASSERT_NE(nullptr, message.data());
    ASSERT_EQ(message.size(), 10);

    std::string messageString = message;
    ASSERT_STREQ("testString", messageString.c_str());
}

TEST(Message, at)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame(std::string{"msg3"});

    auto& message = multipartMessage->at(0);
    std::string messageString = message;
    ASSERT_STREQ("msg1", messageString.c_str());

    network::zeromq::Frame& message2 = multipartMessage->at(1);
    messageString = message2;
    ASSERT_STREQ("msg2", messageString.c_str());

    network::zeromq::Frame& message3 = multipartMessage->at(2);
    messageString = message3;
    ASSERT_STREQ("msg3", messageString.c_str());
}

TEST(Message, at_const)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame(std::string{"msg3"});

    const auto& message = multipartMessage->at(0);
    std::string messageString = message;
    ASSERT_STREQ("msg1", messageString.c_str());

    const network::zeromq::Frame& message2 = multipartMessage->at(1);
    messageString = message2;
    ASSERT_STREQ("msg2", messageString.c_str());

    const network::zeromq::Frame& message3 = multipartMessage->at(2);
    messageString = message3;
    ASSERT_STREQ("msg3", messageString.c_str());
}

TEST(Message, begin)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    network::zeromq::FrameIterator it = multipartMessage->begin();
    ASSERT_EQ(multipartMessage->end(), it);
    ASSERT_EQ(std::distance(it, multipartMessage->end()), 0);

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame(std::string{"msg3"});

    ASSERT_NE(multipartMessage->end(), it);
    ASSERT_EQ(std::distance(it, multipartMessage->end()), 3);

    std::advance(it, 3);
    ASSERT_EQ(multipartMessage->end(), it);
    ASSERT_EQ(std::distance(it, multipartMessage->end()), 0);
}

TEST(Message, Body)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    const network::zeromq::FrameSection bodySection = multipartMessage->Body();
    ASSERT_EQ(bodySection.size(), 2);

    const auto& message = bodySection.at(1);
    std::string msgString = message;
    ASSERT_STREQ("msg4", msgString.c_str());
}

TEST(Message, Body_at)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    const auto& message = multipartMessage->Body_at(1);
    std::string msgString = message;
    ASSERT_STREQ("msg4", msgString.c_str());
}

TEST(Message, Body_begin)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    network::zeromq::FrameIterator bodyBegin = multipartMessage->Body_begin();
    auto body = multipartMessage->Body();
    ASSERT_EQ(body.begin(), bodyBegin);
}

TEST(Message, Body_end)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    network::zeromq::FrameIterator bodyEnd = multipartMessage->Body_end();
    auto body = multipartMessage->Body();
    ASSERT_EQ(body.end(), bodyEnd);
}

TEST(Message, end)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    network::zeromq::FrameIterator it = multipartMessage->end();
    ASSERT_EQ(multipartMessage->begin(), it);
    ASSERT_EQ(std::distance(multipartMessage->begin(), it), 0);

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame(std::string{"msg3"});

    network::zeromq::FrameIterator it2 = multipartMessage->end();
    ASSERT_NE(multipartMessage->begin(), it2);
    ASSERT_EQ(
        std::distance(multipartMessage->begin(), it2), 3);
}

TEST(Message, EnsureDelimiter)
{
    // Empty message.
    auto message = network::zeromq::Message::Factory();

    ASSERT_EQ(message->size(), 0);

    message->EnsureDelimiter();  // Adds delimiter.

    ASSERT_EQ(message->size(), 1);

    message->AddFrame();

    ASSERT_EQ(message->size(), 2);

    message->EnsureDelimiter();  // Doesn't add delimiter

    ASSERT_EQ(message->size(), 2);

    // Message body only.
    auto message2 = Context().ZMQ().Message(std::string{"msg"});

    ASSERT_EQ(static_cast<std::size_t>(1), message2->size());

    message2->EnsureDelimiter();  // Inserts delimiter.

    ASSERT_EQ(message2->size(), 2);
    ASSERT_EQ(message2->Header().size(), 0);
    ASSERT_EQ(message2->Body().size(), 1);

    message2->EnsureDelimiter();  // Doesn't add delimiter.

    ASSERT_EQ(message2->size(), 2);
    ASSERT_EQ(message2->Header().size(), 0);
    ASSERT_EQ(message2->Body().size(), 1);

    // Header and message body.
    auto message3 = Context().ZMQ().Message(std::string{"header"});
    message3->AddFrame();
    message3->AddFrame(std::string{"body"});

    ASSERT_EQ(message3->size(), 3);
    ASSERT_EQ(message3->Header().size(), 1);
    ASSERT_EQ(message3->Body().size(), 1);

    message3->EnsureDelimiter();  // Doesn't add delimiter.

    ASSERT_EQ(message3->size(), 3);
    ASSERT_EQ(message3->Header().size(), 1);
    ASSERT_EQ(message3->Body().size(), 1);

    // Message body with 2 frames.
    auto message4 = Context().ZMQ().Message(std::string{"frame1"});
    message4->AddFrame(std::string{"frame2"});

    ASSERT_EQ(message4->size(), 2);
    ASSERT_EQ(message4->Header().size(), 0);
    ASSERT_EQ(message4->Body().size(), 2);

    message4->EnsureDelimiter();

    ASSERT_EQ(message4->size(), 3);
    ASSERT_EQ(message4->Header().size(), 1);
    ASSERT_EQ(message4->Body().size(), 1);
}

TEST(Message, Header)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    network::zeromq::FrameSection headerSection = multipartMessage->Header();
    ASSERT_EQ(headerSection.size(), 2);

    const auto& message = headerSection.at(1);
    std::string msgString = message;
    ASSERT_STREQ("msg2", msgString.c_str());
}

TEST(Message, Header_at)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    const auto& message = multipartMessage->Header_at(1);
    std::string msgString = message;
    ASSERT_STREQ("msg2", msgString.c_str());
}

TEST(Message, Header_begin)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    network::zeromq::FrameIterator headerBegin =
        multipartMessage->Header_begin();
    auto header = multipartMessage->Header();
    ASSERT_EQ(header.begin(), headerBegin);
}

TEST(Message, Header_end)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame();
    multipartMessage->AddFrame(std::string{"msg3"});
    multipartMessage->AddFrame(std::string{"msg4"});

    network::zeromq::FrameIterator headerEnd = multipartMessage->Header_end();
    auto header = multipartMessage->Header();
    ASSERT_EQ(header.end(), headerEnd);
}

TEST(Message, PrependEmptyFrame)
{
    auto message = Context().ZMQ().Message(std::string("msg body"));

    ASSERT_EQ(message->size(), 1);

    message->PrependEmptyFrame();

    ASSERT_EQ(message->size(), 2);
    ASSERT_EQ(message->Header().size(), 0);
    ASSERT_EQ(message->Body().size(), 1);

    auto message2 = network::zeromq::Message::Factory();

    message2->AddFrame(std::string{"msg header"});
    message2->AddFrame();
    message2->AddFrame(std::string{"msg body"});

    ASSERT_EQ(message2->size(), 3);
    ASSERT_EQ(message2->Header().size(), 1);
    ASSERT_EQ(message2->Body().size(), 1);

    message2->PrependEmptyFrame();

    ASSERT_EQ(message2->size(), 4);
    ASSERT_EQ(message2->Header().size(), 0);
    ASSERT_EQ(message2->Body().size(), 3);
}

TEST(Message, size)
{
    auto multipartMessage = network::zeromq::Message::Factory();

    std::size_t size = multipartMessage->size();
    ASSERT_EQ(size, 0);

    multipartMessage->AddFrame(std::string{"msg1"});
    multipartMessage->AddFrame(std::string{"msg2"});
    multipartMessage->AddFrame(std::string{"msg3"});

    size = multipartMessage->size();
    ASSERT_EQ(size, 3);
}
