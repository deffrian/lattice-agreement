#pragma once

#include <optional>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>

#include "lattice.h"
#include "acceptor.h"

enum Status {
    Passive,
    Active,
};

template<typename L, typename P>
struct Proposer {
    uint64_t uid;
    Status status;
    uint64_t ack_count;
    uint64_t nack_count;
    uint64_t active_proposal_number;
    L proposed_value;
    P &protocol;


    explicit Proposer(P &protocol) : protocol(protocol), status(Passive), ack_count(0), nack_count(0),
                                     active_proposal_number(0) {
        uid = rand();
    }

    L get_value(L &initial_value) {
        propose(initial_value);
        while (true) {
            auto result = decide();
            if (result.has_value()) {
                return result.value();
            } else {
                refine();
            }
        }
    }

    uint64_t get_uid() {
        return uid;
    }

    void propose(L &initial_value) {
        if (active_proposal_number == 0) {
            proposed_value = initial_value;
            status = Status::Active;
            active_proposal_number += 1;
            protocol.send_proposal(ProposerRequest<L>(proposed_value, active_proposal_number, uid), *this);
        }
    }

    void process_ack(uint64_t proposal_number) {
        if (proposal_number == active_proposal_number) {
            ack_count += 1;
        }
    }

    void process_nack(uint64_t proposal_number, L &value) {
        if (proposal_number == active_proposal_number) {
            proposed_value = LatticeSet::join(proposed_value, value);
            nack_count += 1;
        }
    }

    void refine() {
        uint64_t acc_cnt = protocol.get_acceptors_count();
        if (nack_count > 0 && status == Status::Active &&
            nack_count + ack_count >= (acc_cnt + 2) / 2) {
            active_proposal_number += 1;
            ack_count = 0;
            nack_count = 0;
            protocol.send_proposal(ProposerRequest<L>(proposed_value, active_proposal_number, uid), *this);
        }
    }

    std::optional<L> decide() {
        uint64_t acc_cnt = protocol.get_acceptors_count();
        if (ack_count >= (acc_cnt + 2) / 2 && status == Status::Active) {
            status = Status::Passive;
            return {proposed_value};
        } else {
            return {};
        }
    }
};

template<typename L>
struct ProposerProtocolTcp {
    std::vector<AcceptorDescriptor> acceptors;

    void add_acceptor(const AcceptorDescriptor &acceptor) {
        acceptors.push_back(acceptor);
    }

    void send_proposal(const ProposerRequest<L> &proposal, Proposer<L, ProposerProtocolTcp<L>> &proposer) {
        for (size_t i = 0; i < acceptors.size(); ++i) {
            auto res = get_reply(proposal, acceptors[i]);
            if (std::holds_alternative<Ack<L>>(res)) {
                proposer.process_ack(std::get<Ack<L>>(res).proposal_number);
            } else {
                auto nack = std::get<Nack<L>>(res);
                proposer.process_nack(nack.proposal_number, nack.proposed_value);
            }
        }
    }

    std::variant<Ack<L>, Nack<L>> get_reply(const ProposerRequest<L> &proposal, const AcceptorDescriptor &descriptor) {
        int sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            printf("\n Socket creation error \n");
            exit(EXIT_FAILURE);
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(descriptor.port);

        // Convert IPv4 and IPv6 addresses from text to binary
        // form
        if (inet_pton(AF_INET, descriptor.ip_address.data(), &serv_addr.sin_addr) <= 0) {
            std::cout << "Invalid address" << std::endl;
            exit(EXIT_FAILURE);
        }

        if (connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
            std::cout << "Connection failed: " << descriptor.ip_address << ' ' << descriptor.port << std::endl;
            exit(EXIT_FAILURE);
        }
        std::cout << "Sending request" << std::endl;
        send(sock, &proposal.proposal_number, 64 / 8, 0);
        send(sock, &proposal.proposer_id, 64 / 8, 0);
        uint64_t set_size = proposal.proposed_value.set.size();

        send(sock, &set_size, 64 / 8, 0);
        for (uint64_t elem : proposal.proposed_value.set) {
            send(sock, &elem, 64 / 8, 0);
        }
        std::cout << "Receiving reply" << std::endl;

        uint8_t isAck = -1;
        ssize_t len = read(sock, &isAck, 1);
        if (len != 1) {
            assert(false);
        }
        std::cout << "isAck " << (int)isAck << std::endl;
        uint64_t proposal_number = read_number(sock);
        std::cout << "recv proposal number " << proposal_number << std::endl;
        uint64_t proposer_id = read_number(sock);
        std::cout << "recv proposer id " << proposer_id << std::endl;
        LatticeSet set_recv;
        uint64_t set_size_recv = read_number(sock);
        std::cout << "received set: " << set_size_recv << std::endl;
        for (size_t i = 0; i < set_size_recv; ++i) {
            uint64_t elem = read_number(sock);
            std::cout << "elem: " << elem << std::endl;
            set_recv.insert(elem);
        }
        close(sock);
        if (isAck) {
            return {Ack<L>{proposal_number, set_recv, proposer_id}};
        } else {
            return {Nack<L>{proposal_number, set_recv, proposer_id}};
        }
    }

    uint64_t read_number(int clientFd) {
        uint64_t number;
        size_t bytes_read = 0;

        while (bytes_read != 64 / 8) {
            ssize_t len = read(clientFd, (char*)(&number) + bytes_read, 64 / 8 - bytes_read);
            if (len <= 0) {
                std::cout << "error reading number" << std::endl;
                assert(false);
            }
            bytes_read += len;
        }
        return number;
    }

    uint64_t get_acceptors_count() const {
        return acceptors.size();
    }
};
