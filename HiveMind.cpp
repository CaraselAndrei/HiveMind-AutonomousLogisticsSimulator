#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
#include <cmath>       // Necesar pentru abs()
#include "harta.h"
#include "settings.h"
#include "agenti.h"
#include "pachete.h"

using namespace std;

int main()
{
    // 1. Initializare Setari
    ProceduralMapGenerator generator;
    SimulationSettings settings = ConfigLoader::load("simulation_setup.txt");

    vector<Point> destinatii;
    vector<Point> statii;
    Point base;
    MapGrid myMap;

    // 2. Selectie Harta
    cout << "Selecteaza optiunea:\n";
    cout << "1. Citeste din fisier.\n";
    cout << "2. Genereaza harta procedural.\n";
    int x = 2;
    // cin >> x; // Poti decomenta pentru input manual

    if (x == 1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test");
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generatorHarta->getDestinations();
        statii = generatorHarta->getStations();
        base = generatorHarta->getBase();
        cout << "Harta citita din fisier." << endl;
        // printMap(myMap); // Optional: afisare harta
        delete generatorHarta;
    }
    else {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generator.getDestinations();
        statii = generator.getStations();
        base = generator.getBase();
        cout << "Harta generata procedural." << endl;
    }

    cout << "\nLista destinatii (Total: " << destinatii.size() << "):" << endl;
    for (size_t i = 0; i < destinatii.size(); i++) {
        cout << "Destinatia " << i + 1 << ": [" << destinatii[i].x << ", " << destinatii[i].y << "]" << endl;
    }

    // 3. Initializare Flota
    vector<Agent*> flota;
    for (int i = 0; i < settings.dronesCount; ++i) flota.push_back(new Drona(base));
    for (int i = 0; i < settings.robotsCount; ++i) flota.push_back(new Robot(base));
    for (int i = 0; i < settings.scootersCount; ++i) flota.push_back(new Scuter(base));

    // 4. Structuri de Date pentru Pachete
    vector<Pachet*> pacheteActive;      // Evidenta totala
    vector<Pachet*> pacheteInAsteptare; // Coada de dispatch

    // Variabile pentru Scoring
    long long profitTotal = 0;
    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrateCounter = 0;

    // ==========================================
    // 5. BUCLA PRINCIPALA (SIMULATION LOOP)
    // ==========================================
    for (int tick = 0; tick < settings.maxTicks; tick++) {

        // --- 0. LOGICA AUTOMATA: Retur la baza dupa incarcare ---
        // Verificam daca un agent s-a incarcat complet intr-o statie si sta degeaba
        for(auto* agent : flota) {
            if (agent->stare == AgentState::IDLE && agent->baterie == agent->baterieMax) {
                // Daca e plin, dar nu e la baza, il chemam acasa
                if (agent->pozitie.x != base.x || agent->pozitie.y != base.y) {
                    vector<Point> drumAcasa = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                    if (!drumAcasa.empty()) {
                        agent->setPath(drumAcasa);
                        // cout << " -> [HOME] " << agent->nume << " (" << agent->id << ") s-a incarcat si revine la baza.\n";
                    }
                }
            }
        }

        // --- A. GENERARE PACHETE (Spawn) ---
        if (tick % settings.spawnFrequency == 0 && pacheteActive.size() < (size_t)settings.totalPackages) {
            int randIdx = rand() % destinatii.size();

            Pachet* pNou = new Pachet(pacheteActive.size(), destinatii[randIdx], tick);
            pacheteActive.push_back(pNou);
            pacheteInAsteptare.push_back(pNou);

            cout << "[TICK " << tick << "] Pachet NOU #" << pNou->id << " (Val: " << pNou->valoare << ", Deadline: " << pNou->deadline << ")" << endl;

            // Sortam de baza (Safety), desi logica Smart Dispatch va face selectia principala
            sort(pacheteInAsteptare.begin(), pacheteInAsteptare.end(), [](Pachet* a, Pachet* b) {
                return (a->spawnTime + a->deadline) < (b->spawnTime + b->deadline);
            });
        }

        // --- B. STRATEGIE ALOCARE (Pentru agentii care sunt DEJA IDLE in baza) ---
        if (!pacheteInAsteptare.empty()) {
            for (auto* agent : flota) {
                if (pacheteInAsteptare.empty()) break;

                // Verificam agentii liberi din baza
                if (agent->stare == AgentState::IDLE && !agent->isDead() &&
                    agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                    // --- SMART DISPATCH LOGIC (START) ---
                    int bestPachetIdx = -1;
                    for (int i = 0; i < pacheteInAsteptare.size(); ++i) {
                        Pachet* p = pacheteInAsteptare[i];

                        // 1. Curatenie Expired
                        if (p->isExpired(tick)) {
                            p->status = PachetStatus::EXPIRAT;
                            totalPenalties += 50;
                            pacheteInAsteptare.erase(pacheteInAsteptare.begin() + i);
                            i--; continue;
                        }

                        // 2. Calcul parametrii
                        int dist = abs(agent->pozitie.x - p->destinatie.x) + abs(agent->pozitie.y - p->destinatie.y);
                        int timpRamas = (p->spawnTime + p->deadline) - tick;

                        // 3. Matching
                        bool isMatch = false;
                        if (agent->nume == "Drona") {
                            if (timpRamas < 15 || dist > 15) isMatch = true; // Urgent sau departe
                        } else if (agent->nume == "Robot") {
                            if (timpRamas > 20) isMatch = true; // Relaxat
                        } else {
                            isMatch = true; // Scuter ia restul
                        }

                        // 4. Fezabilitate
                        if (isMatch) {
                            vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());
                            int energieNecesara = drum.size() * agent->consumBaterie;

                            // Verificam daca are baterie dus-intors (aproximativ) + marja
                            if (!drum.empty() && agent->baterie > (energieNecesara * 2.2)) {
                                bestPachetIdx = i;
                                agent->setPath(drum, p);
                                p->status = PachetStatus::ALOCAT;
                                cout << " -> [DISPATCH B] " << agent->nume << " (" << agent->id << ") a luat pachetul "
                                     << p->id << " (Timp ramas: " << timpRamas << ")" << endl;
                                break;
                            }
                        }
                    }
                    if (bestPachetIdx != -1) {
                        pacheteInAsteptare.erase(pacheteInAsteptare.begin() + bestPachetIdx);
                    }
                    // --- SMART DISPATCH LOGIC (END) ---
                }
            }
        }

        // --- C. UPDATE AGENTI SI LOGICA DE MISCARE ---
        for (auto* agent : flota) {

            // Cost doar daca se misca
            if (!agent->isDead() && agent->stare == AgentState::MOVING) {
                totalOpCost += agent->getCost();
            }

            bool aTerminatDrumul = agent->update();

            // SCENARIUL 1: LIVRARE REUSITA
            if (aTerminatDrumul && agent->hasPackage()) {
                Pachet* p = agent->pachetCurent;

                if (p->isExpired(tick)) {
                    totalPenalties += 50;
                    cout << " -> [LIVRARE] Pachet " << p->id << " TARZIU.\n";
                } else {
                    totalRewards += p->valoare;
                    cout << " -> [LIVRARE] Pachet " << p->id << " SUCCES! (+" << p->valoare << ")\n";
                }
                p->status = PachetStatus::LIVRAT;
                agent->pachetCurent = nullptr;
                pacheteLivrateCounter++;

                // --- LOGICA RETUR: Cea mai apropiata Statie sau Baza ---
                Point punctDestinatie = base;
                vector<Point> drumOptim;
                int distantaMinima = 999999;

                // 1. Calcul drum spre Baza
                vector<Point> drumBaza = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                if (!drumBaza.empty()) {
                    distantaMinima = drumBaza.size();
                    drumOptim = drumBaza;
                    punctDestinatie = base;
                }

                // 2. Calcul drum spre Statii
                for (const auto& statie : statii) {
                    vector<Point> drumStatie = findPath(agent->pozitie, statie, myMap, agent->poateZbura());
                    if (!drumStatie.empty() && drumStatie.size() < (size_t)distantaMinima) {
                        distantaMinima = drumStatie.size();
                        drumOptim = drumStatie;
                        punctDestinatie = statie;
                    }
                }

                if (!drumOptim.empty()) {
                    agent->setPath(drumOptim);
                    string destinatieNume = (punctDestinatie.x == base.x && punctDestinatie.y == base.y) ? "BAZA" : "STATIE";
                    cout << " -> [RETUR] " << agent->nume << " (" << agent->id << ") merge la " << destinatieNume << endl;
                }
            }

            // SCENARIUL 2: AJUNS LA DESTINATIE FARA PACHET
            else if (aTerminatDrumul && !agent->hasPackage()) {

                // CAZ A: A AJUNS LA BAZA -> Start Dispatching
                if (agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                    if (!pacheteInAsteptare.empty()) {

                        // --- SMART DISPATCH LOGIC (COPY - START) ---
                        // (Aceeasi logica ca in sectiunea B pentru consistenta)
                        int bestPachetIdx = -1;
                        for (int i = 0; i < pacheteInAsteptare.size(); ++i) {
                            Pachet* p = pacheteInAsteptare[i];

                            if (p->isExpired(tick)) {
                                p->status = PachetStatus::EXPIRAT;
                                totalPenalties += 50;
                                pacheteInAsteptare.erase(pacheteInAsteptare.begin() + i);
                                i--; continue;
                            }

                            int dist = abs(agent->pozitie.x - p->destinatie.x) + abs(agent->pozitie.y - p->destinatie.y);
                            int timpRamas = (p->spawnTime + p->deadline) - tick;

                            bool isMatch = false;
                            if (agent->nume == "Drona") {
                                if (timpRamas < 15 || dist > 15) isMatch = true;
                            } else if (agent->nume == "Robot") {
                                if (timpRamas > 20) isMatch = true;
                            } else {
                                isMatch = true;
                            }

                            if (isMatch) {
                                vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());
                                int energieNecesara = drum.size() * agent->consumBaterie;

                                if (!drum.empty() && agent->baterie > (energieNecesara * 2.2)) {
                                    bestPachetIdx = i;
                                    agent->setPath(drum, p);
                                    p->status = PachetStatus::ALOCAT;
                                    cout << " -> [INSTANT C] " << agent->nume << " (" << agent->id << ") a luat pachetul "
                                         << p->id << " (Urgent/Match)" << endl;
                                    break;
                                }
                            }
                        }
                        if (bestPachetIdx != -1) {
                            pacheteInAsteptare.erase(pacheteInAsteptare.begin() + bestPachetIdx);
                        } else {
                            // Daca nu gasim pachet potrivit, dar bateria nu e plina, incarcam
                            if(agent->baterie < agent->baterieMax) agent->stare = AgentState::CHARGING;
                        }
                        // --- SMART DISPATCH LOGIC (COPY - END) ---
                    }
                    else {
                        // Niciun pachet in asteptare, incarcam daca e cazul
                        if(agent->baterie < agent->baterieMax) agent->stare = AgentState::CHARGING;
                    }
                }

                // CAZ B: A AJUNS LA O STATIE -> Start Charging
                else {
                    // Verificam daca e chiar o statie (pentru siguranta)
                    bool eStatie = false;
                    for(auto& s : statii) {
                        if(s.x == agent->pozitie.x && s.y == agent->pozitie.y) { eStatie = true; break; }
                    }
                    if(eStatie) {
                        agent->stare = AgentState::CHARGING;
                        // cout << " -> [CHARGING] " << agent->nume << " se incarca la statie.\n";
                    }
                }
            }
        } // Sfarsit loop agenti

        // Raport periodic
        if (tick % 50 == 0) {
            cout << "[STATUS TICK " << tick << "] Rewards: " << totalRewards
                 << " | Cost: " << totalOpCost << " | Penalties: " << totalPenalties
                 << " | In Coada: " << pacheteInAsteptare.size() << endl;
        }

        // Oprire daca s-au livrat toate pachetele
        if(pacheteLivrateCounter >= settings.totalPackages && pacheteActive.size() >= (size_t)settings.totalPackages) {
            cout << "Toate pachetele au fost livrate! Oprire simulare." << endl;
            break;
        }

    } // SFARSIT LOOP SIMULARE

    // ==========================================
    // 6. CALCUL FINAL SI CLEANUP
    // ==========================================

    for (auto* p : pacheteActive) {
        if (p->status != PachetStatus::LIVRAT) {
            totalPenalties += 200;
        }
        delete p;
    }

    for (auto* agent : flota) {
        if (agent->isDead()) totalPenalties += 500;
        delete agent;
    }

    profitTotal = totalRewards - totalOpCost - totalPenalties;

    cout << "\n=== RAPORT FINAL ===\n";
    cout << "Pachete Livrate: " << pacheteLivrateCounter << "/" << pacheteActive.size() << endl;
    cout << "Total Rewards: " << totalRewards << endl;
    cout << "Total Cost Operare: " << totalOpCost << endl;
    cout << "Total Penalizari: " << totalPenalties << endl;
    cout << "----------------------------\n";
    cout << "PROFIT NET: " << profitTotal << " credite\n";

    return 0;
}