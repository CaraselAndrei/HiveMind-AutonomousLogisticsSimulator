#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <algorithm>
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
    int x;
    cin >> x; // Decomenteaza pentru input manual

    if (x == 1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test"); // Asigura-te ca fisierul exista
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generatorHarta->getDestinations();
        statii = generatorHarta->getStations();
        base = generatorHarta->getBase();
        cout << "Harta citita din fisier:" << endl;
        printMap(myMap);
        delete generatorHarta;
    }
    else {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generator.getDestinations();
        statii = generator.getStations();
        base = generator.getBase();
        cout << "Harta generata procedural:" << endl;
        printMap(myMap);
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
    vector<Pachet*> pacheteInAsteptare; // Coada de dispatch (Vector pt sortare)

    // Variabile pentru Scoring
    long long profitTotal = 0;
    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrateCounter = 0;

    // ==========================================
    // 5. BUCLA PRINCIPALA (SIMULATION LOOP)
    // ==========================================
    for (int tick = 0; tick < settings.maxTicks && pacheteLivrateCounter<settings.totalPackages; tick++) {

        // --- A. GENERARE PACHETE (Spawn) ---
        if (tick % settings.spawnFrequency == 0 && pacheteActive.size() < (size_t)settings.totalPackages) {
            int randIdx = rand() % destinatii.size();

            Pachet* pNou = new Pachet(pacheteActive.size(), destinatii[randIdx], tick);
            pacheteActive.push_back(pNou);
            pacheteInAsteptare.push_back(pNou);

            cout << "[TICK " << tick << "] Pachet NOU #" << pNou->id << " (Valoare: " << pNou->valoare<<". Deadline: "<<pNou->deadline << ")" << endl;

            // SORTARE: Cele mai urgente primele (Urgency Sort Strategy)
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

                    // Cautam pachet potrivit in lista sortata
                    // --- INCEPUT COD NOU STRATEGIE 3 (Cost-Efficiency) ---

        int bestPachetIdx = -1;

        // Iteram prin toate pachetele disponibile pentru a gasi "Perechea Perfecta"
        for (int i = 0; i < pacheteInAsteptare.size(); ++i) {
            Pachet* p = pacheteInAsteptare[i];

            // 1. Verificam daca pachetul a expirat (Curatenie)
            if (p->isExpired(tick)) {
                p->status = PachetStatus::EXPIRAT;
                totalPenalties += 50;
                pacheteInAsteptare.erase(pacheteInAsteptare.begin() + i);
                i--;
                continue;
            }

            // 2. Calculam distanta si timpul ramas
            // Folosim functia manhattan din harta.h (asigura-te ca e accesibila)
            int dist = abs(agent->pozitie.x - p->destinatie.x) + abs(agent->pozitie.y - p->destinatie.y);
            int timpRamas = (p->spawnTime + p->deadline) - tick;

            // 3. Reguli de Dispatch pe Tip de Agent
            bool isMatch = false;

            if (agent->nume == "Drona") {
                // DRONA (Scumpa & Rapida): O folosim DOAR pentru urgente mari sau distante lungi
                // Ex: Daca mai sunt sub 15 tick-uri SAU distanta e mare (> 15)
                if (timpRamas < 15 || dist > 15) {
                    isMatch = true;
                }
            }
            else if (agent->nume == "Robot") {
                // ROBOT (Ieftin & Lent): Il folosim pentru pachete NE-urgente si relativ aproape
                // Ex: Daca avem timp destul (> 20 tick-uri)
                if (timpRamas > 20) {
                    isMatch = true;
                }
            }
            else {
                // SCUTER (Mediu): Ia orice altceva (pachete medii)
                // Sau ia pachetele refuzate de drone/roboti daca nu e nimic altceva
                isMatch = true;
            }

    // 4. Verificare Fezabilitate (Drum + Baterie)
    if (isMatch) {
        vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());
        int energieNecesara = (drum.size() * 2) * agent->consumBaterie;

        // Daca drumul exista si avem baterie (Safety First inclus)
        if (!drum.empty() && agent->baterie > (energieNecesara * 1.1)) {
            // Am gasit pachetul ideal!
            bestPachetIdx = i;

            // Il alocam efectiv
            agent->setPath(drum, p);
            p->status = PachetStatus::ALOCAT;

            cout << " -> [SMART DISPATCH] " << agent->nume << " (" << agent->id << ") a luat pachetul "
                 << p->id << " (Dist: " << dist << ", Timp: " << timpRamas << ")" << endl;

            break; // Iesim din bucla de pachete, am gasit unul
        }
    }
}

// 5. Stergere din lista (doar daca am alocat)
            if (bestPachetIdx != -1) {
                pacheteInAsteptare.erase(pacheteInAsteptare.begin() + bestPachetIdx);
            }


                }
            }
        }

        // --- C. UPDATE AGENTI SI LOGICA DE MISCARE ---
        for (auto* agent : flota) {

            if (agent->stare == AgentState::MOVING) totalOpCost += agent->getCost();

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

                // Retur la baza
                vector<Point> drumRetur = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                agent->setPath(drumRetur);
            }
            // SCENARIUL 2: AJUNS LA BAZA (INSTANT DISPATCH)
            else if (aTerminatDrumul && !agent->hasPackage()) {

                if (agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                    if (!pacheteInAsteptare.empty()) {
                        // Re-sortam pentru siguranta (desi e deja sortat la spawn)
                         sort(pacheteInAsteptare.begin(), pacheteInAsteptare.end(), [](Pachet* a, Pachet* b) {
                            return (a->spawnTime + a->deadline) < (b->spawnTime + b->deadline);
                        });

                        int indexDeSters = -1;
                        for (int i = 0; i < pacheteInAsteptare.size(); ++i) {
                            Pachet* p = pacheteInAsteptare[i];

                            if (p->isExpired(tick)) {
                                p->status = PachetStatus::EXPIRAT;
                                totalPenalties += 50;
                                pacheteInAsteptare.erase(pacheteInAsteptare.begin() + i);
                                i--;
                                continue;
                            }

                            vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());

                            // Safety First Check
                            int distanta = drum.size();
                            int energieNecesara = (distanta * 2) * agent->consumBaterie;

                            if (!drum.empty() && agent->baterie > (energieNecesara * 1.1)) {
                                agent->setPath(drum, p);
                                p->status = PachetStatus::ALOCAT;
                                indexDeSters = i;
                                cout << " -> [INSTANT C] Agent " << agent->nume << " (" << agent->id << ") a preluat URGENT pachetul " << p->id <<" la momentul"<<tick<< endl;
                                break;
                            }
                        }

                        if (indexDeSters != -1) {
                            pacheteInAsteptare.erase(pacheteInAsteptare.begin() + indexDeSters);
                        } else {
                            // Daca nu poate lua niciun pachet (ex: baterie slaba), incarca
                            if(agent->baterie < agent->baterieMax) agent->stare = AgentState::CHARGING;
                        }
                    } else {
                         if(agent->baterie < agent->baterieMax) agent->stare = AgentState::CHARGING;
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

    }

    // ==========================================
    // 6. CALCUL FINAL SI CLEANUP
    // ==========================================

    // Penalizari pentru pachete nelivrate ramase
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