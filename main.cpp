#include <algorithm>
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

int main() {
    std::cout << "hello world!\n";
    return 0;
}