#include <iostream>
#include <fstream>

#include "coordinator_generalized/gla_coordinator.h"
#include "general/lattice.h"
#include "acceptor.h"
#include "proposer.h"
#include "learner.h"

int main(int argc, char *argv[]) {
    if (argc != 6) {
        LOG(ERROR) << "usage: ip port coordinator_port coordinator_ip coordinator_client_port";
        throw std::runtime_error("usage");
    }

    std::string ip = argv[1];
    uint64_t port = std::stoi(argv[2]);
    uint64_t coordinator_port = std::stoi(argv[3]);
    uint64_t coordinator_client_port = std::stoi(argv[5]);
    ProcessDescriptor coordinator_descriptor{argv[4], 10, coordinator_port};
    GLACoordinatorClient<LatticeSet> coordinator_client(coordinator_client_port, coordinator_descriptor);

    // Register self
    uint64_t id = coordinator_client.send_register(port, coordinator_client_port, ip);
    uint64_t n;
    uint64_t f;
    std::vector<LatticeSet> initial_value;
    std::vector<ProcessDescriptor> peers;

    // Receive test info
    coordinator_client.wait_for_test_info(n, f, initial_value, peers);

    LOG(INFO) << "Starting protocol" << port << id;
    // Setup server
    FaleiroProtocol<LatticeSet> protocol(port);

    for (const auto &item: peers) {
        protocol.add_process({item.ip_address, item.id, item.port});
    }

    Acceptor<LatticeSet> acceptor(protocol);
    Proposer<LatticeSet> proposer(protocol, id, n);
    Learner<LatticeSet> learner(protocol, n);

    // Starting server
    std::cout << "Start server. port: " << port << std::endl;
    protocol.start(&acceptor, &proposer, &learner);

    // Wait for start signal
    coordinator_client.wait_for_start();

    LOG(INFO) << "Run la";
    std::this_thread::sleep_for(std::chrono::seconds(10));

    // Run la
    auto begin = std::chrono::steady_clock::now();

    std::vector<LatticeSet> results;
    for (const auto &elem : initial_value) {
        proposer.receive_value(elem);
        proposer.start();
        auto y = learner.learn_value(elem);
        results.push_back(y);
    }

    auto end = std::chrono::steady_clock::now();
    uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

    LOG(INFO) << "DONE" << id;

    LOG(INFO) << "Answer" << results;
    LOG(INFO) << "Elapsed microseconds:" << elapsed_time;

    // Sending results
    LOG(INFO) << "Sending results";
    coordinator_client.send_test_complete(elapsed_time, results);

    // Wait before stopping protocol
    coordinator_client.wait_for_stop();
    protocol.stop();
}

