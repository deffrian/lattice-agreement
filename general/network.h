#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <vector>
#include <thread>
#include <unistd.h>

#include "general/logger.h"

struct ProcessDescriptor {
    std::string ip_address;
    uint64_t id;
    uint64_t port;
};

uint8_t read_byte(int client_fd) {
    uint8_t byte;

    ssize_t len = read(client_fd, &byte, 1);
    if (len != 1) {
        LOG(ERROR) << "Error reading byte" << errno;
        throw std::runtime_error("Error reading byte: " + std::to_string(len));
    }
    return byte;
}

uint64_t read_number(int client_fd) {
    uint64_t number;
    size_t bytes_read = 0;

    while (bytes_read != 64 / 8) {
        ssize_t len = read(client_fd, (char*)(&number) + bytes_read, 64 / 8 - bytes_read);
        if (len <= 0) {
            LOG(ERROR) << "Error reading number" << errno;
            throw std::runtime_error("Error reading number: " + std::to_string(len));
        }
        bytes_read += len;
    }
    return number;
}

std::string read_string(int client_fd) {
    uint64_t s_len = read_number(client_fd);
    std::string s(s_len, ' ');
    size_t bytes_read = 0;

    while (bytes_read != s_len) {
        ssize_t len = read(client_fd, s.data() + bytes_read, s_len - bytes_read);
        if (len <= 0) {
            LOG(ERROR) << "Error reading string" << errno;
            throw std::runtime_error("Error reading string: " + std::to_string(len));
        }
        bytes_read += len;
    }
    return s;
}

template<typename L>
L read_lattice(int client_fd) {
    L res;
    uint64_t size = read_number(client_fd);
    for (uint64_t i = 0; i < size; ++i) {
        res.insert(read_number(client_fd));
    }
    return res;
}

template<typename L>
std::vector<L> read_lattice_vector(int client_fd) {
    uint64_t v_size = read_number(client_fd);
    std::vector<L> v(v_size);
    for (uint64_t i = 0; i < v_size; ++i) {
        v[i] = read_lattice<L>(client_fd);
    }
    return v;
}

int open_socket(const ProcessDescriptor &descriptor) {
    LOG(INFO) << "Open connection to " << descriptor.id << " port: " << descriptor.port << " ip: " << descriptor.ip_address;
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG(ERROR) << "Socket creation error" << errno;
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(descriptor.port);

    if (inet_pton(AF_INET, descriptor.ip_address.data(), &serv_addr.sin_addr) <= 0) {
        LOG(ERROR) << "Invalid address";
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
        LOG(ERROR) << "Connection failed: " << descriptor.ip_address << ' ' << descriptor.port << " error: " << errno;
        exit(EXIT_FAILURE);
    }
    return sock;
}


void send_byte(int sock, uint8_t byte) {
    ssize_t len = send(sock, &byte, 1, 0);
    if (len != 1) {
        LOG(ERROR) << "Error sending byte:" << errno;
        throw std::runtime_error("Error sending byte: " + std::to_string(len));
    }
}

void send_number(int sock, uint64_t num) {
    ssize_t len = send(sock, &num, 64 / 8, 0);
    if (len != 64 / 8) {
        LOG(ERROR) << "Error sending number:" << errno;
        throw std::runtime_error("Error sending number: " + std::to_string(len));
    }
}

void send_string(int sock, const std::string &s) {
    send_number(sock, s.length());
    ssize_t len = send(sock, s.data(), s.length(), 0);
    if (len != s.length()) {
        LOG(ERROR) << "error sending string:" << errno;
        throw std::runtime_error("Error sending string: " + std::to_string(len));
    }
}

template<typename L>
void send_lattice(int sock, const L &lattice) {
    send_number(sock, lattice.set.size());
    for (uint64_t elem : lattice.set) {
        send_number(sock, elem);
    }
}

template<typename L>
void send_lattice_vector(int sock, const std::vector<L> &v) {
    send_number(sock, v.size());
    for (const auto &elem: v) {
        send_lattice(sock, elem);
    }
}

struct TcpServer {
    const size_t MAX_CONNECTIONS = 1024;

    int server_fd;

    std::vector<int> all_connections;

    struct sockaddr_in address;
    int addrlen;

    TcpServer(uint64_t port) : all_connections(MAX_CONNECTIONS, -1) {
        int opt = 1;
        addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            exit(EXIT_FAILURE);
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
            exit(EXIT_FAILURE);
        }
        if (listen(server_fd, 100000) < 0) {
            exit(EXIT_FAILURE);
        }
        all_connections[0] = server_fd;
    }

    int accept_client() {
        struct sockaddr_in client_addr;
        return accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    }

    int select_client() {
        fd_set read_fd_set;
        FD_ZERO(&read_fd_set);

        for (size_t i = 0; i < MAX_CONNECTIONS; ++i) {
            if (all_connections[i] >= 0) {
                FD_SET(all_connections[i], &read_fd_set);
            }
        }


        int select_val = select(FD_SETSIZE, &read_fd_set, nullptr, nullptr, nullptr);
        if (select_val < 0) {
            LOG(ERROR) << "Select error";
            exit(EXIT_FAILURE);
        }
        if (FD_ISSET(server_fd, &read_fd_set)) {
            /* accept the new connection */
            printf("Returned fd is %d (server's fd)\n", server_fd);
            struct sockaddr_in client_addr;
            int client_fd = accept_client();
            if (client_fd < 0) {
                LOG(ERROR) << "Error accepting connection";
                exit(EXIT_FAILURE);
            }
            LOG(INFO) << "Connection accepted";
            for (int i = 0; i < MAX_CONNECTIONS; i++) {
                if (all_connections[i] < 0) {
                    all_connections[i] = client_fd;
                    break;
                }
            }
            return client_fd;
        }

        for (size_t i = 1; i < MAX_CONNECTIONS; i++) {
            if ((all_connections[i] > 0) &&
                (FD_ISSET(all_connections[i], &read_fd_set))) {

                return all_connections[i];
            }
        }
        LOG(ERROR) << "Wrong select";
        exit(EXIT_FAILURE);
    }

    int accept_client(std::string &client_ip) {
        struct sockaddr_in client_addr;
        client_ip.resize(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip.data(), INET_ADDRSTRLEN);
        return accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    }

};
