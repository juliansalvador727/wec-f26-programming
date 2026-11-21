#include <iostream>
#include <vector>

// ive divided my code into sections, the first part will be setup related stuffs
// #1 - Constants
// #2 - Data types

// pascal case for structs/enums
// ideally snake case for variables

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

int main() {
    std::cout << "hello world!\n";
    return 0;
}