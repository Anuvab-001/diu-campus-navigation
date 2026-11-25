#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define MAX_LOCATIONS 40
#define NAME_LEN 80
#define INF (INT_MAX / 4)
#define WALKING_SPEED_M_PER_MIN 80.0

typedef struct Edge {
    int to;
    int distance;
    struct Edge *next;
} Edge;

typedef struct {
    char name[NAME_LEN];
    Edge *head;
} Location;

typedef struct {
    int vertex;
    int distance;
} HeapNode;

typedef struct {
    HeapNode *data;
    int size;
    int capacity;
} MinHeap;

static Location locations[MAX_LOCATIONS];
static int locationCount = 0;

static int equalsIgnoreCase(const char *a, const char *b) {
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a;
        unsigned char cb = (unsigned char)*b;
        if (ca>='A'&&ca<='Z') ca= (unsigned char)(ca+('a'-'A'));
        if (cb>='A'&&cb<='Z') cb= (unsigned char)(cb + ('a' - 'A'));
        if (ca!= cb) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int findLocation(const char *name) {
    for (int i = 0; i < locationCount; i++) {
        if (equalsIgnoreCase(locations[i].name, name)) return i;
    }
    return -1;
}

static int addLocation(const char *name) {
    int existing = findLocation(name);
    if (existing != -1) return existing;
    if (locationCount >= MAX_LOCATIONS) {
        fprintf(stderr, "Too many locations.\n");
        exit(EXIT_FAILURE);
    }
    snprintf(locations[locationCount].name, NAME_LEN, "%s", name);
    locations[locationCount].head = NULL;
    return locationCount++;
}

static void addOneWayEdge(int u, int v, int distance) {
    Edge *e = malloc(sizeof(*e));
    if (!e) { perror("malloc"); exit(EXIT_FAILURE); }
    e->to = v;
    e->distance = distance;
    e->next = locations[u].head;
    locations[u].head = e;
}

static void addRoad(const char *from, const char *to, int distance) {
    int u = addLocation(from);
    int v = addLocation(to);
    addOneWayEdge(u, v, distance);
    addOneWayEdge(v, u, distance);
}

static void initHeap(MinHeap *h, int capacity) {
    h->data = malloc((size_t)capacity * sizeof(*h->data));
    if (!h->data) { perror("malloc"); exit(EXIT_FAILURE); }
    h->size = 0;
    h->capacity = capacity;
}

static void heapSwap(HeapNode *a, HeapNode *b) {
    HeapNode t = *a; *a = *b; *b = t;
}

static void heapPush(MinHeap *h, int vertex, int distance) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        HeapNode *newData = realloc(h->data, (size_t)h->capacity * sizeof(*h->data));
        if (!newData) {
            perror("realloc");
            free(h->data);
            exit(EXIT_FAILURE);
        }
        h->data = newData;
    }
    int i = h->size++;
    h->data[i] = (HeapNode){vertex, distance};
    while (i > 0) {
        int p = (i - 1) / 2;
        if (h->data[p].distance <= h->data[i].distance) break;
        heapSwap(&h->data[p], &h->data[i]);
        i = p;
    }
}

static HeapNode heapPop(MinHeap *h) {
    HeapNode result = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = left + 1;
        int smallest = i;
        if (left < h->size && h->data[left].distance < h->data[smallest].distance) smallest = left;
        if (right < h->size && h->data[right].distance < h->data[smallest].distance) smallest = right;
        if (smallest == i) break;
        heapSwap(&h->data[i], &h->data[smallest]);
        i = smallest;
    }
    return result;
}

static void freeHeap(MinHeap *h) { free(h->data); }

static int parseChoice(const char *text, int *out) {
    char extra;
    return sscanf(text, " %d %c", out, &extra) == 1;
}

static int chooseLocationByNumber(const char *prompt) {
    char input[64];
    while (1) {
        printf("\n%s\n", prompt);
        for (int i = 0; i < locationCount; ++i) {
            printf("%2d. %s\n", i + 1, locations[i].name);
        }
        printf("Enter location number: ");
        if (!fgets(input, sizeof(input), stdin)) return -1;
        char extra;
        int number;
        if (sscanf(input, " %d %c", &number, &extra) == 1 &&
            number >= 1 && number <= locationCount) return number - 1;
        printf("Invalid location number. Please choose 1-%d.\n", locationCount);
    }
}

static int walkingMinutes(int meters) {
    if (meters <= 0) return 0;
    int minutes = (int)(meters / WALKING_SPEED_M_PER_MIN + 0.5);
    return minutes < 1 ? 1 : minutes;
}

