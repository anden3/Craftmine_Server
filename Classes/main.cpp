#include <map>
#include <mutex>
#include <chrono>
#include <random>
#include <string>
#include <thread>
#include <fstream>

#ifdef WIN32
    #include <curses.h>
#else
    #include <ncurses.h>
#endif

#include <enet/enet.h>
#include <json.hpp>

const int DEFAULT_CHANNEL = 0;

static std::map<enet_uint32, ENetPeer*> ConnectedPeers;

static enet_uint32 Host = ENET_HOST_ANY;
static int Port = 1234;

static int Seed = 0;

static ENetAddress address;
static ENetHost* server;

static bool Verbose = false;
static bool Exit = false;

static std::mutex WriteLock;

static int MaxX;
static int MaxY;

static int TopLine = 1;

static WINDOW* Top;
static WINDOW* Bottom;

const std::map<char, short> ColorCodes = {
    {'0', 232}, // Black
    {'1', 17 }, // Dark Blue
    {'2', 28 }, // Dark Green
    {'3', 4  }, // Dark Aqua
    {'4', 124}, // Dark Red
    {'5', 91 }, // Dark Purple
    {'6', 214}, // Gold
    {'7', 242}, // Gray
    {'8', 237}, // Dark Gray
    {'9', 21 }, // Blue
    {'a', 154}, // Green
    {'b', 123}, // Aqua
    {'c', 196}, // Red
    {'d', 213}, // Light Purple
    {'e', 226}, // Yellow
    {'f', 255}  // White
};

void Write(std::string str);

std::vector<std::string> Split(const std::string &s, const char delim) {
    std::vector<std::string> elements;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elements.push_back(item);
    }

    return elements;
}

std::string Join(const std::vector<int> &list, const std::string joiner) {
    std::string result = "";

    for (unsigned long i = 0; i < list.size() - 1; ++i) {
        result += std::to_string(list[i]) + joiner;
    }

    return result + std::to_string(list.back());
}

std::string Join(const std::vector<std::string> &list, const std::string joiner = " ") {
    std::string result = "";

    for (unsigned long i = 0; i < list.size() - 1; ++i) {
        result += list[i] + joiner;
    }

    return result + list.back();
}

std::string To_String(enet_uint8* data) {
    return std::string(reinterpret_cast<char*>(data));
}

std::string To_String(void* data) {
    return std::string(static_cast<char*>(data));
}

std::string Get_Timestamp() {
    time_t t = std::time(0);
    struct tm* now = std::localtime(&t);

    return Join({
        now->tm_hour, now->tm_min, now->tm_sec
    }, ":");
}

int Get_Seed() {
    static std::uniform_int_distribution<int> uni(-2147483648, 2147483647);
    static bool Initialized = false;
    static std::mt19937_64 rng;

    if (!Initialized) {
        Initialized = true;

        int64_t timeSeed = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        std::seed_seq ss {
            static_cast<uint32_t>(timeSeed & 0xffffffff),
            static_cast<uint32_t>(timeSeed >> 32)
        };

        rng.seed(ss);
    }

    return uni(rng);
}

template <typename T> std::string To_Hex(T value) {
    std::stringstream ss;
    ss << std::hex << value;
    return ss.str();
}

void Init_Connection(enet_uint32 host, int port) {
    enet_initialize();

    address.host = host;
    address.port = static_cast<unsigned short>(port);

    server = enet_host_create(&address, 32, 2, 0, 0);
    
    Write("&6Ready!");
    Write("");
}

void Send(enet_uint32 peerID, std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(ConnectedPeers[peerID], static_cast<unsigned char>(channel), packet);
}

void Broadcast(std::string message, int channel) {
    ENetPacket* packet = enet_packet_create(message.c_str(), message.length() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(server, static_cast<unsigned char>(channel), packet);
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

        propFile << properties;
    }
    else {
        properties << propFile;

        Host = properties["host"];
        Port = properties["port"];
        Seed = properties["seed"];
    }
}

