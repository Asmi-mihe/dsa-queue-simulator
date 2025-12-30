#ifndef simulator_h
#define simulator_h

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <SDL2/SDL_ttf.h> 

// Max vehicles constant
#define MAX_VEHICLES 100
#define VEHICLE_NUMBER_SIZE 10

#define LIGHT_A 0
#define LIGHT_B 1
#define LIGHT_C 2
#define LIGHT_D 3
#define LIGHT_ALL_RED 4

#define VEHICLE_PASS_TIME 500      // ms per vehicle
#define PRIORITY_PASS_TIME 300     // ms per vehicle

// Vehicle structure
typedef struct {
    char vehicleNumber[VEHICLE_NUMBER_SIZE];
    char road[2];   // A, B, C, D
    int lane; // lane number
    int priority; // optional, for priority vehicle
} Vehicle;

// Queue structure
typedef struct {
    Vehicle data[MAX_VEHICLES];
    int front, rear;
} VehicleQueue;

typedef struct {
    int currentLight;
    int nextLight;
    int lastServedCount;   // number of vehicles served in last cycle
    int lastGreenDuration; // computed green time in ms
    int remainingTime;     // time left for current green in ms
} SharedData;

// Vehicle sprite structure
typedef struct {
    float x, y;
    float dx, dy;
    bool active;
    SDL_Color color;
    int width, height;
} VehicleSprite;

// Shared queues (declare as extern so other files can use them)
extern VehicleQueue queueA;
extern VehicleQueue queueB;
extern VehicleQueue queueC;
extern VehicleQueue queueD;

// Shared mutexes
extern SDL_mutex* mutexA;
extern SDL_mutex* mutexB;
extern SDL_mutex* mutexC;
extern SDL_mutex* mutexD;

// Queue functions
void initQueue(VehicleQueue* q);
bool isEmpty(VehicleQueue* q);
bool isFull(VehicleQueue* q);
int enqueue(VehicleQueue* q, Vehicle v);
int dequeue(VehicleQueue* q, Vehicle* v);
int queueSize(VehicleQueue* q);


// Function prototypes
void spawnVehicleSprite(char road, SharedData* sharedData, bool canMove);
void updateSprites(SharedData* sharedData);
void drawSprites(SDL_Renderer *renderer);
void drawRoads(SDL_Renderer *renderer, TTF_Font *font);
void drawHUD(SDL_Renderer *renderer, TTF_Font *font, SharedData *sharedData);
void refreshLights(SDL_Renderer *renderer, TTF_Font *font, SharedData *sharedData);
void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y);

// Thread functions
int chequeQueue(void* arg);
int readAndParseFiles(void* arg);

#endif