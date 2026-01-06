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
    vector<Point> destinatii;
    vector<Point> statii;
    Point base;

    cout<<"Selecteaza optiunea:\n";
    cout<<"1.Citeste din fisier.\n";
    cout<<"2.Genereaza harta.\n";
    cout<<"0.Iesi.\n";
    int x=2;
    //cin>>x;

    MapGrid myMap;
    if(x==1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta2");
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generatorHarta->getDestinations();
        statii=generatorHarta->getStations();
        cout << "Harta citita:" << endl;
        printMap(myMap);
        base = generatorHarta->getBase();
        destinatii=generatorHarta->getDestinations();
        statii=generatorHarta->getStations();
    }
    else if(x==2) {
        myMap = generator.generateMap(settings.mapRows, settings.mapCols, settings.maxStations,settings.clientsCount);
        destinatii = generator.getDestinations();
        base = generator.getBase();
        printMap(myMap);
    }

    int deliveries=0;
    ProceduralMapGenerator tools;
    cout << "\nLista destinatii (Total: " << destinatii.size() << "):" << endl;
    for(size_t i = 0; i < destinatii.size(); i++) {
        cout << "Destinatia " << i + 1 << ": [" << destinatii[i].x << ", " << destinatii[i].y << "]" << endl;
    }

    //Simulare thick
    vector<Agent*> flota;
    for(int i=0; i<settings.dronesCount; ++i) flota.push_back(new Drona(base));
    for(int i=0; i<settings.robotsCount; ++i) flota.push_back(new Robot(base));
    for(int i=0; i<settings.scootersCount; ++i) flota.push_back(new Scuter(base));

    vector<Pachet> pacheteActive;
    pacheteActive.reserve(settings.totalPackages);

    // Variabile pentru Scoring
    long long profitTotal = 0;
    long long totalRewards = 0;
    long long totalOpCost = 0;
    long long totalPenalties = 0;
    int pacheteLivrate = 0;

    // 3. Bucla Principala (Simulation Loop)
    for (int tick = 0; tick < settings.maxTicks; tick++) {

        // A. Generare Pachete (Comenzi Dinamice)
        // -> Sunt generate dinamic la intervale regulate
        if (tick % settings.spawnFrequency == 0 && pacheteActive.size() < settings.totalPackages) {
            // Alegem random un client destinatie
            int randIdx = rand() % destinatii.size();
            Pachet pNou(pacheteActive.size(), destinatii[randIdx], tick);
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
                                break;
                            }
                        } else {
                            // Daca e idle dar nu e la baza, il trimitem la baza
                            vector<Point> drumIncarcare = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                            for (auto  statie: statii) {
                                vector <Point> drum=findPath(agent->pozitie, statie, myMap, agent->poateZbura());
                                if (drumIncarcare.size() > drum.size()) {
                                    drumIncarcare=drum;
                                }
                            }
                            if(!drumIncarcare.empty()) agent->setPath(drumIncarcare);
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
        if (tick % 10 == 0) {
            cout << "[TICK " << tick << "] Rewards: " << totalRewards
                 << " Cost: " << totalOpCost << " Penalties: " << totalPenalties << endl;
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