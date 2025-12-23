#define SDL_MAIN_HANDLED
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

// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font);
void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y);
void drawLightForA(SDL_Renderer* renderer, bool isRed);
void drawLightForB(SDL_Renderer* renderer, bool isRed);
void drawLightForC(SDL_Renderer* renderer, bool isRed);
void drawLightForD(SDL_Renderer* renderer, bool isRed);
void refreshLight(SDL_Renderer *renderer, SharedData* sharedData);
void* chequeQueue(void* arg);
void* readAndParseFile(void* arg);

void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void drawArrwow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
    // Sort vertices by ascending Y (bubble sort approach)
    if (y1 > y2) { swap(&y1, &y2); swap(&x1, &x2); }
    if (y1 > y3) { swap(&y1, &y3); swap(&x1, &x3); }
    if (y2 > y3) { swap(&y2, &y3); swap(&x2, &x3); }

    // Compute slopes
    float dx1 = (y2 - y1) ? (float)(x2 - x1) / (y2 - y1) : 0;
    float dx2 = (y3 - y1) ? (float)(x3 - x1) / (y3 - y1) : 0;
    float dx3 = (y3 - y2) ? (float)(x3 - x2) / (y3 - y2) : 0;

    float sx1 = x1, sx2 = x1;

    // Fill first part (top to middle)
    for (int y = y1; y < y2; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx1;
        sx2 += dx2;
    }

    sx1 = x2;

    // Fill second part (middle to bottom)
    for (int y = y2; y <= y3; y++) {
        SDL_RenderDrawLine(renderer, (int)sx1, y, (int)sx2, y);
        sx1 += dx3;
        sx2 += dx2;
    }
}

// Draw light for Road A (top side)
void drawLightForA(SDL_Renderer* renderer, bool isRed){
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {400, 100, 50, 30}; // position near top
    SDL_RenderFillRect(renderer, &lightBox);

    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);

    SDL_Rect straight_Light = {405, 105, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 435,105, 435, 125, 445,115);
}

// Draw light for Road B (bottom side) 
void drawLightForB(SDL_Renderer* renderer, bool isRed){
    // draw light box
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {400, 300, 50, 30};
    SDL_RenderFillRect(renderer, &lightBox);
    // draw light
    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // red
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);    // green
    SDL_Rect straight_Light = {405, 305, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 435,305, 435, 305+20, 435+10, 305+10);
}

// Draw light for Road C (right side)
void drawLightForC(SDL_Renderer* renderer, bool isRed){
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {650, 400, 50, 30}; // position near right
    SDL_RenderFillRect(renderer, &lightBox);

    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);

    SDL_Rect straight_Light = {655, 405, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 685,405, 685,425, 695,415);
}

// Draw light for Road D (left side)
void drawLightForD(SDL_Renderer* renderer, bool isRed){
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_Rect lightBox = {100, 400, 50, 30}; // position near left
    SDL_RenderFillRect(renderer, &lightBox);

    if(isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else SDL_SetRenderDrawColor(renderer, 11, 156, 50, 255);

    SDL_Rect straight_Light = {105, 405, 20, 20};
    SDL_RenderFillRect(renderer, &straight_Light);
    drawArrwow(renderer, 135,405, 135,425, 145,415);
}

void drawRoadsAndLane(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 211,211,211,255);
    // Vertical road
    
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);
    // draw horizontal lanes
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for(int i=0; i<=3; i++){
        // Horizontal lanes
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i // x2, y2
        );
        SDL_RenderDrawLine(renderer, 
            800, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,
            WINDOW_WIDTH/2 + ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i
        );
        // Vertical lanes
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, 0,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 - ROAD_WIDTH/2
        );
        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, 800,
            WINDOW_WIDTH/2 - ROAD_WIDTH/2 + LANE_WIDTH*i, WINDOW_HEIGHT/2 + ROAD_WIDTH/2
        );
    }
    displayText(renderer, font, "A",400, 10);
    displayText(renderer, font, "B",400,770);
    displayText(renderer, font, "D",10,400);
    displayText(renderer, font, "C",770,400);
    
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *textSurface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_FreeSurface(textSurface);
    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_Log("DIM of SDL_Rect %d %d %d %d", textRect.x, textRect.y, textRect.h, textRect.w);
    // SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
    // SDL_Log("TTF_Error: %s\n", TTF_GetError());
}

