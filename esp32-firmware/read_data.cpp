#include <iostream>
#include <fstream>
#include <string>
#include "json.hpp" // The nlohmann/json library

// For convenience, use the nlohmann::json namespace
using json = nlohmann::json;

/**
 * @brief Parses a JSON file to extract and print the Fajr prayer time.
 * 
 * This program uses the nlohmann/json library to parse a JSON file,
 * navigate to the nested "fajr" prayer time, and print it to the console.
 * It includes robust error handling for file I/O, JSON parsing, and missing keys.
 * 
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments. Expects the file path as an optional second argument.
 * @return int Returns 0 on success, 1 on failure.
 */
int main(int argc, char* argv[]) {
    // Determine the file path
    std::string filePath = "data-collection/output/display_data.json";
    if (argc > 1) {
        filePath = argv[1];
    }

    // 1. Open the file
    std::ifstream inputFile(filePath);
    if (!inputFile.is_open()) {
        std::cerr << "Error: Could not open file '" << filePath << "'" << std::endl;
        return 1;
    }

    // 2. Parse the JSON from the file stream
    json data;
    try {
        data = json::parse(inputFile);
    } catch (json::parse_error& e) {
        std::cerr << "Error: JSON parsing failed.\n"
                  << "Message: " << e.what() << '\n'
                  << "Byte position: " << e.byte << std::endl;
        return 1;
    }

    // 3. Safely access the nested "fajr" value and print it
    try {
        // Access can be chained like a dictionary or object
        std::string fajr_time = data.at("prayer_times").at("fajr");
        std::cout << "Fajr prayer time: " << fajr_time << std::endl;
    } catch (json::out_of_range& e) {
        std::cerr << "Error: Could not find required key in JSON.\n"
                  << "Message: " << e.what() << std::endl;
        return 1;
    }

    // File is closed automatically when 'inputFile' goes out of scope
    return 0; // Success
}
