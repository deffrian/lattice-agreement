#include "generalizer.h"
#include "general/lattice.h"
#include "zheng/generator.h"

int main(int argc, char *argv[]) {
    Protocol<LatticeSet> protocol(8090);
    ZhengLAGenerator<LatticeSet> la_gen;
    Generalizer<LatticeSet, ZhengLAGenerator<LatticeSet>> la(protocol, la_gen, 8, 4);

    protocol.start(&la);

    LatticeSet s1;
    s1.insert(1);
    la.propose(s1);
    LatticeSet s2;
    s2.insert(2);
    la.propose(s2);
}