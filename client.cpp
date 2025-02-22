#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "json.hpp" // Local include
#include <set>
#include <fstream>

using json = nlohmann::json;

struct Packet {
    std::string symbol;
    char buysellindicator;
    int quantity;
    int price;
    int packetSequence;
};

void sendRequest(int socket, uint8_t callType, uint8_t resendSeq = 0) {
    uint8_t request[2] = {callType, resendSeq};
    send(socket, request, sizeof(request), 0);
}

Packet parsePacket(const uint8_t* data) {
    Packet packet;
    packet.symbol = std::string(reinterpret_cast<const char*>(data), 4);
    packet.buysellindicator = data[4];
    packet.quantity = ntohl(*reinterpret_cast<const int*>(data + 5));
    packet.price = ntohl(*reinterpret_cast<const int*>(data + 9));
    packet.packetSequence = ntohl(*reinterpret_cast<const int*>(data + 13));
    return packet;
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Could not create socket" << std::endl;
        return 1;
    }

    sockaddr_in server;
    server.sin_addr.s_addr = inet_addr("127.0.0.1"); // Replace with your ngrok TCP endpoint
    server.sin_family = AF_INET;
    server.sin_port = htons(3000); // Replace with your ngrok TCP port

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        std::cerr << "Connection failed" << std::endl;
        return 1;
    }

    std::vector<Packet> packets;
    std::set<int> receivedSequences;

    // Request all packets
    sendRequest(sock, 1);

    uint8_t buffer[17];
    while (true) {
        int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
        if (bytesReceived <= 0) break;

        Packet packet = parsePacket(buffer);
        packets.push_back(packet);
        receivedSequences.insert(packet.packetSequence);
    }

    // Check for missing sequences and request them
    for (int i = 1; i <= packets.back().packetSequence; ++i) {
        if (receivedSequences.find(i) == receivedSequences.end()) {
            sendRequest(sock, 2, i);
            int bytesReceived = recv(sock, buffer, sizeof(buffer), 0);
            if (bytesReceived > 0) {
                Packet packet = parsePacket(buffer);
                packets.push_back(packet);
                receivedSequences.insert(packet.packetSequence);
            }
        }
    }

    close(sock);

    // Generate JSON
    json j;
    for (const auto& packet : packets) {
        j.push_back({
            {"symbol", packet.symbol},
            {"buysellindicator", std::string(1, packet.buysellindicator)},
            {"quantity", packet.quantity},
            {"price", packet.price},
            {"packetSequence", packet.packetSequence}
        });
    }

    std::ofstream o("output.json");
    o << std::setw(4) << j << std::endl;

    return 0;
}