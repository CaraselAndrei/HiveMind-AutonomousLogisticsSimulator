#include <iostream>
#include <vector>
#include <queue>        // Necesara pentru coada de pachete
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
    // Pentru testare automata poti lasa x hardcodat sau citi de la tastatura
    int x = 2;
    // cin >> x;

    if (x == 1) {
        IMapGenerator* generatorHarta = new FileMapLoader("harta_test");
        myMap = generatorHarta->generateMap(settings.mapRows, settings.mapCols, settings.maxStations, settings.clientsCount);
        destinatii = generatorHarta->getDestinations();
        statii = generatorHarta->getStations();
        base = generatorHarta->getBase();
        cout << "Harta citita din fisier:" << endl;
        printMap(myMap);
        delete generatorHarta; // Curatam memoria generatorului
    }
    else {
        // Generare procedurala
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
    // Folosim pointeri pentru a evita invalidarea referintelor cand vectorul se redimensioneaza
    vector<Pachet*> pacheteActive;      // Evidenta tuturor pachetelor pentru statistici
    queue<Pachet*> pacheteInAsteptare;  // Coada FIFO pentru alocare rapida

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

        // A. GENERARE PACHETE (Spawn)
        if (tick % settings.spawnFrequency == 0 && pacheteActive.size() < (size_t)settings.totalPackages) {
            int randIdx = rand() % destinatii.size();

            // Alocam dinamic noul pachet
            Pachet* pNou = new Pachet(pacheteActive.size(), destinatii[randIdx], tick);

            pacheteActive.push_back(pNou);      // Adaugam in lista totala
            pacheteInAsteptare.push(pNou);      // Adaugam in coada de dispatch

            cout << "[TICK " << tick << "] Pachet NOU #" << pNou->id << " catre ["
                 << pNou->destinatie.x << "," << pNou->destinatie.y << "] (Valoare: " << pNou->valoare << ")" << endl;
        }

        // B. STRATEGIE ALOCARE (Pentru agentii care stau DEJA degeaba in baza)
        // Aceasta etapa e pentru agentii care erau deja IDLE la inceputul tick-ului
        if (!pacheteInAsteptare.empty()) {
            for (auto* agent : flota) {
                if (pacheteInAsteptare.empty()) break;

                // Verificam daca agentul e liber, viu si se afla la baza
                if (agent->stare == AgentState::IDLE && !agent->isDead() &&
                    agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                    Pachet* p = pacheteInAsteptare.front();

                    // Calculam ruta
                    vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());

                    if (!drum.empty()) {
                        agent->setPath(drum, p);
                        p->status = PachetStatus::ALOCAT;
                        pacheteInAsteptare.pop(); // Scoatem din coada
                        cout << " -> [DISPATCH] Agent " << agent->nume <<" "<<agent->id<< " a preluat pachetul " << p->id << endl;
                    }
                }
            }
        }

        // C. UPDATE AGENTI SI LOGICA DE MISCARE
        for (auto* agent : flota) {

            // Calcul cost operational
            if (!agent->isDead()) {
                totalOpCost += agent->getCost();
            }

            // Executam miscarea
            bool aTerminatDrumul = agent->update();

            // --- SCENARIUL 1: Agentul a ajuns la destinatie cu un pachet (LIVRARE) ---
            if (aTerminatDrumul && agent->hasPackage()) {
                Pachet* p = agent->pachetCurent;

                if (p->isExpired(tick)) {
                    totalPenalties += 50; // Penalizare intarziere
                    cout << " -> [LIVRARE] Pachet " << p->id << " livrat TARZIU.\n";
                } else {
                    totalRewards += p->valoare;
                    cout << " -> [LIVRARE] Pachet " << p->id << " livrat SUCCES! (+" << p->valoare << ")\n";
                }

                p->status = PachetStatus::LIVRAT;
                agent->pachetCurent = nullptr; // Eliberam agentul
                pacheteLivrateCounter++;

                // Trimitem agentul inapoi la baza
                vector<Point> drumRetur = findPath(agent->pozitie, base, myMap, agent->poateZbura());
                agent->setPath(drumRetur);
            }

            // --- SCENARIUL 2: Agentul a ajuns la BAZA fara pachet (GATA DE MUNCA) ---
            // Aici implementam alocarea INSTANTANEE
            else if (aTerminatDrumul && !agent->hasPackage()) {

                // Verificam daca locatia curenta este baza
                if (agent->pozitie.x == base.x && agent->pozitie.y == base.y) {

                    // Incercam sa luam un pachet din coada imediat
                    bool aPrimitPachet = false;

                    while (!pacheteInAsteptare.empty()) {
                        Pachet* p = pacheteInAsteptare.front();

                        // Verificam daca pachetul a expirat cat a stat in coada (optional)
                        if (p->isExpired(tick)) {
                             cout << " -> [EXPIRAT] Pachetul " << p->id << " a expirat in coada.\n";
                             p->status = PachetStatus::EXPIRAT;
                             pacheteInAsteptare.pop();
                             totalPenalties += 10; // Mica penalizare
                             continue; // Trecem la urmatorul
                        }

                        // Calculam ruta pentru pachetul valid
                        vector<Point> drum = findPath(agent->pozitie, p->destinatie, myMap, agent->poateZbura());

                        if (!drum.empty()) {
                            agent->setPath(drum, p);
                            p->status = PachetStatus::ALOCAT;
                            pacheteInAsteptare.pop();
                            cout << " -> [INSTANT] Agent " << agent->nume <<" "<<agent->id<<" a ajuns la baza si a preluat IMEDIAT pachetul " << p->id << endl;
                            aPrimitPachet = true;
                            break; // Am gasit pachet, iesim din while
                        } else {
                            // Pachetul nu poate fi livrat de acest agent (ex: inaccesibil), il lasam in coada sau il mutam la coada?
                            // Pentru simplitate, presupunem ca il poate lua altcineva, dar aici il sarim momentan
                            break;
                        }
                    }

                    // Daca nu a primit pachet si are bateria scazuta, poate intra la incarcat
                    if (!aPrimitPachet && agent->baterie < agent->baterieMax) {
                         agent->stare = AgentState::CHARGING;
                    }
                }
            }
        }

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

    // Penalizari pentru pachete nelivrate
    for (auto* p : pacheteActive) {
        if (p->status != PachetStatus::LIVRAT) {
            totalPenalties += 200; // Penalizare mare pentru pachet pierdut
        }
        delete p; // Stergem memoria alocata pentru pachet
    }

    // Penalizari pentru agenti morti si stergere memorie
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