#pragma once


struct TcpClient {

    int sock;

    explicit TcpClient(const ProcessDescriptor &descriptor) {
        LOG(INFO) << "Open connection to" << descriptor.id << "port:" << descriptor.port << "ip:" << descriptor.ip_address;
        sock = 0;
        struct sockaddr_in serv_addr;
        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            LOG(ERROR) << "Socket creation error" << errno;
            throw std::runtime_error("Socket creation error");
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(descriptor.port);

        if (inet_pton(AF_INET, descriptor.ip_address.data(), &serv_addr.sin_addr) <= 0) {
            LOG(ERROR) << "Invalid address";
            throw std::runtime_error("Invalid address");
        }

        if (connect(sock, (struct sockaddr*)&serv_addr,sizeof(serv_addr)) < 0) {
            LOG(ERROR) << "Client connection failed: " << descriptor.ip_address << ' ' << descriptor.port << " error: " << errno;
            throw std::runtime_error("Client connection failed " + descriptor.ip_address + " " + std::to_string(descriptor.port));
        }
    }

    template<typename L>
    void send_recVal(const std::vector<std::pair<std::vector<L>, uint64_t>> &recVal) {
        send_number(recVal.size());
        for (auto elem: recVal) {
            send_lattice_vector(elem.first);
            send_number(elem.second);
        }
    }


    void send_byte(uint8_t byte) {
        ssize_t len = send(sock, &byte, 1, 0);
        if (len != 1) {
            LOG(ERROR) << "Error sending byte:" << errno;
            throw std::runtime_error("Error sending byte: " + std::to_string(len));
        }
    }

    void send_number(uint64_t num) {
        ssize_t len = send(sock, &num, 64 / 8, 0);
        if (len != 64 / 8) {
            LOG(ERROR) << "Error sending number:" << errno;
            throw std::runtime_error("Error sending number: " + std::to_string(len));
        }
    }

    void send_string(const std::string &s) {
        send_number(s.length());
        ssize_t len = send(sock, s.data(), s.length(), 0);
        if (len != s.length()) {
            LOG(ERROR) << "error sending string:" << errno;
            throw std::runtime_error("Error sending string: " + std::to_string(len));
        }
    }

    template<typename L>
    void send_lattice(const L &lattice) {
        send_number(lattice.set.size());
        for (uint64_t elem : lattice.set) {
            send_number(elem);
        }
    }

    template<typename L>
    void send_lattice_vector(const std::vector<L> &v) {
        send_number(v.size());
        for (const auto &elem: v) {
            send_lattice(elem);
        }
    }
};