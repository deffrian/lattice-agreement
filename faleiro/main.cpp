#include <iostream>

#include "lattice.h"
#include "acceptor.h"
#include "proposer.h"

int main() {
    AcceptorProtocolTcp<LatticeSet> acceptor_protocol;
    Acceptor<LatticeSet, AcceptorProtocolTcp<LatticeSet>> acceptor1(acceptor_protocol);
    Acceptor<LatticeSet, AcceptorProtocolTcp<LatticeSet>> acceptor2(acceptor_protocol);
    g_acceptors.push_back(&acceptor1);
    g_acceptors.push_back(&acceptor2);
    LatticeSet set1;
    LatticeSet set2;
    set1.insert(3);
    set2.insert(2);

    std::cout << "here" << std::endl;
    ProposerProtocolTcp<LatticeSet> protocol;
    protocol.add_acceptor(AcceptorDescriptor{"", acceptor1.get_id()});
    protocol.add_acceptor(AcceptorDescriptor{"", acceptor2.get_id()});

    Proposer<LatticeSet, ProposerProtocolTcp<LatticeSet>> proposer1(protocol);
    Proposer<LatticeSet, ProposerProtocolTcp<LatticeSet>> proposer2(protocol);

    auto res1 = proposer1.get_value(set1);
    std::cout << "res1 " << res1.set.size() << " " << *res1.set.begin() << std::endl;
    std::cout << "first done" << std::endl;
    auto res2 = proposer2.get_value(set2);
    std::cout << "res2 " << res2.set.size() << ' ' << *res2.set.begin() << ' ' << *(++res2.set.begin()) << std::endl;
//    acceptor.process_proposal(0, set, 0);
}
