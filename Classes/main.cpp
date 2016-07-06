#include <enet/enet.h>
#include <ncurses.h>

#include <cstdio>
#include <map>

#include <json.hpp>

#include "Input.h"
#include "Log.h"

static bool Verbose = false;

static ENetAddress address;
static ENetHost* server;

static Input input;
Log messageLog;

static std::map<enet_uint32, ENetPeer*> ConnectedPeers;

std::vector<std::string> Split(const std::string &s, const char delim) {
    std::vector<std::string> elements;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elements.push_back(item);
    }

    return elements;
}

void Init_Connection(enet_uint32 host, int port) {
    enet_initialize();

    address.host = host;
    address.port = static_cast<unsigned short>(port);

    server = enet_host_create(&address, 32, 2, 0, 0);

    puts("SERVER READY\n");
}

void Init_Curses() {
    initscr();
    raw();
    noecho();

    input.Init();
    messageLog.Init();
}

void Send(enet_uint32 peerID, std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(ConnectedPeers[peerID], static_cast<unsigned char>(channel), packet);
}

void Broadcast(std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, static_cast<unsigned char>(channel), packet);
}

int main(int argc, char* argv[]) {
    Init_Curses();

    enet_uint32 host = ENET_PORT_ANY;
    int port = 1234;

    for (int i = 1; i < argc; ++i) {
        std::string argument(argv[i]);

        if (argument == "--verbose" || argument == "-v") {
            Verbose = true;
        }
        else if ((argument == "--host" || argument == "-h") && i + 1 < argc) {
            std::string addr = argv[++i];
            std::vector<std::string> ipParts = Split(addr, '.');
            std::stringstream hexRepr;

            for (auto const &part : ipParts) {
                std::stringstream hexPart;
                hexPart << std::hex << std::stoi(part);

                if (hexPart.str().length() < 2) {
                    const std::string &temp = hexPart.str();
                    hexPart.seekp(0);

                    for (unsigned long j = 0; j < (2 - temp.length()); ++j) {
                        hexPart << "0";
                    }

                    hexPart << temp;
                }

                hexRepr << hexPart.str();
            }

            std::stringstream ss;
            ss << std::hex << hexRepr.str();
            ss >> host;
        }
        else if ((argument == "--port" || argument == "-p") && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        }
    }

    Init_Connection(host, port);

    while (true) {
        char pressedKey = static_cast<char>(wgetch(cmdWin));

        if (pressedKey != -1) {
            if (pressedKey == 'q') {
                break;
            }
            else {
                input.Type(pressedKey);
            }
        }

        ENetEvent event;

        while (enet_host_service(server, &event, 50) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_NONE:
                    break;

                case ENET_EVENT_TYPE_CONNECT:
                    printf("PEER CONNECTED FROM %x:%u\n", event.peer->address.host, event.peer->address.port);
                    ConnectedPeers[event.peer->connectID] = event.peer;
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("%s DISCONNECTED\n\n", event.peer->data);
                    ConnectedPeers.erase(event.peer->connectID);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    nlohmann::json j = nlohmann::json::parse(reinterpret_cast<char*>(event.packet->data));

                    for (nlohmann::json::iterator it = j["events"].begin(); it != j["events"].end(); ++it) {
                        if (it.key() == "connect") {
                            event.peer->data = const_cast<char*>(it.value()["name"].get<std::string>().c_str());

                            puts("PEER CONNECTED");
                            printf("NAME: %s\n\n", it.value()["name"].get<std::string>().c_str());
                        }
                        else if (it.key() == "blockBreak") {
                            puts("BLOCK BROKEN");
                            printf("AT POSITION: %s\n", it.value()["pos"].get<std::string>().c_str());
                            printf("BY: %s\n\n", it.value()["player"].get<std::string>().c_str());

                            nlohmann::json response;
                            response["events"]["breakBlock"]["pos"] = it.value()["pos"];
                            response["events"]["breakBlock"]["player"] = it.value()["player"];
                            Broadcast(response.dump(), event.channelID);
                        }
                    }

                    if (Verbose) {
                        puts("PACKET RECEIVED");
                        printf("LENGTH: %zu\n", event.packet->dataLength);
                        printf("DATA: %s\n", event.packet->data);
                        printf("SENDER: %s\n", event.peer->data);
                        printf("CHANNEL ID: %u\n\n", event.channelID);
                    }

                    break;
            }
        }
    }

    enet_deinitialize();
    endwin();

    return 0;
}
