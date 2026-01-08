#pragma once
#include "settings.h"
#include "harta.h"
#include "agenti.h"
#include "pachete.h"

using namespace std;

class SimulationEngine {
private:
    SimulationSettings settings;
    MapGrid map;
    Point baseLocation;
    vector<Point> destinations;
    vector<Point> stations;

    vector<Agent*> flota;
    vector<Pachet*> pacheteActive;
    vector<Pachet*> pacheteInAsteptare;
    vector<Pachet*> pacheteExpirate;

    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrate = 0;
    int currentTick = 0;

    int estimateDistance(Point a, Point b, bool canFly) {
        int dist = abs(a.x - b.x) + abs(a.y - b.y);
        if (canFly) return dist;
        return (int)(dist * 2);//distanta aproximativa pentru vehicule terestre (*2 obtinut prin trial and error, cele mai mari profituri s-au obtinut cu el)
    }

    void spawnPackages() {
        if (currentTick % settings.spawnFrequency == 0 && pacheteActive.size() < (size_t)settings.totalPackages) {
            int randIdx = rand() % destinations.size();
            Pachet* pNou = new Pachet(pacheteActive.size(), destinations[randIdx], currentTick);
            pacheteActive.push_back(pNou);
            pacheteInAsteptare.push_back(pNou);
        }
    }

    bool canAgentTakePackage(Agent* agent, Pachet* p) {
        int distEstimata = estimateDistance(agent->pozitie, p->destinatie, agent->poateZbura());
        int timpNecesar = (distEstimata / agent->viteza) + 2;
        int timpRamasPachet = (p->spawnTime + p->deadline) - currentTick;

        if (timpNecesar >= timpRamasPachet) {
            return false;//prioritizam pachetele care pot fi livrate la timp
        }

        int costOperare = timpNecesar * agent->costPerTick;
        int profitNet = p->valoare - costOperare;

        if (profitNet < 50) return false; // prioritizam profitul mai mare ca 50
        return true;

        //Metoda neimplementata
        //E greu de gasit valori astfel incat secventa urmatoare sa dea randament bun pe orice harta
        /*
        if (agent->nume == "Drona") {
            if (timpRamasPachet < 7 || distEstimata > 6) return true;
            return false;
        }
        else if (agent->nume == "Scuter") {
            if (timpRamasPachet > 10 && distEstimata > 15) return true;
            return false;
        }
        else {
            return true;//lasam la roboti distantele medii
        }*/
    }

    void startDeliveryRoute(Agent* agent) {
        if (agent->incarcatura.empty()) return;

        Pachet* target = agent->incarcatura.front();
        vector<Point> drum = findPath(agent->pozitie, target->destinatie, map, agent->poateZbura());
        agent->setPath(drum);
    }

    void dispatchAgents() {
        for (auto it = pacheteInAsteptare.begin(); it != pacheteInAsteptare.end(); ) {
            if ((*it)->isExpired(currentTick)) {
                (*it)->status = PachetStatus::EXPIRAT;
                pacheteExpirate.push_back(*it);
                it = pacheteInAsteptare.erase(it);
                totalPenalties+=50;
            } else {
                ++it;
            }
        }

        for (auto* agent : flota) {
            bool inBase = (agent->pozitie.x == baseLocation.x && agent->pozitie.y == baseLocation.y);

            if (inBase && agent->stare != AgentState::DEAD && agent->stare != AgentState::CHARGING) {
                if (agent->areLoc() && !pacheteInAsteptare.empty()) {
                    for (auto it = pacheteInAsteptare.begin(); it != pacheteInAsteptare.end(); ) {
                        Pachet* p = *it;
                        if (canAgentTakePackage(agent, p)) {
                            if (agent->baterie > agent->baterieMax * 0.2) {
                                agent->adaugaPachet(p);
                                p->status = PachetStatus::ALOCAT;
                                it = pacheteInAsteptare.erase(it);
                            } else {
                                ++it;
                            }

                            if (!agent->areLoc()) break;
                        } else {
                            ++it;
                        }
                    }
                }

                if (agent->areLoc() && pacheteInAsteptare.empty() && !pacheteExpirate.empty()) {
                    for (auto it = pacheteExpirate.begin(); it != pacheteExpirate.end(); ) {
                        Pachet* p = *it;
                        int dist = estimateDistance(agent->pozitie, p->destinatie, agent->poateZbura());
                        int costDrum = dist * agent->getCost();

                        if (agent->baterie > agent->baterieMax * 0.2 && p->valoare > costDrum) {
                            agent->adaugaPachet(p);
                            p->status = PachetStatus::ALOCAT;
                            it = pacheteExpirate.erase(it);
                        } else {
                            ++it;
                        }

                        if (!agent->areLoc()) break;
                    }
                }

                if (!agent->incarcatura.empty()) {
                    bool urgent = agent->trebuieSaPleceUrgent(currentTick);
                    bool plin = !agent->areLoc();
                    bool timeout = agent->ticksWaiting >= agent->maxWaitTicks;
                    bool backlogMode = pacheteInAsteptare.empty() && !pacheteExpirate.empty();

                    if (urgent || plin || timeout || backlogMode) {
                        startDeliveryRoute(agent);
                    } else {
                        agent->ticksWaiting++;
                        agent->stare = AgentState::IDLE;
                    }
                }
                else if (agent->baterie < agent->baterieMax) {
                    agent->stare = AgentState::CHARGING;
                }
            }
        }
    }

