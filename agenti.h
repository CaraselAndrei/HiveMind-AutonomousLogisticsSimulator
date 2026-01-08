#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <cmath>
#include "settings.h"
#include "pachete.h"

using namespace std;

enum class AgentState {
    IDLE,
    MOVING,
    CHARGING,
    DEAD
};

int globalId = 0;

class Agent {
public:
    deque<Point> currentPath;

    vector<Pachet*> incarcatura;

    int ticksWaiting = 0;
    int maxWaitTicks = 15;

    string nume;
    char simbol;
    int viteza;
    double baterie;
    int baterieMax;
    int consumBaterie;
    int costPerTick;
    int capacitate;
    const int id;
    Point pozitie;
    AgentState stare;

    Agent(string n, char s, int v, int bMax, int cons, int cost, int cap, Point startPos)
        : id(globalId++),
          nume(n), simbol(s), viteza(v), baterieMax(bMax), baterie(bMax),
          consumBaterie(cons), costPerTick(cost), capacitate(cap),
          pozitie(startPos), stare(AgentState::IDLE) {}

    virtual ~Agent() = default;

    virtual bool poateZbura() const = 0;

    void setPath(const vector<Point>& path) {
        currentPath.clear();
        for(const auto& pt : path) {
            currentPath.push_back(pt);
        }

        if(!currentPath.empty() && currentPath.front().x == pozitie.x && currentPath.front().y == pozitie.y) {
            currentPath.pop_front();
        }

        if (!currentPath.empty()) {
            stare = AgentState::MOVING;
            ticksWaiting = 0;
        }
    }

    bool adaugaPachet(Pachet* p) {
        if (incarcatura.size() < (size_t)capacitate) {
            incarcatura.push_back(p);
            return true;
        }
        return false;
    }

    bool areLoc() const {
        return incarcatura.size() < (size_t)capacitate;
    }

    bool trebuieSaPleceUrgent(int currentTick) {
        if (incarcatura.empty()) return false;

        for (auto* p : incarcatura) {
            int timpRamas = (p->spawnTime + p->deadline) - currentTick;
            int dist = abs(pozitie.x - p->destinatie.x) + abs(pozitie.y - p->destinatie.y);

            if (timpRamas <= dist + 5) return true;
        }
        return false;
    }

    bool update() {
        if (stare == AgentState::DEAD) return false;

        if (baterie <= 0) {
            stare = AgentState::DEAD;
            cout << "[CRITIC] " << nume << " (" << id << ") a ramas fara baterie si a murit!\n";
            return false;
        }

        if (stare == AgentState::MOVING) {
            for(int i = 0; i < viteza && !currentPath.empty(); ++i) {
                if (baterie < consumBaterie) {
                    stare = AgentState::DEAD;
                    break;
                }
                pozitie = currentPath.front();
                currentPath.pop_front();
                baterie -= consumBaterie;
            }

            if (currentPath.empty() && stare != AgentState::DEAD) {
                return true;
            }
        }
        else if (stare == AgentState::CHARGING) {
            baterie += (baterieMax * 0.25);
            if (baterie >= baterieMax) {
                baterie = baterieMax;
                stare = AgentState::IDLE;
            }
        }

        return false;
    }

    char getSimbol() const { return simbol; }
    Point getPozitie() const { return pozitie; }
    bool isDead() const { return stare == AgentState::DEAD; }
    int getCost() const { return costPerTick; }
};

class Drona : public Agent {
public:
    Drona(Point startPos)
        : Agent("Drona", '^', 3, 100, 10, 15, 1, startPos) {}

    bool poateZbura() const override {
        return true;
    }
};

class Robot : public Agent {
public:
    Robot(Point startPos)
        : Agent("Robot", 'R', 1, 300, 2, 1, 4, startPos) {}

    bool poateZbura() const override {
        return false;
    }
};

class Scuter : public Agent {
public:
    Scuter(Point startPos)
        : Agent("Scuter", 'S', 2, 200, 5, 4, 2, startPos) {}

    bool poateZbura() const override {
        return false;
    }
};

int estimateDistance(Point a, Point b, bool canFly) {
    int dist = abs(a.x - b.x) + abs(a.y - b.y);
    if (canFly) return dist;
    return (int)(dist * 2.5);
}