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

    MapGrid myMap;
    if(x==1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test");
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        cout << "Harta citita:" << endl;
        printMap(myMap);
    }
    else if(x==2) {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations,settings.clientsCount);
        printMap(myMap);
    }

    int thick=0;//momentul initial
    int deliveries=0;
    ProceduralMapGenerator tools;

    //Simulare thick
    while (thick<=100/*settings.maxTicks*/) {
        thick++;
        if (thick%settings.spawnFrequency==0 && deliveries<settings.clientsCount) {
            //Spawn pachet in baza
        }
    }
    return 0;
}