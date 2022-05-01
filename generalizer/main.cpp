#include "generalizer.h"
#include "lattice.h"

int main(int argc, char *argv[]) {

    Protocol<LatticeSet> protocol(8090);
    Generalizer<LatticeSet> la(protocol);

    protocol.start(&la);

    LatticeSet s1;
    s1.insert(1);
    la.propose(s1);
    LatticeSet s2;
    s2.insert(2);
    la.propose(s2);
}