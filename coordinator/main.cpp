
#include "la_coordinator.h"
#include "general/lattice.h"

int main() {
    LACoordinator<LatticeSet> coordinator(4, 1, 8090);

    std::cout << "Done" << std::endl;
}