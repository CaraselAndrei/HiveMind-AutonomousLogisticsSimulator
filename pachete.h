#pragma once
#include <iostream>
#include <random>
#include <vector>
#include "harta.h"
#include "settings.h"

using namespace std;

enum class PachetStatus {
    NEALOCAT,
    ALOCAT,
    LIVRAT,
    EXPIRAT
};

class Pachet {
public:
    int id;
    int valoare;      // Reward 200-800
    int deadline;     // Durata maxima (10-20 ticks)
    int spawnTime;    // Tick-ul in care a aparut
    Point destinatie; // Locatia clientului
    PachetStatus status;

    Pachet(int id, Point dest, int time) : id(id), destinatie(dest), spawnTime(time), status(PachetStatus::NEALOCAT) {
        static mt19937 rng(random_device{}());
        uniform_int_distribution<int> distVal(200, 800); //
        uniform_int_distribution<int> distDeadline(10, 20); //

        valoare = distVal(rng);
        deadline = distDeadline(rng);
    }

    bool isExpired(int currentTick) const {
        return currentTick > (spawnTime + deadline);
    }
};