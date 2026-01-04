#pragma once
#include <iostream>
#include <vector>
#include <random>
#include <queue>
#include "settings.h"


using MapGrid=vector<vector<char>>;
using namespace std;

class IMapGenerator {
public:
    virtual MapGrid generateMap(int rows, int cols, int stations, int clients)=0;
    virtual vector<Point> getDestinations() = 0;
    virtual vector<Point> getStations() = 0;
    virtual Point getBase()=0;
    virtual ~IMapGenerator() = default;
};

class ProceduralMapGenerator : public IMapGenerator {
private:
    mt19937 rng;
    vector<Point> destinations;
    vector<Point> station_list;
    Point base;

public:
    ProceduralMapGenerator() {
        random_device rd;
        rng.seed(rd());
    }

    vector<Point> getDestinations() override {
        return destinations;
    }

    vector<Point> getStations() override {
        return station_list;
    }

    Point getBase() override {
        return base;
    }

    MapGrid generateMap(int rows, int cols, int stations, int clients) override {
        MapGrid map;
        bool valid = false;

        while (!valid) {
            destinations.clear();
            station_list.clear();
            map = MapGrid(rows, vector<char>(cols, '.'));

            uniform_int_distribution<int> distRow(0, rows - 1);
            uniform_int_distribution<int> distCol(0, cols - 1);

            int hubR = distRow(rng);
            int hubC = distCol(rng);
            map[hubR][hubC] = 'B';
            base.x=hubR;
            base.y=hubC;

            int numWalls = (rows * cols) * 0.4; // 40% ziduri
            placeItem(map, numWalls, '#', distRow, distCol);

            placeItem(map, stations, 'S', distRow, distCol);
            placeItem(map, clients, 'D', distRow, distCol);


            if (isMapValid(map, hubR, hubC,  stations+clients)) {
                valid = true;
                cout<<"Harta generata valid!"<<endl;
            } else {
                cout << "Harta invalida (blocata)."<<endl;
            }
        }
        return map;
    }

    private:
    void placeItem(MapGrid& map, int count, char item, uniform_int_distribution<int>& dR, uniform_int_distribution<int>& dC) {
        int placed = 0;
        while (placed < count) {
            int r = dR(rng);
            int c = dC(rng);
            if (map[r][c] == '.') {
                map[r][c] = item;
                placed++;
                if (item=='D') {
                    Point d;
                    d.x=r;
                    d.y=c;
                    destinations.push_back(d);
                }
                else if (item=='S') {
                    Point s;
                    s.x=r;
                    s.y=c;
                    station_list.push_back(s);
                }
            }
        }
    }

    bool isMapValid(const MapGrid& map, int startRow, int startCol, int totalTargets) {
        int rows = map.size();
        int cols = map[0].size();

        vector<vector<bool>> visited(rows, vector<bool>(cols, false));

        queue<pair<int, int>> q;

        q.push({startRow, startCol});
        visited[startRow][startCol] = true;

        int foundTargets = 0;
        int dr[] = {-1, 1, 0, 0};
        int dc[] = {0, 0, -1, 1};

        while (!q.empty()) {
            std::pair<int, int> current = q.front();
            q.pop();
            int r = current.first;
            int c = current.second;

            for (int i = 0; i < 4; i++) {
                int nr = r + dr[i];
                int nc = c + dc[i];

                if (nr >= 0 && nr < rows && nc >= 0 && nc < cols && map[nr][nc] != '#' && !visited[nr][nc]) {
                    visited[nr][nc] = true;
                    q.push({nr, nc});
                    if (map[nr][nc] == 'D' || map[nr][nc] == 'S') {
                        foundTargets++;
                    }
                }
            }
        }
        return foundTargets == totalTargets;
    }
};

void printMap(const MapGrid& map) {
    for (const auto& row : map) {
        for (char cell : row) {
            cout<<cell<<" ";
        }
        cout<<endl;
    }
}

class FileMapLoader :public IMapGenerator{
private:
    string numeFisier;
    vector<Point> destinations;
    vector<Point> station;
    Point base;
public:

    vector<Point> getDestinations() override {
        return destinations;
    }

    vector<Point> getStations() override {
        return station;
    }

    Point getBase() override {
        return base;
    }

    FileMapLoader(const string& caleFisier) : numeFisier("harta_test") {}
    MapGrid generateMap(int rows, int cols, int stations, int clients){
        MapGrid harta(rows, vector<char>(cols, '.'));

        ifstream file("harta_test");
        if (!file.is_open()) {
            cerr<<"[EROARE] Nu s-a putut deschide fisierul:"<<numeFisier<<endl;
            cerr<<"Se returneaza o harta goala (default)."<<endl;
            return harta;
        }
        for (int i = 0; i < rows; ++i) {
            for (int j = 0; j < cols; ++j) {
                file>>harta[i][j];
                if (harta[i][j] == 'D') {
                    Point p;
                    p.x=i;
                    p.y=j;
                    destinations.push_back(p);
                }
                else if (harta[i][j] == 'S') {
                    Point p;
                    p.x=i;
                    p.y=j;
                    station.push_back(p);
                }
                else if (harta[i][j] == 'B') {
                    base.x=i;
                    base.y=j;
                }
            }
        }

        file.close();
        cout<<"Harta incarcata cu succes din "<<numeFisier<<endl;
        return harta;
    }
};