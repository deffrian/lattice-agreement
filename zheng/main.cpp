#include <fstream>
#include <cassert>

#include "zheng_la.h"

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
    if (argc != 7) {
        std::cout << "usage: n f port id elem config" << std::endl;
        assert(false);
    }
    ProtocolTcp<LatticeSet> protocol(std::stoi(argv[3]));
    read_processes_from_config(argv[6], protocol, std::stoi(argv[4]));
    ZhengLA<LatticeSet> la(std::stoi(argv[2]), std::stoi(argv[1]), std::stoi(argv[5]), protocol);
    std::cout << "Start server. port: " << std::stoi(argv[3]) << std::endl;
    protocol.start(&la);
    std::this_thread::sleep_for(std::chrono::seconds(20));
    std::cout << "Server started" << std::endl;

    LatticeSet s;
    s.insert(std::stoi(argv[5]));
    auto y = la.start(s);
    std::cout << "Answer: " << std::endl;
    for (auto elem : y.set) {
        std::cout << elem <<  ' ';
    }
    std::cout << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(30));
}