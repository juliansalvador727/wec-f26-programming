#include <netinet/in.h>  // sockaddr_in, htons, htonl, INADDR_LOOPBACK
#include <sys/socket.h>  // socket, connect, send, recv
#include <unistd.h>      // close

#include <algorithm>
#include <cctype>   // std::isspace, std::isdigit
#include <cstdlib>  // std::strtod
#include <iostream>
#include <map>
#include <sstream>  // std::ostringstream
#include <string>
#include <vector>

// ive divided my code into sections, the first part will be setup related stuffs
// #1 - Constants
// #2 - Data types

// pascal case for structs/enums
// ideally snake case for variables
// camel case for class methods

// #1 Compile-time constants
namespace config
{
constexpr double solar_per_hour = 200.0; // units of available solar energy per hour
constexpr double battery_capacity = 100.0; // charging cannot exceed 100%
constexpr double generator_cost = 1.0; // the dollar cost per unit
constexpr int hours_per_day = 24;
constexpr int server_port = 8800;
}

// #2 Data Types, structs, enums for this
enum class Event { None, Cloudy, GeneratorFailure, BatteryFailure, GridFailure };

enum class BuildingType { Library, Laboratory, LectureHall, StudentCentre, StudentResidence };

struct Building {
    BuildingType type = BuildingType::LectureHall; // default init to lecture hall cuz c++11 doesnt allow us to have empty default init bruh
    double load = 0;

    // old buildings cant use solar power
    bool canUseSolar() const { return type != BuildingType::LectureHall; } 
};


// current state of things given to server at the start of a given hour
struct HourState {
    Event event = Event::None;
    double grid_price = 0;
    std::vector<Building> buildings;
};


// for each building, where energy comes from this hour
struct Supply {
    double solar = 0;
    double battery = 0;
    double grid = 0;
};

struct HourPlan {
    std::vector<Supply> supplies;
    double generator = 0;
};

// #3 our controller class
class Controller {
    private:
        double battery_charge_ = 0;
        int hour_ = 0;

        bool isLastHour() const { return hour_ == config::hours_per_day - 1; }

        bool shouldUseBattery(const HourState& state) const {
            Event event = state.event;
            double price = state.grid_price;
            if (event == Event::BatteryFailure) return false;
            if (event == Event::GridFailure) return true;
            if (isLastHour()) return true;
            if (event == Event::GeneratorFailure) return false;
            return price > config::generator_cost;
        }

        bool shouldRecharge(const HourState& state) const {
            Event event = state.event;
            return event != Event::GeneratorFailure && !isLastHour();
        }
        static double draw(double& available, double& needed) {
            double amount = std::min(available, needed);
            available -= amount;
            needed -= amount;
            return amount;
        }
    public:
        HourPlan planHour(const HourState& state) {
            Event event = state.event;

            double solar_left = config::solar_per_hour;
            double battery_left = battery_charge_;

            if (event == Event::Cloudy) {
                solar_left = 0;
            }
            if (!shouldUseBattery(state)) {
                battery_left = 0;
            }

            HourPlan plan;
            double unmet = 0;
            for (const auto& building : state.buildings) {
                double need = building.load;
                Supply supply;
                if (building.canUseSolar()) {
                    supply.solar = draw(solar_left, need);
                }
                supply.battery = draw(battery_left, need);
                if (event != Event::GridFailure) {
                    supply.grid = need;
                    need = 0;
                }
                plan.supplies.push_back(supply);
                battery_charge_ -= supply.battery;
                unmet += need;
            }
            if (unmet > 1e-9) std::cerr << "[hour " << hour_ << "] WARNING: unmet demand " << unmet << "\n";
            if (shouldRecharge(state)) {
                plan.generator = config::battery_capacity - battery_charge_;
            }
            battery_charge_ += plan.generator;
            ++hour_;
            return plan;
        }
};

namespace json 
{

// parsed json value
struct Value {
    double number = 0;
    std::string text;
    std::vector<Value> items;
    std::map<std::string, Value> fields;

    bool has(const std::string& key) const { return fields.count(key) > 0; };

    // handles empty values
    const Value& operator[](const std::string& key) const {
        static const Value empty;
        auto it = fields.find(key);
        return it == fields.end() ? empty : it->second;
    }

};

// recursive descent parser, c++11
// referenced from https://github.com/dropbox/json11/blob/master/json11.cpp
// and https://www.json.org/json-en.html
class Parser {
private:
    const std::string& text_;
    std::size_t pos_ = 0;
    bool failed_ = false;

    // returns the next non white space character without consuming, null terminated at the end. i.e. \0
    char peek() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) ++pos_;
        return pos_ < text_.size() ? text_[pos_] : '\0';
    }

    void expect(char c) {
        if (peek() == c) ++pos_;
        else failed_ = true;
    }

    bool consumeWord(const std::string& word) {
        if (text_.compare(pos_, word.size(), word) != 0) return false;
        pos_ += word.size();
        return true;
    }

    Value parseValue() {
        Value v;
        char c = peek();
        if (c == '{') v.fields = parseObject();
        else if (c == '[') v.items = parseArray();
        else if (c == '"') v.text = parseString();
        else if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) v.number = parseNumber();
        else if (!consumeWord("true") && !consumeWord("false") && !consumeWord("null")) failed_ = true;
        return v;
    }

    std::map<std::string, Value> parseObject() {
        std::map<std::string, Value> fields;
        expect('{');
        while (!failed_ && peek() != '}') {
            if (!fields.empty()) expect(',');
            std::string key = parseString();
            expect(':');
            fields[key] = parseValue();
        }
        expect('}');
        return fields;
    }

    std::vector<Value> parseArray() {
        std::vector<Value> items;
        expect('[');
        while (!failed_ && peek() != ']') {
            if (!items.empty()) expect(',');
            items.push_back(parseValue());
        }
        expect(']');
        return items;
    }

    std::string parseString() {
        expect('"');
        size_t end = text_.find('"', pos_);  // the server's strings contain no escaped quotes
        if (failed_ || end == std::string::npos) {
            failed_ = true;
            return "";
        }
        std::string out = text_.substr(pos_, end - pos_);
        pos_ = end + 1;
        return out;
    }

    double parseNumber() {
        const char* start = text_.c_str() + pos_;
        char* end = nullptr;
        double value = std::strtod(start, &end);
        if (end == start) failed_ = true;
        pos_ += end - start;
        return value;
    }
