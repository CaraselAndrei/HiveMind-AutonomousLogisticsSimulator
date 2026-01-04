#pragma once
#include <iostream>
#include <string>
#include <vector>

using namespace std;

enum class AgentState {
    IDLE,
    MOVING,
    CHARGING,
    DEAD
};

struct Point {
    int x, y;
};

class Agent {
protected:
    string nume;
    char simbol;
    int viteza;
    int baterie;
    int baterieMax;
    int consumBaterie;
    int costPerTick;
    int capacitate;
    Point pozitie;
    AgentState stare;


public:
    Agent(string n, char s, int v, int bMax, int cons, int cost, int cap, Point startPos)
        : nume(n), simbol(s), viteza(v), baterieMax(bMax), baterie(bMax),
          consumBaterie(cons), costPerTick(cost), capacitate(cap),
          pozitie(startPos), stare(AgentState::IDLE) {}

    virtual ~Agent() = default;

    virtual bool poateZbura() const = 0;

    virtual void update() {
        if (stare == AgentState::DEAD) return;
        if (baterie <= 0) {
            stare = AgentState::DEAD;
            cout << nume << " a ramas fara baterie si a murit!\n";
            return;
        }
        switch (stare) {
            case AgentState::IDLE:
                // Așteaptă comenzi. Dacă e în bază, se încarcă.
                break;

            case AgentState::MOVING:
                baterie -= consumBaterie;
                break;

            case AgentState::CHARGING:
                baterie +=(baterieMax*0.25);
                if (baterie >= baterieMax) {
                    baterie = baterieMax;
                    stare = AgentState::IDLE;
                }
                break;

             case AgentState::DEAD:
                break;
        }
        // Scădem costul operațional (doar dacă e viu)
        // Profitul scade cu costPerTick
    }

    char getSimbol() const { return simbol; }
    Point getPozitie() const { return pozitie; }
    bool isDead() const { return stare == AgentState::DEAD; }
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