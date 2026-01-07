#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include "settings.h"

using namespace std;

// Forward declaration
class Pachet;

enum class AgentState {
    IDLE,
    MOVING,
    CHARGING,
    DEAD
};

int globalId=0;

class Agent {
public:
    deque<Point> currentPath;
    Pachet* pachetCurent = nullptr;

    string nume;
    char simbol;
    int viteza;
    int baterie;
    int baterieMax;
    int consumBaterie;
    int costPerTick;
    int capacitate;
    const int id=0;
    Point pozitie;
    AgentState stare;

    Agent(string n, char s, int v, int bMax, int cons, int cost, int cap, Point startPos)
        : id(globalId++), // 3. Atribuim ID-ul curent si incrementam contorul
          nume(n), simbol(s), viteza(v), baterieMax(bMax), baterie(bMax),
          consumBaterie(cons), costPerTick(cost), capacitate(cap),
          pozitie(startPos), stare(AgentState::IDLE) {}

    virtual ~Agent() = default;

    virtual bool poateZbura() const = 0;

    void setPath(const vector<Point>& path, Pachet* p = nullptr) {
        currentPath.clear();
        for(const auto& pt : path) {
            currentPath.push_back(pt);
        }

        // Scoatem prima pozitie daca e cea curenta
        if(!currentPath.empty() && currentPath.front().x == pozitie.x && currentPath.front().y == pozitie.y) {
            currentPath.pop_front();
        }

        if (!currentPath.empty()) {
            stare = AgentState::MOVING;
            pachetCurent = p;
        }
    }

    bool hasPackage() const {
        return pachetCurent != nullptr;
    }

    // Returneaza true daca a terminat drumul in acest tick
    bool update() {
        if (stare == AgentState::DEAD) return false;

        if (baterie <= 0) {
            stare = AgentState::DEAD;
            cout << nume << " a ramas fara baterie si a murit!\n";
            return false;
        }

        if (stare == AgentState::MOVING) {
            // Miscare in functie de viteza (nr de celule per tick)
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
                stare = AgentState::IDLE;
                return true; // A ajuns la destinatie
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