#include "simulation_engine.h"

int main() {
    SimulationEngine engine;

    std::cout << "1. Harta Fisier\n";
    cout<<"2. Harta Procedurala\n";
    cout<<"Alege: ";
    int opt;
    cin >> opt;

    engine.initialize(opt);
    engine.run();

    return 0;
}