static void printLocations(void) {
    printf("\n========== DIU CAMPUS LOCATIONS ==========\n");
    for (int i = 0; i < locationCount; ++i) printf("%2d. %s\n", i + 1, locations[i].name);
}

static void printGraph(void) {
    printf("\n========== DIRECT WALKING CONNECTIONS ==========\n");
    printf("Distances are estimated walking-path distances.\n");
    for (int u = 0; u < locationCount; ++u) {
        printf("\n%s:\n", locations[u].name);
        for (Edge *e = locations[u].head; e; e = e->next) {
            printf("  -> %-32s %3d m (~%d min)\n", locations[e->to].name, e->distance, walkingMinutes(e->distance));
        }
    }
}

static void dijkstra(int source, int target) {
    int dist[MAX_LOCATIONS];
    int parent[MAX_LOCATIONS];
    int visited[MAX_LOCATIONS] = {0};

    for (int i = 0; i < locationCount; ++i) {
        dist[i] = INF;
        parent[i] = -1;
    }
    dist[source] = 0;

    MinHeap heap;
    initHeap(&heap, locationCount > 4 ? locationCount : 4);
    heapPush(&heap, source, 0);

    while (heap.size > 0) {
        HeapNode current = heapPop(&heap);
        int u = current.vertex;
        if (visited[u]) continue;
        if (current.distance != dist[u]) continue;
        visited[u] = 1;
        if (u == target) break;

        for (Edge *e = locations[u].head; e; e = e->next) {
            int v = e->to;
            if (dist[u] + e->distance < dist[v]) {
                dist[v] = dist[u] + e->distance;
                parent[v] = u;
                heapPush(&heap, v, dist[v]);
            }
        }
    }

    if (dist[target] == INF) {
        printf("\nNo walking route exists between the selected locations.\n");
        freeHeap(&heap);
        return;
    }

    int path[MAX_LOCATIONS];
    int pathLen = 0;
    for (int v = target; v != -1; v = parent[v]) path[pathLen++] = v;

    printf("\n========== SHORTEST WALKING ROUTE ==========\n");
    for (int i = pathLen - 1; i >= 0; --i) {
        printf("%s", locations[path[i]].name);
        if (i > 0) printf(" -> ");
    }
    printf("\nTotal distance : %d m\n", dist[target]);
    printf("Estimated time : %d min (at about %.0f m/min)\n", walkingMinutes(dist[target]), WALKING_SPEED_M_PER_MIN);
    freeHeap(&heap);
}

static void searchLocation(void) {
    char input[NAME_LEN];
    printf("\nEnter location name: ");
    if (!fgets(input, sizeof(input), stdin)) return;
    input[strcspn(input, "\n")] = '\0';
    int id = findLocation(input);
    if (id == -1) {
        printf("Location not found.\n");
        return;
    }
    printf("Found: %s\n", locations[id].name);
    printf("Direct walking connections:\n");
    for (Edge *e = locations[id].head; e; e = e->next) {
        printf("  %s -> %s : %d m (~%d min)\n", locations[id].name, locations[e->to].name, e->distance, walkingMinutes(e->distance));
    }
}