    void moveAgents() {
        for (auto* agent : flota) {
            if (agent->stare == AgentState::MOVING) {
                totalOpCost += agent->getCost();
            }

            bool arrived = agent->update();

            if (arrived) {
                bool deliveredAny = false;

                for (auto it = agent->incarcatura.begin(); it != agent->incarcatura.end(); ) {
                    Pachet* p = *it;
                    if (agent->pozitie.x == p->destinatie.x && agent->pozitie.y == p->destinatie.y) {
                        p->status = PachetStatus::LIVRAT;

                        if (p->isExpired(currentTick)) totalPenalties += 50;
                        else totalRewards += p->valoare;

                        pacheteLivrate++;

                        it = agent->incarcatura.erase(it);
                        deliveredAny = true;
                    } else {
                        ++it;
                    }
                }

                if (!agent->incarcatura.empty()) {
                    startDeliveryRoute(agent);
                } else {
                    if (agent->pozitie.x == baseLocation.x && agent->pozitie.y == baseLocation.y) {
                        agent->stare = AgentState::IDLE;
                        agent->ticksWaiting = 0;
                    } else {
                        returnToBaseOrCharge(agent);
                    }
                }
            }
        }
    }

    void returnToBaseOrCharge(Agent* agent) {
        vector<Point> drumBaza = findPath(agent->pozitie, baseLocation, map, agent->poateZbura());
        agent->setPath(drumBaza);
    }

public:
    SimulationEngine() {
        settings = ConfigLoader::load("simulation_setup.txt");
    }

    ~SimulationEngine() {
        for (auto* a : flota) delete a;
        for (auto* p : pacheteActive) delete p;
        flota.clear();
        pacheteActive.clear();
        pacheteInAsteptare.clear();
        pacheteExpirate.clear();
    }

    void initialize(int mapOption) {
        IMapGenerator* generator = nullptr;
        ProceduralMapGenerator procGen;

        if (mapOption == 1) {
            generator = new FileMapLoader("harta_test");
            map = generator->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
            destinations = generator->getDestinations();
            stations = generator->getStations();
            baseLocation = generator->getBase();
            delete generator;
        } else {
            map = procGen.generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
            destinations = procGen.getDestinations();
            stations = procGen.getStations();
            baseLocation = procGen.getBase();
        }

        for (int i = 0; i < settings.dronesCount; ++i) flota.push_back(new Drona(baseLocation));
        for (int i = 0; i < settings.robotsCount; ++i) flota.push_back(new Robot(baseLocation));
        for (int i = 0; i < settings.scootersCount; ++i) flota.push_back(new Scuter(baseLocation));
    }

    void run() {
        for (currentTick = 0; currentTick < settings.maxTicks; currentTick++) {
            spawnPackages();
            dispatchAgents();
            moveAgents();

            if (pacheteLivrate >= settings.totalPackages) break;

           /* if (currentTick % 50 == 0) {
                cout << "[TICK " << currentTick << "] Livrate: " << pacheteLivrate
                          << " | Coada: " << pacheteInAsteptare.size() << "\n";
            }*/
        }
        saveReport();
        //printReport();
    }

    void printReport(){
        long long profit = totalRewards - totalOpCost - totalPenalties;
        cout << "\n=== RAPORT FINAL ===\n";
        cout << "Profit: " << profit << "\n";
        cout << "Livrate: " << pacheteLivrate << "/" << settings.totalPackages << "\n";
    }

    void saveReport() {
        long long profit = totalRewards - totalOpCost - totalPenalties;
        ofstream raportFile("raport_final.txt");
        if (raportFile.is_open()) {
            for (const auto& row : map) {
                for (char cell : row) {
                    raportFile << cell << " ";
                }
                raportFile << endl;
            }
            raportFile << "=== RAPORT FINAL ===\n";
            raportFile << "Pachete Livrate: " << pacheteLivrate << "/" << pacheteActive.size() << endl;
            raportFile << "Total Rewards: " << totalRewards << endl;
            raportFile << "Total Cost Operare: " << totalOpCost << endl;
            raportFile << "Total Penalizari: " << totalPenalties << endl;
            raportFile << "----------------------------\n";
            raportFile << "PROFIT NET: " << profit << " credite\n";
            raportFile << "OBSERVATIE: Nu se livreaza toate pachetele, doarece acele pachete nu genereaza profit.\n";
            raportFile << "LINK GITHUB: https://github.com/CaraselAndrei/HiveMind-AutonomousLogisticsSimulator\n";
            raportFile.close();
            raportFile.close();
        }
    }
};