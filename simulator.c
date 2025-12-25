#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h> 
#include <string.h>
 
#define MAX_VEHICLES 100
#define MAX_LINE_LENGTH 100
#define MAIN_FONT "assets/fonts/DejaVuSans.ttf" 
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define LANE_WIDTH 50

const char* VEHICLE_FILE = "vehicles.data"; 

//Traffic light 
#define LIGHT_ALL_RED 0
#define LIGHT_A 1
#define LIGHT_B 2
#define LIGHT_C 3
#define LIGHT_D 4


//Adds vehicles and queue structures
typedef struct {
    char vehicleNumber[10];
    char road[2];   // A, B, C, D
} Vehicle;

typedef struct {
    Vehicle data[MAX_VEHICLES];
    int front, rear;
} VehicleQueue;

typedef struct {
    int currentLight;
    int nextLight;
} SharedData;

void initQueue(VehicleQueue* q) 
{ 
    q->front = q->rear = -1; }

bool isEmpty(VehicleQueue* q) { 
    return q->front == -1; }

bool isFull(VehicleQueue* q) { 
    return (q->rear + 1) % MAX_VEHICLES == q->front; }

void enqueue(VehicleQueue* q, Vehicle v) {
    if (isFull(q)) 
    return;
    if (isEmpty(q)) q->front = 0;
    q->rear = (q->rear + 1) % MAX_VEHICLES;
    q->data[q->rear] = v;
}

Vehicle dequeue(VehicleQueue* q) {
Vehicle v = q->data[q->front];
    if (q->front == q->rear) 
    q->front = q->rear = -1;
    else 
    q->front = (q->front + 1) % MAX_VEHICLES;
    return v;

}

//Global queues as well as mutexes
VehicleQueue queueA, queueB, queueC, queueD;
SDL_mutex* mutexA;
SDL_mutex* mutexB;
SDL_mutex* mutexC; 
SDL_mutex* mutexD;

//File Reader 
int readVehicleFile(void* arg) {
    FILE* file = fopen(VEHICLE_FILE, "r");
    if (!file) 
    {
        perror("Error opening vehicle file");
        return -1;
    }
    fseek(file, 0, SEEK_END); // Skip to old enteries
    char line[MAX_LINE_LENGTH];

    while(1) {
        if(fgets(line, sizeof(line), file)) 
        { // Process the line
            line[strcspn(line, "\n")] = 0; // Remove newline
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":");

            if (vehicleNumber && road) {
                Vehicle v;
                strcpy(v.vehicleNumber, vehicleNumber);
                strcpy(v.road, road);

                if (strcmp(road, "A") == 0){
                    SDL_LockMutex(mutexA);
                    enqueue(&queueA, v);
                    SDL_UnlockMutex(mutexA);
                }
                else if (strcmp(road, "B") == 0){
                    SDL_LockMutex(mutexB);
                    enqueue(&queueB, v);
                    SDL_UnlockMutex(mutexB);
                }
                else if (strcmp(road, "C") == 0){
                    SDL_LockMutex(mutexC);
                    enqueue(&queueC, v);
                    SDL_UnlockMutex(mutexC);
                }
                else if (strcmp(road, "D") == 0){
                    SDL_LockMutex(mutexD);
                    enqueue(&queueD, v);
                    SDL_UnlockMutex(mutexD);
                }   
                printf("Enqueued Vehicle %s on Road %s\n", vehicleNumber, road);
        } 
        else {
            clearerr(file); // Clear EOF flag
            SDL_Delay(500); // Wait before retrying
        }
    }
    fclose(file);
    return 0;
}

int chequeQueue(void* arg)
{
    SharedData* sharedData = (SharedData*)arg;
    while (1) {
     // Priority condition: if Road A has >10 vehicles
        if (!isEmpty(&queueA)) {
            SDL_LockMutex(mutexA);
            Vehicle v = dequeue(&queueA);
            SDL_UnlockMutex(mutexA);
            sharedData->nextLight = LIGHT_A;
            printf("Vehicle %s passed from Road A\n", v.vehicleNumber);
            SDL_Delay(1000);
            }

            if (!isEmpty(&queueB)) 
            {
            SDL_LockMutex(mutexB);
            Vehicle v = dequeue(&queueB);
            SDL_UnlockMutex(mutexB);
            sharedData->nextLight = LIGHT_B;
            printf("Vehicle %s passed from Road B\n", v.vehicleNumber);
            SDL_Delay(1000);

            }
            if (!isEmpty(&queueC)) {
    SDL_LockMutex(mutexC);
                Vehicle v = dequeue(&queueC);
                SDL_UnlockMutex(mutexC);
                sharedData->nextLight = LIGHT_C;
                printf("Vehicle %s passed from Road C\n", v.vehicleNumber);
                SDL_Delay(1000);
            }
            if (!isEmpty(&queueD)) {
                SDL_LockMutex(mutexD);
                Vehicle v = dequeue(&queueD);
                SDL_UnlockMutex(mutexD);
                sharedData->nextLight = LIGHT_D;
                printf("Vehicle %s passed from Road D\n", v.vehicleNumber);
                SDL_Delay(1000);
            }
        }
        return 0;
}

