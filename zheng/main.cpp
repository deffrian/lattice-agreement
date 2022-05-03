#include <fstream>
#include <cassert>

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
    if (argc != 5) {
        std::cout << "usage: port coordinator_port coordinator_ip coordinator_client_port" << std::endl;
        assert(false);
    }

    uint64_t port = std::stoi(argv[1]);
    uint64_t coordinator_port = std::stoi(argv[2]);
    uint64_t coordinator_client_port = std::stoi(argv[4]);
    ProcessDescriptor coordinator_descriptor{argv[3], (uint64_t)-1, coordinator_port};
    LACoordinatorClient<LatticeSet> coordinator_client(coordinator_client_port, coordinator_descriptor);

    // Register self
    uint64_t id = coordinator_client.send_register(port);
    uint64_t n;
    uint64_t f;
    LatticeSet initial_value;
    std::vector<ProcessDescriptor> peers;

    // Receive test info
    coordinator_client.wait_for_test_info(n, f, initial_value, peers);

    // Setup server
    ProtocolTcp<LatticeSet> protocol(port, id);

    for (const auto &item: peers) {
        protocol.add_process(item);
    }

    ZhengLA<LatticeSet> la(f, n, id, protocol);

    // Starting server
    std::cout << "Start server. port: " << port << std::endl;
    protocol.start(&la);

    // Wait for start signal
    coordinator_client.wait_for_start();

    // Run la
    auto begin = std::chrono::steady_clock::now();
    auto y = la.start(initial_value);
    auto end = std::chrono::steady_clock::now();
    uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

    // Sending results
    coordinator_client.send_test_complete(elapsed_time, y);

    std::cout << "Answer: " << std::endl;

    for (auto elem: y.set) {
        std::cout << elem << ' ';
    }
    std::cout << std::endl;
    std::cout << "Elapsed microseconds: " + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) + '\n';

    // Wait before stopping protocol
    coordinator_client.wait_for_stop();
    protocol.stop();
}

int main2(int argc, char *argv[]) {
    if (argc != 7) {
        std::cout << "usage: n f port id elem config" << std::endl;
        assert(false);
    }
    uint64_t n = std::stoi(argv[1]);
    uint64_t f = std::stoi(argv[2]);
    uint64_t port = std::stoi(argv[3]);
    uint64_t id = std::stoi(argv[4]);
    uint64_t elem = std::stoi(argv[5]);
    std::string config = argv[6];
    ProtocolTcp<LatticeSet> protocol(port, id);
    read_processes_from_config(config, protocol, id);
    ZhengLA<LatticeSet> la(f, n, id, protocol);
    std::cout << "Start server. port: " << port << std::endl;
    std::cout << "Callback: " << &la << std::endl;
    protocol.start(&la);
    std::cout << "Server started" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::cout << "process id: " << id << std::endl;
    LatticeSet s;
    s.insert(elem);
    auto begin = std::chrono::steady_clock::now();
    auto y = la.start(s);
    auto end = std::chrono::steady_clock::now();
    std::cout << "Answer: " << std::endl;
    for (auto elem : y.set) {
        std::cout << elem <<  ' ';
    }
    std::cout << std::endl;
    std::cout << "Elapsed microseconds: " + std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count()) + '\n';
    std::this_thread::sleep_for(std::chrono::seconds(60));
    std::cout.flush();
    protocol.stop();
}