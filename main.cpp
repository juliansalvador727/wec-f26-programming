#include <iostream>

// Compile-time constants
namespace config
{
constexpr double solar_per_hour = 200.0; // units of available solar energy per hour
constexpr double battery_capacity = 100.0; // charging cannot exceed 100%
constexpr double generator_cost = 1.0; // the dollar cost per unit
constexpr int hours_per_day = 24;
constexpr int server_port = 8800;
}

enum class Event { None, Cloudy, GeneratorFailure, BatteryFailure, GridFailure };



int main() {
    std::cout << "hello world!\n";
    return 0;
}