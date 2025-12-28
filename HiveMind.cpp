#include "harta.h"

using namespace std;

int main()
{
    ProceduralMapGenerator generator;
    SimulationSettings settings=ConfigLoader::load("simulation_setup.txt");;

    MapGrid myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);

    printMap(myMap);

    return 0;
}