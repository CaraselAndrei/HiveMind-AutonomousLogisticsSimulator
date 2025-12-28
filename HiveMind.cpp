#include "harta.h"

using namespace std;

int main()
{
    ProceduralMapGenerator generator;

    MapGrid myMap = generator.generateMap(10, 20);

    printMap(myMap);

    return 0;
}