// Function declarations
bool initializeSDL(SDL_Window **window, SDL_Renderer **renderer);
void drawRoads(SDL_Renderer *renderer);
void displayText(SDL_Renderer *renderer, char *text, int x, int y);
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

void drawArrow(SDL_Renderer* renderer, int x1, int y1, int x2, int y2, int x3, int y3) {
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

// Draw traffic light boxes
void drawLight(SDL_Renderer *renderer, int x, int y, bool isRed) {
    SDL_Rect box = {x, y, 30, 30};
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    SDL_RenderFillRect(renderer, &box);

    if (isRed) SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    else SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);

    SDL_Rect light = {x+5, y+5, 20, 20};
    SDL_RenderFillRect(renderer, &light);
}


void drawRoads(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_SetRenderDrawColor(renderer, 200,200,200,255);
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
    displayText(renderer, font, "A",WINDOW_WIDTH/2 - 10, 10);
    displayText(renderer, font, "B",WINDOW_WIDTH/2 - 10, WINDOW_HEIGHT - 30);
    displayText(renderer, font, "C",WINDOW_WIDTH/2 - 10, WINDOW_HEIGHT/2 - 10);
    displayText(renderer, font, "D",10, WINDOW_HEIGHT/2 - 10);
    
}

void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, textColor);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &rect.w, &rect.h);
    SDL_RenderCopy(renderer, texture, NULL, &rect);
   SDL_DestroyTexture(texture);
}

// Refresh lights based on SharedData
void refreshLights(SDL_Renderer *renderer, TTF_Font *font, SharedData *sharedData) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);

    drawRoads(renderer, font);

    // Default all red
    bool redA = true, redB = true, redC = true, redD = true;

    switch (sharedData->nextLight) {
        case LIGHT_A: redA = false; break;
        case LIGHT_B: redB = false; break;
        case LIGHT_C: redC = false; break;
        case LIGHT_D: redD = false; break;
        default: break;
    }

    // Position lights near each road
    drawLight(renderer, WINDOW_WIDTH/2 - 15, 100, redA);          // Top (A)
    drawLight(renderer, WINDOW_WIDTH/2 - 15, WINDOW_HEIGHT-130, redB); // Bottom (B)
    drawLight(renderer, WINDOW_WIDTH-130, WINDOW_HEIGHT/2 - 15, redC); // Right (C)
    drawLight(renderer, 100, WINDOW_HEIGHT/2 - 15, redD);         // Left (D)

    SDL_RenderPresent(renderer);
}


//pass the queue on this function for sharing the data
void* readAndParseFile(void* arg) {
    while(1){
        FILE* file = fopen(VEHICLE_FILE, "r");
        if (!file) { perror("Error opening file"); continue;
         }

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

    int main() {
    // Initialize queues
    initQueue(&queueA);
    initQueue(&queueB);
    initQueue(&queueC);
    initQueue(&queueD);

    // Initialize mutexes
    mutexA = SDL_CreateMutex();
    mutexB = SDL_CreateMutex();
    mutexC = SDL_CreateMutex();
    mutexD = SDL_CreateMutex();
if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) return -1;
    if (TTF_Init() == -1) return -1;

    SDL_Window* window = SDL_CreateWindow("Traffic Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SharedData sharedData = {0, 0};

    // Start threads
    SDL_Thread* tQueue = SDL_CreateThread(chequeQueue, "QueueThread", &sharedData);
    SDL_Thread* tReadFile = SDL_CreateThread(readAndParseFile, "FileThread", NULL);

    // Load font
    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
    }

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        refreshLights(renderer, font, &sharedData);
        SDL_Delay(50);
    }

    // Cleanup
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();

    SDL_DestroyMutex(mutexA);
    SDL_DestroyMutex(mutexB);
    SDL_DestroyMutex(mutexC);
    SDL_DestroyMutex(mutexD);

    return 0;
}