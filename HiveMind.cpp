#include <algorithm>

#include "harta.h"

using namespace std;



int main()
{
    ProceduralMapGenerator generator;
    SimulationSettings settings = ConfigLoader::load("simulation_setup.txt");

    cout<<"Selecteaza optiunea:\n";
    cout<<"1.Citeste din fisier.\n";
    cout<<"2.Genereaza harta.\n";
    cout<<"0.Iesi.\n";
    int x;
    cin>>x;
    if(x==1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test");
        MapGrid myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations);
        cout << "Harta citita:" << endl;
        printMap(myMap);
    }
    else if(x==2) {
        MapGrid myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations);
        printMap(myMap);
    }

    return 0;
}