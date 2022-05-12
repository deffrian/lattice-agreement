
#include "gla_coordinator.h"
#include "general/lattice.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cout << "usage n f port" << std::endl;
        throw std::runtime_error("usage");
    }
    uint64_t n = std::stoi(argv[1]);
    uint64_t f = std::stoi(argv[2]);
    uint64_t port = std::stoi(argv[3]);
    GLACoordinator<LatticeSet> coordinator(n, f, port);

    std::cout << "Done" << std::endl;
}