#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h> 
#include <stdio.h> 
#include <string.h>
 
#define MAX_VEHICLES 100
#define MAX_LINE_LENGTH 20
#define MAIN_FONT "/usr/share/fonts/TTF/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SCALE 1
#define ROAD_WIDTH 150
#define LANE_WIDTH 50
#define ARROW_SIZE 15


const char* VEHICLE_FILE = "vehicles.data";

typedef struct{
    int currentLight;
    int nextLight;
} SharedData;

//Adds vehicles and queue structures
typedef struct {
    char vehicleNumber[10];
    char road[2];   // A, B, C, D
} Vehicle;

typedef struct {
    Vehicle data[MAX_VEHICLES];
    int front, rear;
} VehicleQueue;

void initQueue(VehicleQueue* q) { 
    q->front = q->rear = -1; }
bool isEmpty(VehicleQueue* q) { 
    return q->front == -1; }
bool isFull(VehicleQueue* q) { 
    return (q->rear + 1) % MAX_VEHICLES == q->front; }

void enqueue(VehicleQueue* q, Vehicle v) {
    if (isFull(q)) return;
    if (isEmpty(q)) q->front = 0;
    q->rear = (q->rear + 1) % MAX_VEHICLES;
    q->data[q->rear] = v;
}

Vehicle dequeue(VehicleQueue* q) {
    Vehicle v = q->data[q->front];
    if (q->front == q->rear) q->front = q->rear = -1;
    else q->front = (q->front + 1) % MAX_VEHICLES;
    return v;
}

//Global queues for each road
VehicleQueue queueA, queueB, queueC, queueD;

int main()
{
    initQueue(&queueA);
    initQueue(&queueB);
    initQueue(&queueC);
    initQueue(&queueD);
}
