#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>

using namespace std;

struct SimulationSettings {
    int mapRows, mapCols;
    int maxTicks;
    int maxStations;
    int clientsCount;

    int dronesCount;
    int robotsCount;
    int scootersCount;

    int totalPackages;
    int spawnFrequency;
};

class ConfigLoader {
public:
    static SimulationSettings load(const string& filename) {
        SimulationSettings settings;
        ifstream file("simulation_setup.txt");

        if (!file.is_open()) {
            cerr << "Eroare: Nu s-a putut deschide " << filename << ".\n";
            return settings;
        }

        string line;
        while (getline(file, line)) {
            if (line.empty() || line.substr(0, 2) == "//") continue;

            stringstream ss(line);
            string key, temp;

            ss >> key;

            if (key.back() == ':') key.pop_back();

            if (key == "MAP_SIZE") {
                ss >> settings.mapRows >> settings.mapCols;
            }
            else if (key == "MAX_TICKS") ss >> settings.maxTicks;
            else if (key == "MAX_STATIONS") ss >> settings.maxStations;
            else if (key == "CLIENTS_COUNT") ss >> settings.clientsCount;
            else if (key == "DRONES") ss >> settings.dronesCount;
            else if (key == "ROBOTS") ss >> settings.robotsCount;
            else if (key == "SCOOTERS") ss >> settings.scootersCount;
            else if (key == "TOTAL_PACKAGES") ss >> settings.totalPackages;
            else if (key == "SPAWN_FREQUENCY") ss >> settings.spawnFrequency;
        }

        file.close();
        cout << "Configuratia a fost incarcata cu succes!\n";
        return settings;
    }
};