void Init_Curses() {
    initscr();
    start_color();

    getmaxyx(stdscr, MaxY, MaxX);

    Top = newwin(MaxY - 3, MaxX, 0, 0);
    Bottom = newwin(3, MaxX, MaxY - 3, 0);

    scrollok(Top, TRUE);

    box(Top, '|', '=');
    box(Bottom, '|', '-');

    wsetscrreg(Top, 1, MaxY - 5);
    wsetscrreg(Bottom, 1, 1);

    for (short i = 0; i < 256; ++i) {
        init_pair(i, i, 0);
    }
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

void Parse_Commands(std::string &input) {
    auto arguments = Split(input, ' ');

    if (arguments[0] == "exit") {
        Exit = true;
    }

    else if (arguments[0] == "say" && arguments.size() > 1) {
        std::string message = Join(std::vector<std::string>(arguments.begin() + 1, arguments.end()));

        nlohmann::json response;
        response["events"]["message"]["value"] = message;
        response["events"]["message"]["player"] = "&5Server&f";
        Broadcast(response.dump(), DEFAULT_CHANNEL);

        input = "&5Server&f: " + message;
    }

    else {
        input = "";
    }
}

void Draw_Color_String(std::string str) {
    int startPos = 0;
    std::string partStr = "";
    bool checkNext = false;
    short currentColor = 7;

    mvwprintw(Top, TopLine, 3, Get_Timestamp().c_str());

    for (char const &c : str) {
        if (c == '&') {
            if (partStr.length() > 0) {
                wattron(Top, COLOR_PAIR(currentColor));
                mvwprintw(Top, TopLine, 15 + startPos, partStr.c_str());
                wattroff(Top, COLOR_PAIR(currentColor));

                startPos += partStr.length();

                partStr.clear();
            }

            checkNext = true;
            continue;
        }

        if (checkNext) {
            checkNext = false;
            currentColor = ColorCodes.at(c);
            continue;
        }

        partStr += c;
    }

    if (partStr.length() > 0) {
        wattron(Top, COLOR_PAIR(currentColor));
        mvwprintw(Top, TopLine, 15 + startPos, partStr.c_str());
        wattroff(Top, COLOR_PAIR(currentColor));
    }
}

void Input_Thread() {
    char str[80];
    
    while (!Exit) {
		std::fill(str, str + 80, 0);

        mvwgetstr(Bottom, 1, 2, str);
        std::string message(str);

        Parse_Commands(message);

        if (message != "") {
            Write(message);
        }
        else {
            wmove(Bottom, 1, 2);
        }

        wclrtoeol(Bottom);
        wrefresh(Bottom);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void Write(std::string str) {
    WriteLock.lock();

    if (str != "") {
        Draw_Color_String(str);
    }

    if (TopLine != (MaxY - 5)) {
        ++TopLine;
    }
    else {
        scroll(Top);
        box(Top, '|', '=');
    }

    wrefresh(Top);
    wmove(Bottom, 1, 2);

    WriteLock.unlock();
}

void Check_Event(ENetEvent e) {
    if (e.type == ENET_EVENT_TYPE_NONE) {
        return;
    }

    else if (e.type == ENET_EVENT_TYPE_CONNECT) {
        Write("Peer connected from " + To_Hex(e.peer->address.host) + ":" + std::to_string(e.peer->address.port));
        Write("");

        ConnectedPeers[e.peer->connectID] = e.peer;
    }

    else if (e.type == ENET_EVENT_TYPE_DISCONNECT) {
        Write("&4" + To_String(e.peer->data) + " &fdisconnected");
        Write("");

        ConnectedPeers.erase(e.peer->connectID);
    }

    else if (e.type == ENET_EVENT_TYPE_RECEIVE) {
        nlohmann::json j = nlohmann::json::parse(To_String(e.packet->data));

        for (nlohmann::json::iterator it = j["events"].begin(); it != j["events"].end(); ++it) {
            nlohmann::json response;

            if (it.key() == "connect") {
                e.peer->data = const_cast<char*>(it.value()["name"].get<std::string>().c_str());
                Write("&4" + it.value()["name"].get<std::string>() + " &fconnected.");

                response["events"]["config"]["seed"] = Seed;
                Send(e.peer->connectID, response.dump(), e.channelID);
            }

            else if (it.key() == "blockBreak") {
                if (Verbose) {
                    Write("BLOCK BROKEN");
                    Write("AT POSITION: " + it.value()["pos"].get<std::string>());
                    Write("BY: " + it.value()["player"].get<std::string>());
                    Write("");
                }

                response["events"]["breakBlock"]["pos"] = it.value()["pos"];
                response["events"]["breakBlock"]["player"] = it.value()["player"];
                Broadcast(response.dump(), e.channelID);
            }

            else if (it.key() == "message") {
                Write("&4" + To_String(e.peer->data) + "&f: " + j["events"]["message"].get<std::string>());

                response["events"]["message"]["value"] = j["events"]["message"];
                response["events"]["message"]["player"] = To_String(e.peer->data);
                Broadcast(response.dump(), e.channelID);
            }
        }

        if (Verbose) {
            Write("PACKET RECEIVED");
            Write("LENGTH: " + std::to_string(e.packet->dataLength));
            Write("DATA: " + To_String(e.packet->data));
            Write("SENDER: " + To_String(e.peer->data));
            Write("CHANNEL ID: " + std::to_string(e.channelID));
            Write("");
        }
    }
}

int main(int argc, char* argv[]) {
    Init_Config();
    Init_Curses();
    Parse_Arguments(argc, argv);
    Init_Connection(Host, Port);

    std::thread input(Input_Thread);

    while (!Exit) {
        ENetEvent event;

        while (enet_host_service(server, &event, 50) > 0) {
            Check_Event(event);
        }
    }

    endwin();
    input.join();
    enet_deinitialize();
    return 0;
}