public:
    explicit Parser(const std::string& text) : text_(text) {}

    // returns false if the text isn't valid json
    bool parse(Value& result) {
        result = parseValue();
        return !failed_;
    }
};

bool parse(const std::string& text, Value& result) { return Parser(text).parse(result); }

Event eventFromString(const std::string& name) {
    if (name == "cloudy_weather") return Event::Cloudy;
    if (name == "generator_failure") return Event::GeneratorFailure;
    if (name == "battery_failure") return Event::BatteryFailure;
    if (name == "grid_failure") return Event::GridFailure;
    if (name != "none") std::cerr << "WARNING: unknown event '" << name << "'\n";
    return Event::None;
}

// an unknown type is treated as a lecture hall: it gets no solar,
// so the reply stays valid whatever the building really is
BuildingType buildingTypeFromString(const std::string& name) {
    if (name == "library") return BuildingType::Library;
    if (name == "laboratory") return BuildingType::Laboratory;
    if (name == "student_centre") return BuildingType::StudentCentre;
    if (name == "student_residence") return BuildingType::StudentResidence;
    if (name != "lecture_hall") std::cerr << "WARNING: unknown building type '" << name << "'\n";
    return BuildingType::LectureHall;
}

// turns one server message into the controller's input
HourState toHourState(const Value& message) {
    HourState state;
    state.event = eventFromString(message["random_event"].text);
    state.grid_price = message["grid_price"].number;
    for (const Value& load : message["loads"].items) {
        Building building;
        building.type = buildingTypeFromString(load["type"].text);
        building.load = load["load"].number;
        state.buildings.push_back(building);
    }
    return state;
}

// turns the controller's plan into the reply the server expects
std::string toJson(const HourPlan& plan) {
    std::ostringstream out;
    out.precision(10); // whole numbers still print without a decimal point
    out << "{\"buildings\": [";
    for (std::size_t i = 0; i < plan.supplies.size(); ++i) {
        const Supply& supply = plan.supplies[i];
        if (i > 0) out << ", ";
        out << "{\"solar\": " << supply.solar
            << ", \"battery\": " << supply.battery
            << ", \"grid\": " << supply.grid << "}";
    }
    out << "], \"generator\": " << plan.generator << "}";
    return out.str();
}

}

// talks to the simulation server over tcp
// no destructor (rule of zero): main() calls disconnect() at the end of the day,
// operating system closes the socket if the program exits early
// reference - beej's guide to network programming https://beej.us/guide/bgnet/html/#a-simple-stream-client
class ServerConnection {
private:
    int socket_ = -1;
public:
    // returns false if the server can't be reached
    bool connect(int port) {
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // 127.0.0.1 (localhost)
        // connect() takes a generic sockaddr*;
        return socket_ >= 0 && ::connect(socket_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
    }

    void disconnect() { ::close(socket_); }

    // reads one complete json object by matching braces, so it works whether or
    // not the server ends messages with a newline. messages are small, so it
    // reads one byte at a time. returns false once the server disconnects
    bool receive(std::string& message) {
        message.clear();
        int depth = 0; // how many '{' are open
        bool in_string = false; // braces inside "..." don't count
        char c;
        while (::recv(socket_, &c, 1, 0) == 1) {
            if (message.empty() && c != '{') continue; // skip whitespace between messages
            message += c;
            if (c == '"') in_string = !in_string;
            else if (!in_string && c == '{') ++depth;
            else if (!in_string && c == '}' && --depth == 0) return true;
        }
        return false;
    }

    // returns false if the reply couldn't be sent
    bool send(const std::string& message) {
        std::string line = message + "\n";
        return ::send(socket_, line.data(), line.size(), 0) == static_cast<ssize_t>(line.size());
    }
};

// #6 main: one reply per hourly message until the server disconnects
int main() {
    ServerConnection server;
    if (!server.connect(config::server_port)) {
        std::cerr << "error: could not connect to the server on port " << config::server_port << "\n";
        return 1;
    }

    Controller controller;
    std::string message;
    while (server.receive(message)) {
        json::Value parsed;
        if (!json::parse(message, parsed)) {
            std::cerr << "could not parse message: " << message << "\n";
            continue;
        }
        if (!parsed.has("loads")) {
            std::cerr << "server: " << message << "\n";
            continue;
        }

        HourPlan plan = controller.planHour(json::toHourState(parsed));
        std::string reply = json::toJson(plan);
        std::cerr << "<- " << message << "\n-> " << reply << "\n";

        if (!server.send(reply)) {
            std::cerr << "error: could not send reply\n";
            server.disconnect();
            return 1;
        }
    }

    server.disconnect();
    return 0;
}