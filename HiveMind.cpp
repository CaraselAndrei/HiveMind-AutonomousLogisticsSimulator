#include <algorithm>
#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include "harta.h"
#include "settings.h"
#include "agenti.h"
#include "pachete.h"

using namespace std;

int main()
{
    ProceduralMapGenerator generator;
    SimulationSettings settings = ConfigLoader::load("simulation_setup.txt");
    vector<Point> destinatiiFinale;
    Point base;

    cout<<"Selecteaza optiunea:\n";
    cout<<"1.Citeste din fisier.\n";
    cout<<"2.Genereaza harta.\n";
    cout<<"0.Iesi.\n";
    int x;
    cin>>x;

    MapGrid myMap;
    if(x==1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test");
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatiiFinale = generatorHarta->getDestinations();
        cout << "Harta citita:" << endl;
        printMap(myMap);
        base= generator.getBase();
    }
    else if(x==2) {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations,settings.clientsCount);
        destinatiiFinale = generator.getDestinations();
        printMap(myMap);
    }

    int deliveries=0;
    ProceduralMapGenerator tools;
    cout << "\nLista destinatii (Total: " << destinatiiFinale.size() << "):" << endl;
    for(size_t i = 0; i < destinatiiFinale.size(); i++) {
        cout << "Destinatia " << i + 1 << ": [" << destinatiiFinale[i].x << ", " << destinatiiFinale[i].y << "]" << endl;
    }

    //Simulare thick
vector<Agent*> flota;
    for(int i=0; i<settings.dronesCount; ++i) flota.push_back(new Drona(base));
    for(int i=0; i<settings.robotsCount; ++i) flota.push_back(new Robot(base));
    for(int i=0; i<settings.scootersCount; ++i) flota.push_back(new Scuter(base));

    vector<Pachet> pacheteActive;

    // Variabile pentru Scoring
    long long profitTotal = 0;
    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrate = 0;

    cout << "\n=== INCEPE SIMULAREA HIVEMIND ===\n";

    // 3. Bucla Principala (Simulation Loop)
    for (int tick = 0; tick < settings.maxTicks; tick++) {

        // A. Generare Pachete (Comenzi Dinamice)
        // -> Sunt generate dinamic la intervale regulate
        if (tick % settings.spawnFrequency == 0 && pacheteActive.size() < settings.totalPackages) {
            // Alegem random un client destinatie
            int randIdx = rand() % destinatiiFinale.size();
            Pachet pNou(pacheteActive.size(), destinatiiFinale[randIdx], tick);
            pacheteActive.push_back(pNou);
            cout << "[TICK " << tick << "] Pachet NOU #" << pNou.id << " catre ["
                 << pNou.destinatie.x << "," << pNou.destinatie.y << "] Valoare: " << pNou.valoare << endl;
        }

        // B. HiveMind Strategy: Dispatching & Routing

        for (auto& p : pacheteActive) {
            if (p.status == PachetStatus::NEALOCAT) {
                // Cautam un agent IDLE
                for (auto* agent : flota) {
                    if (agent->stare == AgentState::IDLE && !agent->isDead()) {
                        // Verificam daca e la baza (pachetele se iau de la baza)
                        if (agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                            // Calculam ruta
                            vector<Point> drum = findPath(agent->pozitie, p.destinatie, myMap, agent->poateZbura());

                            if (!drum.empty()) {
                                agent->setPath(drum, &p);
                                p.status = PachetStatus::ALOCAT;
                                cout << " -> Alocat pachetul " << p.id << " agentului " << agent->nume << endl;
                                break; // Trecem la urmatorul pachet
                            }
                        } else {
                            // Daca e idle dar nu e la baza, il trimitem la baza
                             vector<Point> drumBaza = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                             if(!drumBaza.empty()) agent->setPath(drumBaza);
                        }
                    }
                }
            }
        }

        // C. Update Agenti & Costuri
        for (auto* agent : flota) {
            // Cost Operare: Suma costurilor pe tick
            if (!agent->isDead()) {
                totalOpCost += agent->getCost();
            }

            bool ajunsLaDestinatie = agent->update();

            // Verificam Penalizari - Baterie Moarta
            if (agent->stare == AgentState::DEAD) {
                 // Penalizare aplicata o singura data sau pe tick? De obicei o data cand moare.
                 // Aici simplificam si adaugam la final sau verificam tranzitia de stare.
            }

            // D. Verificare Livrare
            if (ajunsLaDestinatie && agent->hasPackage()) {
                Pachet* p = agent->pachetCurent;

                // Verificare deadline
                if (p->isExpired(tick)) {
                    totalPenalties += 50; // Penalizare intarziere
                    cout << " -> [INTARZIAT] Pachet " << p->id << " livrat tarziu.\n";
                } else {
                    totalRewards += p->valoare;
                    cout << " -> [SUCCESS] Pachet " << p->id << " livrat! + " << p->valoare << "\n";
                }

                p->status = PachetStatus::LIVRAT;
                agent->pachetCurent = nullptr; // Eliberam agentul
                pacheteLivrate++;

                // Trimitem agentul inapoi la baza sau la incarcat
                vector<Point> drumRetur = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                agent->setPath(drumRetur);
            }
        }
    }

    // 4. Calcul Final Profit
    // Penalizari pentru pachete nelivrate si agenti morti
    for(const auto& p : pacheteActive) {
        if(p.status != PachetStatus::LIVRAT) {
            totalPenalties += 200; // Pachet nelivrat
        }
    }
    for(auto* agent : flota) {
        if(agent->isDead()) totalPenalties += 500;
        delete agent; // Cleanup
    }

    profitTotal = totalRewards - totalOpCost - totalPenalties;

    cout << "\n=== RAPORT FINAL ===\n";
    cout << "Pachete Livrate: " << pacheteLivrate << "/" << settings.totalPackages << endl;
    cout << "Total Rewards: " << totalRewards << endl;
    cout << "Total Cost Operare: " << totalOpCost << endl;
    cout << "Total Penalizari: " << totalPenalties << endl;
    cout << "----------------------------\n";
    cout << "PROFIT NET: " << profitTotal << " credite\n";
    return 0;
}