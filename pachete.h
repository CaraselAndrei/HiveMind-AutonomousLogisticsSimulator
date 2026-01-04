#pragma once
#include <iostream>
#include <random>
#include <vector>
#include "harta.h"

using namespace std;

struct Point {
    int x, y;
};

class Pachet {
    int value,deadline;
    Client destination;
    mt19937 rng;

    public:
    Pachet() {
        uniform_int_distribution<int> v(200,800);
        uniform_int_distribution<int> d(10,20);
        value = v(rng);
        deadline = d(rng);

    }
};