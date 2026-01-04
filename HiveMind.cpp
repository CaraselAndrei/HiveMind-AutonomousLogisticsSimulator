#include <algorithm>

#include "harta.h"

using namespace std;

int main()
{
    ProceduralMapGenerator generator;
    SimulationSettings settings = ConfigLoader::load("simulation_setup.txt");
    vector<Point> destinatiiFinale;

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
        destinatiiFinale = generatorHarta->getDestinations();
        cout << "Harta citita:" << endl;
        printMap(myMap);
    }
    else if(x==2) {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations,settings.clientsCount);
        destinatiiFinale = generator.getDestinations();
        printMap(myMap);
    }

    int thick=0;//momentul initial
    int deliveries=0;
    ProceduralMapGenerator tools;
    cout << "\nLista destinatii (Total: " << destinatiiFinale.size() << "):" << endl;
    for(size_t i = 0; i < destinatiiFinale.size(); i++) {
        cout << "Destinatia " << i + 1 << ": [" << destinatiiFinale[i].x << ", " << destinatiiFinale[i].y << "]" << endl;
    }

    //Simulare thick

    return 0;
}