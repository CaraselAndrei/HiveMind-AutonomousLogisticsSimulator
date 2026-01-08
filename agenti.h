#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <cmath>
#include "settings.h"
#include "pachete.h" // Necesar pentru definitia Pachet

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

    // [NOU] Lista de pachete (inventar)
    vector<Pachet*> incarcatura;

    // [NOU] Variabile pentru logica de asteptare
    int ticksWaiting = 0;   // Cat timp a stat in baza asteptand sa se umple
    int maxWaitTicks = 15;  // Cat timp are voie sa astepte maxim inainte sa plece

    string nume;
    char simbol;
    int viteza;
    double baterie;       // Schimbat in double pentru precizie la incarcare
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

    // [MODIFICAT] Seteaza calea si reseteaza asteptarea
    void setPath(const vector<Point>& path) {
        currentPath.clear();
        for(const auto& pt : path) {
            currentPath.push_back(pt);
        }

        // Scoatem prima pozitie daca e cea curenta (pentru a nu sta pe loc un tick)
        if(!currentPath.empty() && currentPath.front().x == pozitie.x && currentPath.front().y == pozitie.y) {
            currentPath.pop_front();
        }

        if (!currentPath.empty()) {
            stare = AgentState::MOVING;
            ticksWaiting = 0; // Resetam asteptarea cand pleaca in cursa
        }
    }

    // [NOU] Adauga un pachet in inventar
    bool adaugaPachet(Pachet* p) {
        if (incarcatura.size() < (size_t)capacitate) {
            incarcatura.push_back(p);
            return true;
        }
        return false;
    }

    // [NOU] Verifica daca mai are loc
    bool areLoc() const {
        return incarcatura.size() < (size_t)capacitate;
    }

    // [NOU] Verifica daca trebuie sa plece urgent (se apropie deadline-ul unui pachet din inventar)
    bool trebuieSaPleceUrgent(int currentTick) {
        if (incarcatura.empty()) return false;

        for (auto* p : incarcatura) {
            int timpRamas = (p->spawnTime + p->deadline) - currentTick;
            // Estimam distanta Manhattan pana la destinatie
            int dist = abs(pozitie.x - p->destinatie.x) + abs(pozitie.y - p->destinatie.y);

            // Daca timpul ramas e periculos de mic (distanta + marja de siguranta 5 tick-uri)
            if (timpRamas <= dist + 5) return true;
        }
        return false;
    }

    // [MODIFICAT] Functia de update (consuma baterie, misca agentul)
    // Returneaza true daca a terminat drumul curent (a ajuns la o destinatie)
    bool update() {
        if (stare == AgentState::DEAD) return false;

        if (baterie <= 0) {
            stare = AgentState::DEAD;
            cout << "[CRITIC] " << nume << " (" << id << ") a ramas fara baterie si a murit!\n";
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
                // A ajuns la destinatia curenta (poate fi un client sau baza)
                return true;
            }
        }
        else if (stare == AgentState::CHARGING) {
            baterie += (baterieMax * 0.25); // Incarca 25% pe tick
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
        : Agent("Drona", '^', 3, 100, 10, 15, 1, startPos) {} // Capacitate mica (1), viteza mare

    bool poateZbura() const override {
        return true;
    }
};

class Robot : public Agent {
public:
    Robot(Point startPos)
        : Agent("Robot", 'R', 1, 300, 2, 1, 4, startPos) {} // Capacitate mare (4), viteza mica

    bool poateZbura() const override {
        return false;
    }
};

class Scuter : public Agent {
public:
    Scuter(Point startPos)
        : Agent("Scuter", 'S', 2, 200, 5, 4, 2, startPos) {} // Mediu (2)

    bool poateZbura() const override {
        return false;
    }
};

int estimateDistance(Point a, Point b, bool canFly) {
    int dist = abs(a.x - b.x) + abs(a.y - b.y);
    if (canFly) return dist;
    return (int)(dist * 2.5);
}