void refreshLight(SDL_Renderer *renderer, SharedData* sharedData){
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    drawRoadsAndLane(renderer, NULL);

    // Example: use currentLight to decide which road is green
    switch(sharedData->nextLight){
        case 0: // all red
            drawLightForA(renderer, true);
            drawLightForB(renderer, true);
            drawLightForC(renderer, true);
            drawLightForD(renderer, true);
            break;
        case 1: // Road A green
            drawLightForA(renderer, false);
            drawLightForB(renderer, true);
            drawLightForC(renderer, true);
            drawLightForD(renderer, true);
            break;
        case 2: // Road B green
            drawLightForA(renderer, true);
            drawLightForB(renderer, false);
            drawLightForC(renderer, true);
            drawLightForD(renderer, true);
            break;
        case 3: // Road C green
            drawLightForA(renderer, true);
            drawLightForB(renderer, true);
            drawLightForC(renderer, false);
            drawLightForD(renderer, true);
            break;
        case 4: // Road D green
            drawLightForA(renderer, true);
            drawLightForB(renderer, true);
            drawLightForC(renderer, true);
            drawLightForD(renderer, false);
            break;
    }
    SDL_RenderPresent(renderer);
    sharedData->currentLight = sharedData->nextLight;
}

void* chequeQueue(void* arg){
    SharedData* sharedData = (SharedData*)arg;
    while (1) {
        // Priority condition: if Road A has >10 vehicles
        int sizeA = (queueA.rear - queueA.front + MAX_VEHICLES) % MAX_VEHICLES + 1;
        if (!isEmpty(&queueA) && sizeA > 10) {
            sharedData->nextLight = 2; // green for A
            printf("Priority: Serving Road A\n");
            while (sizeA > 5) {
                Vehicle v = dequeue(&queueA);
                printf("Vehicle %s passed from Road A\n", v.vehicleNumber);
                sizeA = (queueA.rear - queueA.front + MAX_VEHICLES) % MAX_VEHICLES + 1;
                sleep(1);
            }
        } else {
            // Normal round robin: serve B, then C, then D
            if (!isEmpty(&queueB)) {
                sharedData->nextLight = 2;
                Vehicle v = dequeue(&queueB);
                printf("Vehicle %s passed from Road B\n", v.vehicleNumber);
                sleep(2);
            }
            if (!isEmpty(&queueC)) {
                sharedData->nextLight = 2;
                Vehicle v = dequeue(&queueC);
                printf("Vehicle %s passed from Road C\n", v.vehicleNumber);
                sleep(2);
            }
            if (!isEmpty(&queueD)) {
                sharedData->nextLight = 2;
                Vehicle v = dequeue(&queueD);
                printf("Vehicle %s passed from Road D\n", v.vehicleNumber);
                sleep(2);
            }
        }
    }
}

//pass the queue on this function for sharing the data
void* readAndParseFile(void* arg) {
    while(1){
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) { perror("Error opening file"); continue; }

        char line[MAX_LINE_LENGTH];
        while (fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0;
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":");

            if (vehicleNumber && road) {
                Vehicle v;
                strcpy(v.vehicleNumber, vehicleNumber);
                strcpy(v.road, road);

                if (strcmp(road, "A") == 0) enqueue(&queueA, v);
                else if (strcmp(road, "B") == 0) enqueue(&queueB, v);
                else if (strcmp(road, "C") == 0) enqueue(&queueC, v);
                else if (strcmp(road, "D") == 0) enqueue(&queueD, v);

                printf("Enqueued Vehicle %s on Road %s\n", vehicleNumber, road);
            }
        }
                sleep(2);
    }
}
int main()
{
    initQueue(&queueA);
    initQueue(&queueB);
    initQueue(&queueC);
    initQueue(&queueD);
}