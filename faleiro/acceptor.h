#pragma once

#include "lattice.h"
#include "protocol.h"
#include "network.h"

template<typename L>
struct Acceptor {
    L accepted_value;
    uint64_t uid;

    explicit Acceptor(uint64_t uid) : uid(uid) {}

    uint64_t get_id() const {
        return uid;
    }

    AcceptorResponse<L> process_proposal(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        if (accepted_value <= proposed_value) {
            return accept(proposal_number, proposed_value, proposer_id);
        } else {
            return reject(proposal_number, proposed_value, proposer_id);
        }
    }

    // guard: ?Proposal(proposalNumber, proposedValue, proposerId) && acceptedValue <= proposedValue
    AcceptorResponse<L> accept(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        accepted_value = proposed_value;
        return Ack{proposal_number, proposed_value, proposer_id};
    }

    // guard: ?Proposal(proposalNumber, proposedValue, proposerId) && acceptedValue !<= proposedValue
    AcceptorResponse<L> reject(uint64_t proposal_number, const L &proposed_value, uint64_t proposer_id) {
        accepted_value = L::join(accepted_value, proposed_value);
        return Nack{proposal_number, accepted_value, proposer_id};
    }
};

template<typename L>
struct AcceptorProtocolTcp {
    void start(Acceptor<L> &acceptor, uint64_t port) {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            exit(EXIT_FAILURE);
        }

        if (setsockopt(server_fd, SOL_SOCKET,SO_REUSEADDR | SO_REUSEPORT, &opt,sizeof(opt))) {
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 4) < 0) {
            exit(EXIT_FAILURE);
        }
        while (true) {
            new_socket = accept(server_fd, (struct sockaddr *) &address,(socklen_t *) &addrlen);
            if (new_socket < 0) {
                exit(EXIT_FAILURE);
            }
            std::cout << "Connection accepted" << std::endl;
            process_client(new_socket, acceptor);
            std::cout << "Closing connection" << std::endl;
            close(new_socket);
        }
    }

    void process_client(int clientFd, Acceptor<L> &acceptor) {
        uint64_t proposal_number = read_number(clientFd);
        std::cout << "Proposal number: " << proposal_number << std::endl;
        uint64_t proposer_id = read_number(clientFd);
        std::cout << "Proposer id: " << proposer_id << std::endl;
        auto set = read_lattice<LatticeSet>(clientFd);

        auto res = acceptor.process_proposal(proposal_number, set, proposer_id);
        uint8_t isAck = std::holds_alternative<Ack<L>>(res);
        send(clientFd, &isAck, 1, 0);
        send_number(clientFd, proposal_number);
        send_number(clientFd, proposer_id);
        auto result_set = isAck ? std::get<Ack<L>>(res).proposed_value : std::get<Nack<L>>(res).proposed_value;
        std::cout << "sending size: " << result_set.set.size() << std::endl;
        send_number(clientFd, result_set.set.size());

        for (uint64_t elem : result_set.set) {
            std::cout << "sending elem: " << elem << std::endl;
            send_number(clientFd, elem);
        }
//        send(clientFd, (void*)result_set.data(), result_set.size() * 8, 0);
    }
};
