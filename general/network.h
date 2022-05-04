#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <cassert>
#include <vector>
#include <thread>
#include <unistd.h>

struct ProcessDescriptor {
    std::string ip_address;
    uint64_t id;
    uint64_t port;
};

uint8_t read_byte(int client_fd) {
    uint8_t byte;

    ssize_t len = read(client_fd, &byte, 1);
    if (len != 1) {
        std::cout << "error reading byte" << std::endl;
        assert(false);
    }
    return byte;
}

uint64_t read_number(int client_fd) {
    uint64_t number;
    size_t bytes_read = 0;

    while (bytes_read != 64 / 8) {
        ssize_t len = read(client_fd, (char*)(&number) + bytes_read, 64 / 8 - bytes_read);
        if (len <= 0) {
            std::cout << "error reading number" << std::endl;
            assert(false);
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
            std::cout << "error reading string" << std::endl;
            assert(false);
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
    std::cout << "Open connection to " << descriptor.id << " port: " << descriptor.port << " ip: " << descriptor.ip_address << std::endl;
    int sock = 0;
    struct sockaddr_in serv_addr;
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        assert(false);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(descriptor.port);

    if (inet_pton(AF_INET, descriptor.ip_address.data(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address" << std::endl;
        assert(false);
    }

    if (connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
        std::cout << "Connection failed: " << descriptor.ip_address << ' ' << descriptor.port << " error: " << errno << std::endl;
        assert(false);
    }
    return sock;
}


void send_byte(int sock, uint8_t byte) {
    ssize_t len = send(sock, &byte, 1, 0);
    if (len != 1) {
        std::cout << "error sending number: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}

void send_number(int sock, uint64_t num) {
    ssize_t len = send(sock, &num, 64 / 8, 0);
    if (len != 64 / 8) {
        std::cout << "error sending number: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}

void send_string(int sock, const std::string &s) {
    send_number(sock, s.length());
    ssize_t len = send(sock, s.data(), s.length(), 0);
    if (len != s.length()) {
        std::cout << "error sending string: " << errno << std::endl;
        exit(EXIT_FAILURE);
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
    int server_fd;

    struct sockaddr_in address;
    int addrlen;

    TcpServer(uint64_t port) {
        int opt = 1;
        addrlen = sizeof(address);

        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            assert(false);
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            assert(false);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *) &address, sizeof(address)) < 0) {
            assert(false);
        }
        if (listen(server_fd, 100000) < 0) {
            assert(false);
        }
    }

    int accept_client() {
        struct sockaddr_in client_addr;
        return accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    }

    int accept_client(std::string &client_ip) {
        struct sockaddr_in client_addr;
        client_ip.resize(INET_ADDRSTRLEN);
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip.data(), INET_ADDRSTRLEN);
        return accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &addrlen);
    }

};
