#include "simulation_engine.h"

int main() {
    SimulationEngine engine;

    cout << "1. Harta Fisier\n";
    cout<<"2. Harta Procedurala\n";
    cout<<"0. Iesi.\n";
    cout<<"Alege: ";
    int opt;
    cin >> opt;
    while (opt!=0) {
        engine.initialize(opt);
        engine.run();
        cout << "1. Harta Fisier\n";
        cout<<"2. Harta Procedurala\n";
        cout<<"0. Iesi.\n";
        cout<<"Alege: ";
        int opt;
        cin >> opt;
    }
    cout<<"La revedere\n";
    return 0;
}