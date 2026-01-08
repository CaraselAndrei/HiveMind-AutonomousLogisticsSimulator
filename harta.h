#pragma once
#include <iostream>
#include <vector>
#include <random>
#include <queue>
#include "settings.h"
#include <cmath>
#include <algorithm>
#include <map>
#include <fstream>

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

    FileMapLoader(const string& caleFisier) : numeFisier(caleFisier) {}
    MapGrid generateMap(int rows, int cols, int stations, int clients){
        MapGrid harta(rows, vector<char>(cols, '.'));

        ifstream file(numeFisier);
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

class ProceduralMapGenerator : public IMapGenerator {
private:
    mt19937 rng;

public:
    vector<Point> destinations;
    vector<Point> station_list;
    Point base;

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

            int numWalls = (rows * cols) * 0.4;
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

struct Node {
    Point pos;
    int g, h;

    int f() const { return g + h; }

    bool operator>(const Node& other) const {
        return f() > other.f();
    }
};

inline int dist(const Point& a, const Point& b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

vector<Point> findPath(Point start, Point goal, const MapGrid& harta, bool canFly) {
    int rows = harta.size();
    int cols = harta[0].size();

    if (start.x < 0 || start.x >= rows || start.y < 0 || start.y >= cols ||
        goal.x < 0 || goal.x >= rows || goal.y < 0 || goal.y >= cols) {
        return {};
    }

    vector<vector<bool>> visited(rows, vector<bool>(cols, false));

    priority_queue<Node, vector<Node>, greater<Node>> openSet;

    Node startNode;
    startNode.pos = start;
    startNode.g = 0;
    startNode.h = dist(start, goal);
    openSet.push(startNode);

    map<pair<int,int>, Point> cameFrom;

    map<pair<int,int>, int> gScore;
    gScore[{start.x, start.y}] = 0;

    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};

    while (!openSet.empty()) {
        Node current = openSet.top();
        openSet.pop();

        if (current.pos.x == goal.x && current.pos.y == goal.y) {
            vector<Point> path;
            Point p = goal;

            while (true) {
                path.push_back(p);
                if (p.x == start.x && p.y == start.y) break;

                auto it = cameFrom.find({p.x, p.y});
                if (it == cameFrom.end()) break;
                p = it->second;
            }

            reverse(path.begin(), path.end());
            return path;
        }

        if (visited[current.pos.x][current.pos.y]) continue;
        visited[current.pos.x][current.pos.y] = true;

        for (int i = 0; i < 4; i++) {
            int nx = current.pos.x + dx[i];
            int ny = current.pos.y + dy[i];

            if (nx < 0 || nx >= rows || ny < 0 || ny >= cols) continue;
            if (!canFly && harta[nx][ny] == '#') continue;
            if (visited[nx][ny]) continue;

            int tentativeG = current.g + 1;
            pair<int, int> neighborKey = {nx, ny};

            if (gScore.find(neighborKey) == gScore.end() || tentativeG < gScore[neighborKey]) {
                gScore[neighborKey] = tentativeG;
                cameFrom[neighborKey] = current.pos;

                Node neighbor;
                neighbor.pos = {nx, ny};
                neighbor.g = tentativeG;
                neighbor.h = dist(neighbor.pos, goal);

                openSet.push(neighbor);
            }
        }
    }

    return {};
}