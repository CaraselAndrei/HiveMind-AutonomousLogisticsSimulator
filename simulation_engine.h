#pragma once
#include <vector>
#include <iostream>
#include <algorithm>
#include <cmath>
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

    // Entitati
    vector<Agent*> flota;
    vector<Pachet*> pacheteActive;      // Toate pachetele generate
    vector<Pachet*> pacheteInAsteptare; // Pachete care asteapta in baza sa fie alocate

    // Statistici
    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrate = 0;
    int currentTick = 0;

    // Generare pachete
    void spawnPackages() {
        if (currentTick % settings.spawnFrequency == 0 && pacheteActive.size() < (size_t)settings.totalPackages) {
            int randIdx = rand() % destinations.size();
            Pachet* pNou = new Pachet(pacheteActive.size(), destinations[randIdx], currentTick);
            pacheteActive.push_back(pNou);
            pacheteInAsteptare.push_back(pNou);

            // Debug (optional)
            // cout << "[SPAWN] Pachet " << pNou->id << " la Tick " << currentTick << "\n";
        }
    }

    // [NOU] Verifica compatibilitatea fara a asigna direct
    bool canAgentTakePackage(Agent* agent, Pachet* p) {
        // 1. Calculam distanta estimata (drona merge drept, robotul ocoleste)
        int distEstimata = estimateDistance(agent->pozitie, p->destinatie, agent->poateZbura());

        // 2. Calculam timpul necesar (Ticks = Distanta / Viteza)
        // Adaugam +2 ticks marja de eroare pentru pathfinding
        int timpNecesar = (distEstimata / agent->viteza) + 2;

        // 3. Verificam Deadline-ul (Cel mai important!)
        int timpRamasPachet = (p->spawnTime + p->deadline) - currentTick;

        // Daca nu poate ajunge fizic in timp util, refuza pachetul (evitam penalizarea)
        if (timpNecesar >= timpRamasPachet) {
            return false;
        }

        // 4. Calculam Profitabilitatea (Cost vs Reward)
        int costOperare = timpNecesar * agent->costPerTick;
        int profitNet = p->valoare - costOperare;

        // Daca profitul e prea mic (sau negativ), nu merita efortul
        if (profitNet < 50) return false;

        // 5. Specializare pe tipuri (Heuristica de eficienta)

        if (agent->nume == "Drona") {
            // Drona e scumpa (15 cost). O folosim DOAR daca:
            // - E foarte urgent (timp putin) SAU
            // - E foarte departe (unde robotul ar face prea mult)
            if (timpRamasPachet < 7 || distEstimata > 7) return true;
            return false; // Altfel, lasa-l la robot/scuter ca e mai ieftin
        }

        else if (agent->nume == "Scuter") {
            if (timpRamasPachet >10  && distEstimata > 15) return true;
            return false;
        }

        else { // Scuter
            // Scuterul e balansat. Ia orice "pica" intre cele doua categorii.
            // Sau ia pachete indepartate dar care nu sunt critice.
            return true;
        }
    }

    // [NOU] Calculeaza ruta spre urmatorul pachet din inventar
    void startDeliveryRoute(Agent* agent) {
        if (agent->incarcatura.empty()) return;

        // Strategie simpla: Mergem la primul pachet din lista
        // (Aici s-ar putea implementa Nearest Neighbor pentru optimizare)
        Pachet* target = agent->incarcatura.front();

        vector<Point> drum = findPath(agent->pozitie, target->destinatie, map, agent->poateZbura());
        agent->setPath(drum);
        // Nota: Nu setam pachetCurent individual, agentul stie ca are o lista "incarcatura"
    }

    // [MODIFICAT] Logica de alocare cu asteptare (Batching)
    void dispatchAgents() {
        // 1. Curatare pachete expirate din coada de asteptare
        for (auto it = pacheteInAsteptare.begin(); it != pacheteInAsteptare.end(); ) {
            if ((*it)->isExpired(currentTick)) {
                (*it)->status = PachetStatus::EXPIRAT;
                totalPenalties += 50;
                it = pacheteInAsteptare.erase(it);
            } else {
                ++it;
            }
        }

        for (auto* agent : flota) {
            // Verificam daca agentul este in baza
            bool inBase = (agent->pozitie.x == baseLocation.x && agent->pozitie.y == baseLocation.y);

            // Agentul primeste comenzi doar daca e in baza si nu incarca/mort
            if (inBase && agent->stare != AgentState::DEAD && agent->stare != AgentState::CHARGING) {

                // A. Incearca sa umple capacitatea cu pachete din asteptare
                if (agent->areLoc()) {
                    for (auto it = pacheteInAsteptare.begin(); it != pacheteInAsteptare.end(); ) {
                        Pachet* p = *it;
                        if (canAgentTakePackage(agent, p)) {
                            // Verificam daca are destula baterie (estimativ)
                            // Consideram ca are nevoie de minim 20% baterie sa plece in cursa
                            if (agent->baterie > agent->baterieMax * 0.2) {
                                agent->adaugaPachet(p);
                                p->status = PachetStatus::ALOCAT;
                                it = pacheteInAsteptare.erase(it); // Scoatem din lista de asteptare globala

                                cout << " -> [LOAD] " << agent->nume << " (" << agent->id << ") a incarcat Pachet " << p->id
                                     << ". Load: " << agent->incarcatura.size() << "/" << agent->capacitate << "\n";
                            } else {
                                ++it;
                            }

                            if (!agent->areLoc()) break; // S-a umplut, nu mai cautam
                        } else {
                            ++it;
                        }
                    }
                }

                // B. Decizia de plecare (GO vs WAIT)
                if (!agent->incarcatura.empty()) {
                    bool urgent = agent->trebuieSaPleceUrgent(currentTick);
                    bool plin = !agent->areLoc();
                    bool timeout = agent->ticksWaiting >= agent->maxWaitTicks;

                    if (urgent || plin || timeout) {
                        cout << " -> [DEPART] " << agent->nume << " (" << agent->id << ") pleaca cu " << agent->incarcatura.size() << " pachete.\n";
                        startDeliveryRoute(agent);
                    } else {
                        // Mai asteapta un tick pentru a incerca sa ia mai multe pachete
                        agent->ticksWaiting++;
                        agent->stare = AgentState::IDLE;
                    }
                }
                // C. Daca nu are nicio treaba si bateria e scazuta, il punem la incarcat
                else if (agent->baterie < agent->baterieMax) {
                    agent->stare = AgentState::CHARGING;
                }
            }
        }
    }

    // [MODIFICAT] Logica de miscare si livrare secventiala
    void moveAgents() {
        for (auto* agent : flota) {
            // Cost operational
            if (agent->stare == AgentState::MOVING) {
                totalOpCost += agent->getCost();
            }

            bool arrived = agent->update();

            if (arrived) {
                // 1. Verificam daca am ajuns la destinatia unui pachet din inventar
                bool deliveredAny = false;

                for (auto it = agent->incarcatura.begin(); it != agent->incarcatura.end(); ) {
                    Pachet* p = *it;
                    if (agent->pozitie.x == p->destinatie.x && agent->pozitie.y == p->destinatie.y) {
                        // LIVRAT!
                        p->status = PachetStatus::LIVRAT;

                        if (p->isExpired(currentTick)) totalPenalties += 50;
                        else totalRewards += p->valoare;

                        pacheteLivrate++;

                        cout << " -> [LIVRARE] " << agent->nume << " (" << agent->id << ") a livrat Pachet " << p->id << "\n";

                        it = agent->incarcatura.erase(it); // Scoatem din inventarul agentului
                        deliveredAny = true;
                    } else {
                        ++it;
                    }
                }

                // 2. Decidem urmatoarea miscare
                if (!agent->incarcatura.empty()) {
                    // Mai are pachete, calculeaza drumul spre urmatorul
                    startDeliveryRoute(agent);
                } else {
                    // Inventar gol
                    if (agent->pozitie.x == baseLocation.x && agent->pozitie.y == baseLocation.y) {
                        // E deja in baza
                        agent->stare = AgentState::IDLE;
                        agent->ticksWaiting = 0;
                    } else {
                        // Trebuie sa se intoarca
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
        std::cout << "[Engine] Memoria a fost curatata.\n";
    }

    void initialize(int mapOption) {
        // 1. Generare Harta
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

        // 2. Initializare Flota
        for (int i = 0; i < settings.dronesCount; ++i) flota.push_back(new Drona(baseLocation));
        for (int i = 0; i < settings.robotsCount; ++i) flota.push_back(new Robot(baseLocation));
        for (int i = 0; i < settings.scootersCount; ++i) flota.push_back(new Scuter(baseLocation));

        std::cout << "Engine Initializat. Baza la: " << baseLocation.x << " " << baseLocation.y << "\n";
    }

    void run() {
        cout << "=== START SIMULARE ===\n";

        for (currentTick = 0; currentTick < settings.maxTicks; currentTick++) {
            spawnPackages();
            dispatchAgents(); // Logica noua de dispatch
            moveAgents();     // Logica noua de miscare

            // Oprire conditionata
            if (pacheteLivrate >= settings.totalPackages) break;

            if (currentTick % 50 == 0) {
                std::cout << "[TICK " << currentTick << "] Livrate: " << pacheteLivrate
                          << " | Coada: " << pacheteInAsteptare.size() << "\n";
            }
        }
        saveReport();
        printReport();
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
            raportFile.close();
            cout << "[INFO] Raportul a fost salvat in 'raport_final.txt'.\n";
        } else {
            cerr << "[EROARE] Nu s-a putut crea fisierul de raport!\n";
        }
    }
};