#include <map>
#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <iostream>

#include <ncurses.h>
#include <enet/enet.h>

#include <json.hpp>

static bool Verbose = false;

static ENetAddress address;
static ENetHost* server;

static std::map<enet_uint32, ENetPeer*> ConnectedPeers;

static bool Exit = false;
static bool BlockingIO = false;

static int MaxX;
static int MaxY;

static int TopLine = 1;

static WINDOW* Top;
static WINDOW* Bottom;

static enet_uint32 Host = ENET_PORT_ANY;
static int Port = 1234;

void Write(std::string str, ...);

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
    
    Write("Ready!");
}

void Send(enet_uint32 peerID, std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(ConnectedPeers[peerID], static_cast<unsigned char>(channel), packet);
}

void Broadcast(std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, static_cast<unsigned char>(channel), packet);
}

void Init_Curses() {
    initscr();
    
    getmaxyx(stdscr, MaxY, MaxX);

    Top = newwin(MaxY - 3, MaxX, 0, 0);
    Bottom = newwin(3, MaxX, MaxY - 3, 0);

    scrollok(Top, TRUE);
    scrollok(Bottom, TRUE);

    box(Top, '|', '=');
    box(Bottom, '|', '-');

    wsetscrreg(Top, 1, MaxY - 5);
    wsetscrreg(Bottom, 1, 1);
}

void Parse_Arguments(int count, char* args[]) {
    for (int i = 1; i < count; ++i) {
        std::string argument(args[i]);

        if (argument == "--verbose" || argument == "-v") {
            Verbose = true;
        }
        else if ((argument == "--host" || argument == "-h") && i + 1 < count) {
            std::string addr = args[++i];
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
            ss >> Host;
        }
        else if ((argument == "--port" || argument == "-p") && i + 1 < count) {
            Port = std::stoi(args[++i]);
        }
    }
}

void Input_Thread() {
    char str[80];
    
    while (true) {
        bzero(str, 80);
        wrefresh(Bottom);

        mvwgetstr(Bottom, 1, 2, str);
        std::string message(str);

        if (message == "") {
            continue;
        }

        while (BlockingIO) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        BlockingIO = true;

        mvwprintw(Top, TopLine, 3, message.c_str());
        wmove(Bottom, 1, 2);
        wclrtoeol(Bottom);

        if (message == "exit") {
            Exit = true;
            return;
        }

        if (TopLine != (MaxY - 5)) {
            ++TopLine;
        }
        else {
            scroll(Top);
            box(Top, '|', '=');
        }

        wrefresh(Top);

        BlockingIO = false;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Write(std::string str, ...) {
    while (BlockingIO) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    BlockingIO = true;

    wmove(Top, TopLine, 3);

    va_list args;
    va_start(args, str);
    vw_printw(Top, str.c_str(), args);    
    va_end(args);

    if (TopLine != (MaxY - 5)) {
        ++TopLine;
    }
    else {
        scroll(Top);
        box(Top, '|', '=');
    }

    wrefresh(Top);

    wmove(Bottom, 1, 2);

    BlockingIO = false;
}

int main(int argc, char* argv[]) {
    Init_Curses();
    Parse_Arguments(argc, argv);
    Init_Connection(Host, Port);

    std::thread input(Input_Thread);

    while (!Exit) {
        ENetEvent event;

        while (enet_host_service(server, &event, 50) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_NONE:
                    break;

                case ENET_EVENT_TYPE_CONNECT:
                    Write("Peer connected from %x:%u.", event.peer->address.host, event.peer->address.port);
                    Write("");

                    ConnectedPeers[event.peer->connectID] = event.peer;
                    break;

                case ENET_EVENT_TYPE_DISCONNECT:
                    Write("%s DISCONNECTED", event.peer->data);
                    Write("");

                    ConnectedPeers.erase(event.peer->connectID);
                    break;

                case ENET_EVENT_TYPE_RECEIVE:
                    nlohmann::json j = nlohmann::json::parse(reinterpret_cast<char*>(event.packet->data));

                    for (nlohmann::json::iterator it = j["events"].begin(); it != j["events"].end(); ++it) {
                        if (it.key() == "connect") {
                            event.peer->data = const_cast<char*>(it.value()["name"].get<std::string>().c_str());
                            Write("%s connected.", it.value()["name"].get<std::string>().c_str());
                        }
                        else if (it.key() == "blockBreak") {
                            if (Verbose) {
                                Write("BLOCK BROKEN");
                                Write("AT POSITION: %s", it.value()["pos"].get<std::string>().c_str());
                                Write("BY: %s", it.value()["player"].get<std::string>().c_str());
                                Write("");
                            }

                            nlohmann::json response;
                            response["events"]["breakBlock"]["pos"] = it.value()["pos"];
                            response["events"]["breakBlock"]["player"] = it.value()["player"];
                            Broadcast(response.dump(), event.channelID);
                        }
                        else if (it.key() == "message") {
                            Write("%s: %s", event.peer->data, j["events"]["message"].get<std::string>().c_str());

                            nlohmann::json response;
                            response["events"]["message"]["value"] = j["events"]["message"];
                            response["events"]["message"]["player"] = std::string(static_cast<char*>(event.peer->data));
                            Broadcast(response.dump(), event.channelID);
                        }
                    }

                    if (Verbose) {
                        Write("PACKET RECEIVED");
                        Write("LENGTH: %zu", event.packet->dataLength);
                        Write("DATA: %s", event.packet->data);
                        Write("SENDER: %s", event.peer->data);
                        Write("CHANNEL ID: %u", event.channelID);
                        Write("");
                    }

                    break;
            }
        }
    }

    endwin();
    input.join();
    enet_deinitialize();
    return 0;
}
