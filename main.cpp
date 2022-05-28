#include <iostream>

#include "general/network.h"
#include "general/net/server.h"

#include <asio.hpp>

struct Callback : net::IMessageReceivedCallback {
    void on_message_received(net::Message &message) override {
        uint64_t v;
        message >> v;
        LOG(ERROR) << v;
    }
};

int main(int argc, char *argv[]) {
    if (argc == 2) {
        Callback callback;
        net::Server server(&callback, 8000);
//        std::this_thread::sleep_for(std::chrono::seconds(10));
        server.start();
        LOG(INFO) << "STARTED";
        std::this_thread::sleep_for(std::chrono::seconds(100));
        server.stop();
    } else {
        net::Message message;
        message << (uint64_t)228;
        net::ProcessDescriptor descriptor{"127.0.0.1", 0, 8000};
        asio::io_context context;
        auto connection = std::make_shared<net::WriteConnection>(context, descriptor);
        connection->send(message);
//        connection->send(message);
        std::thread([&]() {
            context.run();
        }).detach();
        LOG(INFO) << "SENT";
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}