static void buildCampusGraph(void) {

    addLocation("Main Entrance");
    addLocation("AB4 (Knowledge Tower)");
    addLocation("Library (AB4 - 4th Floor)");
    addLocation("CSE Department (AB4 - 2nd Floor)");
    addLocation("Playground");
    addLocation("Swimming Pool");
    addLocation("Department of Innovation / NFE");
    addLocation("Transport");
    addLocation("AB1 (Faculty of Graduate Studies)");
    addLocation("Green Garden");
    addLocation("Admission Office");
    addLocation("Sadhinota Shommelon");
    addLocation("Food Court");
    addLocation("Health Centre");
    addLocation("Bonomaiya");
    addLocation("Lake");
    addLocation("EEE / Civil Department");
    addLocation("Inspiration Building");
    addLocation("Engineering Campus");
    addLocation("YKSG-2 Male Hall");
    addLocation("Bike Parking");
    addLocation("RASG Female Hall");
    addLocation("Falcon");
    addLocation("DIU Central Mosque");


    addRoad("AB4 (Knowledge Tower)", "Playground", 5);
    addRoad("AB4 (Knowledge Tower)", "Swimming Pool", 40);
    addRoad("AB4 (Knowledge Tower)", "Department of Innovation / NFE", 90);
    addRoad("AB4 (Knowledge Tower)", "Transport", 110);
    addRoad("AB4 (Knowledge Tower)", "AB1 (Faculty of Graduate Studies)", 130);
    addRoad("AB4 (Knowledge Tower)", "Green Garden", 135);
    addRoad("AB4 (Knowledge Tower)", "Admission Office", 140);
    addRoad("AB4 (Knowledge Tower)", "Sadhinota Shommelon", 100);
    addRoad("AB4 (Knowledge Tower)", "Food Court", 125);
    addRoad("AB4 (Knowledge Tower)", "Health Centre", 130);
    addRoad("AB4 (Knowledge Tower)", "Bonomaiya", 120);
    addRoad("AB4 (Knowledge Tower)", "Lake", 180);
    addRoad("AB4 (Knowledge Tower)", "EEE / Civil Department", 350);


    addRoad("AB4 (Knowledge Tower)", "Library (AB4 - 4th Floor)", 0);
    addRoad("AB4 (Knowledge Tower)", "CSE Department (AB4 - 2nd Floor)", 0);
    addRoad("Library (AB4 - 4th Floor)", "CSE Department (AB4 - 2nd Floor)", 0);


    addRoad("Main Entrance", "Playground", 45);
    addRoad("Main Entrance", "Admission Office", 110);
    addRoad("Main Entrance", "AB4 (Knowledge Tower)", 145);
    addRoad("Playground", "Swimming Pool", 35);
    addRoad("Playground", "Sadhinota Shommelon", 100);
    addRoad("Playground", "Food Court", 120);


    addRoad("Playground", "DIU Central Mosque", 45);


    addRoad("Department of Innovation / NFE", "Sadhinota Shommelon", 35);
    addRoad("Transport", "Bonomaiya", 25);
    addRoad("Bonomaiya", "Lake", 35);
    addRoad("Bonomaiya", "Food Court", 35);
    addRoad("Food Court", "Health Centre", 30);
    addRoad("Health Centre", "Admission Office", 35);
    addRoad("Health Centre", "Green Garden", 35);
    addRoad("Green Garden", "Admission Office", 25);
    addRoad("Admission Office", "AB1 (Faculty of Graduate Studies)", 35);
    addRoad("AB1 (Faculty of Graduate Studies)", "EEE / Civil Department", 240);
    addRoad("Lake", "EEE / Civil Department", 190);
    addRoad("Lake", "Sadhinota Shommelon", 55);
    addRoad("Sadhinota Shommelon", "Bike Parking", 35);
    addRoad("Sadhinota Shommelon", "RASG Female Hall", 65);
    addRoad("RASG Female Hall", "Bonomaiya", 55);


    addRoad("Department of Innovation / NFE", "Transport", 25);
    addRoad("DIU Central Mosque", "Transport", 25);
    addRoad("DIU Central Mosque", "Green Garden", 20);

    addRoad("Green Garden", "AB1 (Faculty of Graduate Studies)", 30);

    addRoad("AB4 (Knowledge Tower)", "Inspiration Building", 180);
    addRoad("Inspiration Building", "Engineering Campus", 120);
    addRoad("Engineering Campus", "YKSG-2 Male Hall", 90);
    addRoad("Engineering Campus", "Department of Innovation / NFE", 170);
    addRoad("Department of Innovation / NFE", "Inspiration Building", 80);
    addRoad("Food Court", "DIU Central Mosque", 150);
    addRoad("Falcon", "Inspiration Building", 170);
}

static void freeGraph(void) {
    for (int i = 0; i < locationCount; ++i) {
        Edge *e = locations[i].head;
        while (e) {
            Edge *next = e->next;
            free(e);
            e = next;
        }
        locations[i].head = NULL;
    }
}

static void runMenu(void) {
    char input[128];
    while (1) {
        printf("\n===============================================\n");
        printf("          DIU CAMPUS NAVIGATION SYSTEM\n");
        printf("===============================================\n");
        printf("1. Display all locations\n");
        printf("2. Display direct walking connections\n");
        printf("3. Find shortest walking route\n");
        printf("4. Search a location\n");
        printf("5. Exit\n");
        printf("Enter choice: ");
        if (!fgets(input, sizeof(input), stdin)) break;
        int choice;
        if (!parseChoice(input, &choice)) {
            printf("Invalid input. Enter a number from 1 to 5.\n");
            continue;
        }
        if (choice == 1) printLocations();
        else if (choice == 2) printGraph();
        else if (choice == 3) {
            int source = chooseLocationByNumber("Select CURRENT location:");
            if (source == -1) break;
            int target = chooseLocationByNumber("Select DESTINATION:");
            if (target == -1) break;
            dijkstra(source, target);
        } else if (choice == 4) searchLocation();
        else if (choice == 5) {
            printf("\nThank you for using DIU Campus Navigation System.\n");
            break;
        } else printf("Invalid choice. Enter 1 to 5.\n");
    }
}

int main(void) {
    buildCampusGraph();
    runMenu();
    freeGraph();
    return 0;
}
