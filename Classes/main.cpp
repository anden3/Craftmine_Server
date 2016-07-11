#include "main.h"

#include <map>
#include <fstream>

#include <enet/enet.h>
#include <json.hpp>

#define GLM_SWIZZLE
#include <glm/glm.hpp>

#include <boost/filesystem.hpp>

#include "TUI.h"
#include "Functions.h"

std::string World = "";

const int DEFAULT_CHANNEL = 0;

static std::map<unsigned int, ENetPeer*> ConnectedPeers;
static std::map<unsigned int, std::string> PeerNames;

static unsigned int Host = ENET_HOST_ANY;
static int Port = 1234;

static int Seed = 0;

static ENetAddress address;
static ENetHost* server;

static int Verbose = 0;

struct Player {
    glm::vec3 Position;
    float Pitch;
    float Yaw;
};

struct Stack {
    Stack() { Type = 0; Size = 0; }

    Stack(std::string type, int size = 1) : Size(size) {
        unsigned long delimPos = type.find(':');
        Type = std::stoi(type.substr(0, delimPos));
        if (delimPos != std::string::npos) { Data = std::stoi(type.substr(delimPos + 1)); }
    }

    Stack(int type, int size = 1) : Type(type), Size(size) {}
    Stack(int type, int data, int size) : Type(type), Size(size), Data(data) {}

    int Type;
    int Size;
    int Data = 0;
};

static std::map<std::string, Player> Players;

bool Exit = false;

void Init_Config();
void Init_Connection(unsigned int host, int port);

void Parse_Arguments(int count, char* args[]);
void Check_Event(ENetEvent e);

void Chat_Message(ENetEvent &e, std::string message);
void Player_Move(ENetEvent &e, nlohmann::json &data);
void Save_Player(ENetEvent &e, nlohmann::json &data);
void Player_Connect(ENetEvent &e, std::string name);
void Block_Break(ENetEvent &e, std::string pos);
void Player_Disconnect(ENetEvent &e);
void Load_Player(ENetEvent &e);

void Send(unsigned int peerID, std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(ConnectedPeers[peerID], static_cast<unsigned char>(channel), packet);
}

void Broadcast(std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, static_cast<unsigned char>(channel), packet);
}

std::string Get_Name(ENetEvent &e) {
    return *static_cast<std::string*>(e.peer->data);
}

std::string Get_Name(ENetPeer &peer) {
    return *static_cast<std::string*>(peer.data);
}

int main(int argc, char* argv[]) {
    Init_Config();
    Curses::Init();
    Parse_Arguments(argc, argv);
    Init_Connection(Host, Port);

    while (!Exit) {
        ENetEvent event;

        while (enet_host_service(server, &event, 50) > 0) {
            Check_Event(event);
        }
    }
    
    Curses::Clean_Up();
    enet_deinitialize();
    return 0;
}

void Init_Config() {
    std::fstream propFile("properties.json");
    nlohmann::json properties;

    if (!propFile.is_open()) {
        propFile.clear();
        propFile.open("properties.json", std::ios_base::out);

        Seed = Get_Seed();
        
        properties["host"] = ENET_HOST_ANY;
        properties["port"] = Port;
        properties["seed"] = Seed;
        properties["world"] = World;

        propFile << properties;
    }
    else {
        properties << propFile;

        Host = properties["host"];
        Port = properties["port"];
        Seed = properties["seed"];
        World = properties["world"];

        boost::filesystem::path newWorld("Worlds/" + World);
        boost::filesystem::create_directory(newWorld);
    }
}

void Init_Connection(unsigned int host, int port) {
    enet_initialize();

    address.host = host;
    address.port = static_cast<unsigned short>(port);

    server = enet_host_create(&address, 32, 2, 0, 0);
    
    Curses::Write("&6Ready!", true);
}

