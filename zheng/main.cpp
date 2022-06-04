#include <fstream>

#include "zheng_la.h"
#include "coordinator/la_coordinator.h"

template<typename L>
void read_processes_from_config(const std::string &acceptors_config, ProtocolTcp<L> &protocol, uint64_t my_id) {
    std::ifstream s(acceptors_config);
    std::string ip;
    uint64_t port;
    uint64_t id;
    while ((s >> ip) && (s >> port) && (s >> id)) {
        if (my_id != id) {
            protocol.add_process({ip, id, port});
        }
    }
    s.close();
}

int main(int argc, char *argv[]) {
    try {
        if (argc != 6) {
            std::cout << "usage: ip port coordinator_port coordinator_ip coordinator_client_port" << std::endl;
            throw std::runtime_error("usage");
        }

        std::string ip = argv[1];
        uint64_t port = std::stoi(argv[2]);
        uint64_t coordinator_port = std::stoi(argv[3]);
        uint64_t coordinator_client_port = std::stoi(argv[5]);
        ProcessDescriptor coordinator_descriptor{argv[4], 10, coordinator_port};
        LACoordinatorClient<LatticeSet> coordinator_client(coordinator_client_port, coordinator_descriptor);

        // Register self
        uint64_t id = coordinator_client.send_register(port, coordinator_client_port, ip);
        uint64_t n;
        uint64_t f;
        LatticeSet initial_value;
        std::vector<ProcessDescriptor> peers;

        // Receive test info
        coordinator_client.wait_for_test_info(n, f, initial_value, peers);

        LOG(INFO) << "Starting protocol" << port << id;
        // Setup server
        ProtocolTcp<LatticeSet> protocol(port, id);

        for (const auto &item: peers) {
            if (id != item.id) {
                protocol.add_process({item.ip_address, item.id, item.port});
            }
        }

        ZhengLA<LatticeSet> la(f, n, id, protocol);

        // Starting server
        std::cout << "Start server. port: " << port << std::endl;
        protocol.start(&la);

        // Wait for start signal
        coordinator_client.wait_for_start();

        LOG(INFO) << "Run la";
        std::this_thread::sleep_for(std::chrono::seconds(2));
//    std::this_thread::sleep_for(std::chrono::seconds(id));
//    if (id > 20) {
//        std::this_thread::sleep_for(std::chrono::seconds(240));
//    }

        // Run la
        auto begin = std::chrono::steady_clock::now();
        auto y = la.start(initial_value);
        auto end = std::chrono::steady_clock::now();
        uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

        LOG(INFO) << "DONE" << id;

        std::cout << "Answer: " << std::endl;

        for (auto elem: y.set) {
            std::cout << elem << ' ';
        }
        std::cout << std::endl;
        std::cout << "Elapsed microseconds: " +
                     std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) + '\n';

        // Sending results
        LOG(INFO) << "Sending results";
        coordinator_client.send_test_complete(elapsed_time, y);

        // Wait before stopping protocol
        coordinator_client.wait_for_stop();
        protocol.stop();
    } catch (std::exception &e) {
        LOG(ERROR) << "EXCEPTION" << e.what();
    }
}
