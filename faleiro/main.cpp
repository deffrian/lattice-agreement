#include <iostream>
#include <fstream>

#include "lattice.h"
#include "acceptor.h"
#include "proposer.h"

template<typename L>
void read_acceptors_from_config(const std::string &acceptors_config, ProposerProtocolTcp<L> &protocol) {
    std::ifstream s(acceptors_config);
    std::string ip;
    uint64_t port;
    uint64_t id;
    while ((s >> ip) && (s >> port) && (s >> id)) {
        protocol.add_acceptor({ip, id, port});
    }
    s.close();
}

template<typename L>
void read_set_from_config(const std::string &set_config, L &set) {
    std::ifstream s(set_config);
    uint64_t elem;
    while (s >> elem) {
        set.insert(elem);
    }
    s.close();
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "wrong args" << std::endl;
        std::cout << "s port id" << std::endl;
        std::cout << "c acceptors_cfg set_cfg" << std::endl;
        return -1;
    }
    if (argv[1][0] == 's') {
        AcceptorProtocolTcp<LatticeSet> acceptor_protocol;
        Acceptor<LatticeSet> acceptor1(std::stoi(argv[3]));
        acceptor_protocol.start(acceptor1, std::stoi(argv[2]));
    } else {
        ProposerProtocolTcp<LatticeSet> protocol;
        LatticeSet s;
        read_acceptors_from_config(argv[2], protocol);
        read_set_from_config(argv[3], s);
        Proposer<LatticeSet, ProposerProtocolTcp<LatticeSet>> proposer(protocol);
        auto result = proposer.start(s);
        for (auto elem : result.set) {
            std::cout << elem << ' ';
        }
        std::cout << std::endl;
    }
}