void Parse_Arguments(int count, char* args[]) {
    for (int i = 1; i < count; ++i) {
        std::string argument(args[i]);

        if (argument == "--verbose" || argument == "-v") {
            Verbose = 1;
        }
        else if (argument == "--very-verbose" || argument == "-vv") {
            Verbose = 2;
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

void Check_Event(ENetEvent e) {
    if (e.type == ENET_EVENT_TYPE_NONE) {
        return;
    }

    else if (e.type == ENET_EVENT_TYPE_CONNECT) {
        Curses::Write("Peer connected from " + To_Hex(e.peer->address.host) + ":" + std::to_string(e.peer->address.port), true);
        ConnectedPeers[e.peer->connectID] = e.peer;
    }

    else if (e.type == ENET_EVENT_TYPE_DISCONNECT) {
        Player_Disconnect(e);
    }

    else if (e.type == ENET_EVENT_TYPE_RECEIVE) {
        nlohmann::json j = nlohmann::json::parse(To_String(e.packet->data));

        if (j["event"] == "connect") {
            Player_Connect(e, j["name"]);
        }

        else if (j["event"] == "blockBreak") {
            Block_Break(e, j["pos"]);
        }

        else if (j["event"] == "message") {
            Chat_Message(e, j["message"]);
        }

        else if (j["event"] == "update") {
            Player_Move(e, j);
        }

        else if (j["event"] == "save") {
            Save_Player(e, j);
        }

        if (Verbose == 2 || (Verbose == 1 && j["event"] != "update")) {
            Curses::Write("PACKET RECEIVED");
            Curses::Write("LENGTH: " + std::to_string(e.packet->dataLength));
            Curses::Write("DATA: " + To_String(e.packet->data));
            Curses::Write("SENDER: " + Get_Name(e));
            Curses::Write("CHANNEL ID: " + std::to_string(e.channelID), true);
        }

        enet_packet_destroy(e.packet);
    }
}

void Player_Connect(ENetEvent &e, std::string name) {
    PeerNames[e.peer->connectID] = name;
    e.peer->data = &PeerNames[e.peer->connectID];
    Players[name] = Player();

    Curses::Write("&4" + name + " &fconnected.");

    nlohmann::json response;
    response["event"] = "config";
    response["seed"] = Seed;
    response["world"] = World;
    Send(e.peer->connectID, response.dump(), e.channelID);

    Load_Player(e);
}

void Player_Disconnect(ENetEvent &e) {
    Curses::Write("&4" + Get_Name(e) + " &fdisconnected", true);
    Players.erase(Get_Name(e));

    unsigned int connectID;

    for (auto const &peer : ConnectedPeers) {
        if (peer.second->data == e.peer->data) {
            connectID = peer.first;
        }
    }
    PeerNames.erase(connectID);
    ConnectedPeers.erase(connectID);
}

void Player_Move(ENetEvent &e, nlohmann::json &data) {
    Player &p = Players[Get_Name(e)];

    if (data.count("pos")) {
        p.Position = glm::vec3(
            data["pos"][0], data["pos"][1], data["pos"][2]
        );
    }

    if (data.count("pitch")) {
        p.Pitch = data["pitch"];
    }

    if (data.count("yaw")) {
        p.Yaw = data["yaw"];
    }

    nlohmann::json response = data;
    response["event"] = "update";
    response["player"] = Get_Name(e);

    for (auto const &peer : ConnectedPeers) {
        if (peer.first != e.peer->connectID) {
            Send(peer.first, response.dump(), e.channelID);
        }
    }
}

void Load_Player(ENetEvent &e) {
    std::ifstream dataFile("Worlds/" + World + "/" + Get_Name(e) + ".json");

    if (dataFile.good()) {
        nlohmann::json playerData;
        playerData << dataFile;

        playerData["event"] = "load";
        Send(e.peer->connectID, playerData.dump(), e.channelID);
    }
}

void Save_Player(ENetEvent &e, nlohmann::json &data) {
    std::ofstream dataFile(
        "Worlds/" + World + "/" + Get_Name(e) + ".json",
        std::ofstream::trunc
    );
    dataFile << data;
}

void Block_Break(ENetEvent &e, std::string pos) {
    nlohmann::json response;
    response["event"] = "blockBreak";
    response["pos"] = pos;
    response["player"] = Get_Name(e);
    Broadcast(response.dump(), e.channelID);
}

void Chat_Message(ENetEvent &e, std::string message) {
    Curses::Write("&4" + Get_Name(e) + "&f: " + message);

    nlohmann::json response;
    response["event"] = "message";
    response["message"] = message;
    response["player"] = Get_Name(e);
    Broadcast(response.dump(), e.channelID);
}

void Parse_Commands(std::string &input) {
    auto arguments = Split(input, ' ');

    if (arguments[0] == "exit") {
        Exit = true;
    }

    else if (arguments[0] == "say" && arguments.size() > 1) {
        std::string message = Join(std::vector<std::string>(arguments.begin() + 1, arguments.end()));

        nlohmann::json response;
        response["event"] = "message";
        response["message"] = message;
        response["player"] = "&5Server&f";
        Broadcast(response.dump(), DEFAULT_CHANNEL);

        input = "&5Server&f: " + message;
    }

    else {
        input = "&3Error! &fInvalid command.";
    }
}
