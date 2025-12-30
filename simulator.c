#define SDL_MAIN_HANDLED
#include "simulator.h"
#include "receiver.h"
#include "traffic_generator.h" 

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h> 
#include <string.h>


//Configuration constants
#define MAIN_FONT "assets/fonts/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define LANE_WIDTH 50

#define MAX_SPRITES 50 

//File based communication
const char* VEHICLE_FILE = "vehicles.data"; 


void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y);

VehicleSprite sprites[MAX_SPRITES];

//Globals
VehicleQueue queueA; VehicleQueue queueB; VehicleQueue queueC; VehicleQueue queueD;


SDL_mutex* mutexA= NULL;
SDL_mutex* mutexB= NULL;
SDL_mutex* mutexC= NULL;
SDL_mutex* mutexD= NULL;

//Queue Helpers
void initQueue(VehicleQueue* q) 
{ 
    q->front = 0;
    q->rear = 0; 
}
bool isEmpty(VehicleQueue* q) { 
    return q->front == q->rear; 
}
bool isFull(VehicleQueue* q) { 
    return ((q->rear + 1) % MAX_VEHICLES) == q->front; 
}

int enqueue(VehicleQueue* q, Vehicle v) {
    if (isFull(q)) return 0; // fail
    q->data[q->rear] = v;
    q->rear = (q->rear + 1) % MAX_VEHICLES;
    return 1;
}

int dequeue(VehicleQueue* q, Vehicle* v) {
    if (isEmpty(q)) return 0; // fail
    *v = q->data[q->front];
    q->front = (q->front + 1) % MAX_VEHICLES;
    return 1;
}
int queueSize(VehicleQueue* q) {
    return (q->rear - q->front + MAX_VEHICLES) % MAX_VEHICLES;
}

void init_mutexes() {
    mutexA = SDL_CreateMutex();
    mutexB = SDL_CreateMutex();
    mutexC = SDL_CreateMutex();
    mutexD = SDL_CreateMutex();
    if (!mutexA || !mutexB || !mutexC || !mutexD) {
        fprintf(stderr, "Failed to create mutexes!\n");
        exit(1);
    }
}  
//Vehicle Sprite spwan
void spawnVehicleSprite(char road, SharedData* sharedData, bool canMove) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!sprites[i].active) {
            sprites[i].active = true;

        // Random size: small car (25x12) to truck (40x18)
        sprites[i].width = 25 + rand() % 16;   // 25–40
        sprites[i].height = 12 + rand() % 7;   // 12–18

    // Base speed factor: more vehicles = faster movement
            float speedFactor = 1.0f;
            if (sharedData->lastServedCount > 0) {
                speedFactor += sharedData->lastServedCount / 5.0f; 
                // e.g., 5 vehicles → 2x speed
            }

        int laneOffset = (i % 2 == 0) ? -LANE_WIDTH/4 : LANE_WIDTH/4;

            switch (road) {
                case 'A': // from top
                    sprites[i].x = WINDOW_WIDTH/2 + laneOffset;
                    sprites[i].y = 0;
                    sprites[i].dx = 0;
                    sprites[i].dy = (int)(5 * speedFactor);
                    sprites[i].color = (SDL_Color){255, 0, 0, 255}; // red
                    break;
                case 'B': // from bottom
                    sprites[i].x = WINDOW_WIDTH/2 + laneOffset;
                    sprites[i].y = WINDOW_HEIGHT;
                    sprites[i].dx = 0;
                    sprites[i].dy = (int)(-5 * speedFactor);
                    sprites[i].color = (SDL_Color){0, 255, 0, 255}; // green
                    break;
                case 'C': // from right
                    sprites[i].x = WINDOW_WIDTH;
                    sprites[i].y = WINDOW_HEIGHT/2+ laneOffset;
                    sprites[i].dx = (int)(-5 * speedFactor);
                    sprites[i].dy = 0;
                    sprites[i].color = (SDL_Color){0, 0, 255, 255}; // blue
                    break;
                case 'D': // from left
                    sprites[i].x = 0;
                    sprites[i].y = WINDOW_HEIGHT/2+ laneOffset;
                    sprites[i].dx = (int)(5 * speedFactor);
                    sprites[i].dy = 0;
                    sprites[i].color = (SDL_Color){255, 255, 0, 255}; // yellow
                    break;
            }
            break;
        }
    }
}

void updateSprites(SharedData* sharedData) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].active) {
            sprites[i].x += sprites[i].dx;
            sprites[i].y += sprites[i].dy;

            if (sprites[i].x < -50 || sprites[i].x > WINDOW_WIDTH + 50 || sprites[i].y < -50 || sprites[i].y > WINDOW_HEIGHT + 50) {
                sprites[i].active = false;
            }
        }
    }
}

void drawSprites(SDL_Renderer *renderer) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].active) {
            //Car body
            SDL_SetRenderDrawColor(renderer, 
                sprites[i].color.r,
                sprites[i].color.g,
                sprites[i].color.b,
                sprites[i].color.a);
            SDL_Rect car = {(int)sprites[i].x, (int)sprites[i].y, sprites[i].width, sprites[i].height};
            SDL_RenderFillRect(renderer, &car);
        }
    }
}


//Queue Thread: check queues and manage lights
int chequeQueue(void* arg)
{
    SharedData* sharedData = (SharedData*)arg;
    int rrIndex=0;
    const int MIN_GREEN_TIME = 5000; // 5 seconds minimum
    const int PRIORITY_EXTRA_TIME = 2000; // 2 seconds extra for priority
    while (1) {
        // Read sizes safely
        SDL_LockMutex(mutexA); int sizeA = queueSize(&queueA); SDL_UnlockMutex(mutexA);
        SDL_LockMutex(mutexB); int sizeB = queueSize(&queueB); SDL_UnlockMutex(mutexB);
        SDL_LockMutex(mutexC); int sizeC = queueSize(&queueC); SDL_UnlockMutex(mutexC);
        SDL_LockMutex(mutexD); int sizeD = queueSize(&queueD); SDL_UnlockMutex(mutexD);

        // Priority condition
        if (sizeA > 10) {
            sharedData->nextLight = LIGHT_A;
            int servedCount = 0;
            Vehicle v;

            while (1) {
                SDL_LockMutex(mutexA);
                sizeA = queueSize(&queueA);
                if (sizeA <= 5 || isEmpty(&queueA)) {
                    SDL_UnlockMutex(mutexA);
                    break;
                }
                bool success = dequeue(&queueA, &v);
                SDL_UnlockMutex(mutexA);

                if (success) {
                    printf("PRIORITY: Vehicle %s passed from Road A\n", v.vehicleNumber);
                    spawnVehicleSprite(v.road[0], sharedData, true);
                    SDL_Delay(PRIORITY_PASS_TIME);
                    servedCount++;
                }
            }

            sharedData->lastServedCount = servedCount;
            sharedData->lastGreenDuration = servedCount * PRIORITY_PASS_TIME;
            sharedData->remainingTime = sharedData->lastGreenDuration;
            if (sharedData->lastGreenDuration < MIN_GREEN_TIME)
                sharedData->lastGreenDuration = MIN_GREEN_TIME;

            sharedData->remainingTime = sharedData->lastGreenDuration;
        } else {
            // Normal round-robin
            int totalWait = sizeA + sizeB + sizeC + sizeD;
            if (totalWait == 0) {
                sharedData->nextLight = LIGHT_ALL_RED;
                SDL_Delay(200);
                continue;
            }

            int order[4] = {LIGHT_B, LIGHT_C, LIGHT_D, LIGHT_A};
            bool served = false;

            for (int k=0; k<4; k++) {
                int light = order[(rrIndex + k) % 4];
                Vehicle v;
                int servedCount = 0;

                switch(light) {
                    case LIGHT_A: SDL_LockMutex(mutexA); if(dequeue(&queueA,&v)) servedCount=1; SDL_UnlockMutex(mutexA); break;
                    case LIGHT_B: SDL_LockMutex(mutexB); if(dequeue(&queueB,&v)) servedCount=1; SDL_UnlockMutex(mutexB); break;
                    case LIGHT_C: SDL_LockMutex(mutexC); if(dequeue(&queueC,&v)) servedCount=1; SDL_UnlockMutex(mutexC); break;
                    case LIGHT_D: SDL_LockMutex(mutexD); if(dequeue(&queueD,&v)) servedCount=1; SDL_UnlockMutex(mutexD); break;
                }

                if (servedCount) {
                    sharedData->nextLight = light;
                    printf("Vehicle %s passed from Road %c\n", v.vehicleNumber, v.road[0]);
                    bool canMove = (sharedData->nextLight == LIGHT_A && v.road[0]=='A') ||
                    (sharedData->nextLight == LIGHT_B && v.road[0]=='B') ||
                    (sharedData->nextLight == LIGHT_C && v.road[0]=='C') ||
                    (sharedData->nextLight == LIGHT_D && v.road[0]=='D');
                    spawnVehicleSprite(v.road[0], sharedData, canMove);
                    SDL_Delay(VEHICLE_PASS_TIME);

                    sharedData->lastServedCount = servedCount;
                    sharedData->lastGreenDuration = servedCount * VEHICLE_PASS_TIME*3;
                    sharedData->remainingTime = sharedData->lastGreenDuration;

                    if (sharedData->lastGreenDuration < MIN_GREEN_TIME)
                        sharedData->lastGreenDuration = MIN_GREEN_TIME;

                    sharedData->remainingTime = sharedData->lastGreenDuration;
                    served = true;
                    break;
                }
            }
            rrIndex = (rrIndex + 1) % 4;
            SDL_Delay(50);
        }
    }

    return 0;
}


 void swap(int *a, int *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

//Drawing
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
    int radius = 10;  // circle radius

    // Draw outer gray circle (housing)
    SDL_SetRenderDrawColor(renderer, 150, 150, 150, 255);
    for (int w = 0; w < 3; w++) { // thickness
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                if (dx*dx + dy*dy <= radius*radius) {
                    SDL_RenderDrawPoint(renderer, x + dx, y + dy);
                }
            }
        }
    }

    // Draw inner colored circle
    SDL_Color color = isRed ? (SDL_Color){255,0,0,255} : (SDL_Color){0,200,0,255};
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius+2; dy <= radius-2; dy++) {
        for (int dx = -radius+2; dx <= radius-2; dx++) {
            if (dx*dx + dy*dy <= (radius-2)*(radius-2)) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}


void drawRoads(SDL_Renderer *renderer, TTF_Font *font) {
    // Draw green grass background
    SDL_SetRenderDrawColor(renderer, 120, 200, 120, 255); // light green
    SDL_RenderClear(renderer);

    // Draw roads (dark asphalt)
    SDL_SetRenderDrawColor(renderer, 50, 50, 50, 255); // dark gray

    // Vertical road
    SDL_Rect verticalRoad = {WINDOW_WIDTH / 2 - ROAD_WIDTH / 2, 0, ROAD_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &verticalRoad);

    // Horizontal road
    SDL_Rect horizontalRoad = {0, WINDOW_HEIGHT / 2 - ROAD_WIDTH / 2, WINDOW_WIDTH, ROAD_WIDTH};
    SDL_RenderFillRect(renderer, &horizontalRoad);

    // Draw dashed lane separators
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white lines
    int dashLength = 15, gap = 10;

    // Vertical lanes
    for (int i = 1; i < ROAD_WIDTH / LANE_WIDTH; i++) {
        int x = WINDOW_WIDTH/2 - ROAD_WIDTH/2 + i*LANE_WIDTH;
        for (int y = 0; y < WINDOW_HEIGHT; y += dashLength + gap) {
            SDL_RenderDrawLine(renderer, x, y, x, y + dashLength);
        }
    }

    // Horizontal lanes
    for (int i = 1; i < ROAD_WIDTH / LANE_WIDTH; i++) {
        int y = WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + i*LANE_WIDTH;
        for (int x = 0; x < WINDOW_WIDTH; x += dashLength + gap) {
            SDL_RenderDrawLine(renderer, x, y, x + dashLength, y);
        }
    }
    // Road labels
        int offset = 20;    
        displayText(renderer, font, "A",WINDOW_WIDTH/2 - 10, offset);
        displayText(renderer, font, "B",WINDOW_WIDTH/2 - 10, WINDOW_HEIGHT - ROAD_WIDTH + offset);
        displayText(renderer, font, "C",WINDOW_WIDTH - offset, WINDOW_HEIGHT/2 - 10);
        displayText(renderer, font, "D",offset, WINDOW_HEIGHT/2 - 10);
        
        // Priority label
        displayText(renderer, font, "Priority Lane: AL2", 20, 20);
    }


void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y){
    // display necessary text
    SDL_Color textColor = {0, 0, 0, 255}; // black color
    SDL_Surface *surface = TTF_RenderText_Solid(font, text, textColor);
    if(!surface) {
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(!texture) {
        return;
    }
    SDL_Rect textRect = {x,y,0,0 };
    SDL_QueryTexture(texture, NULL, NULL, &textRect.w, &textRect.h);
    SDL_RenderCopy(renderer, texture, NULL, &textRect);
   SDL_DestroyTexture(texture);
}

void drawHUD(SDL_Renderer *renderer, TTF_Font *font, SharedData *sharedData) {
  int sizeA = queueSize(&queueA);

    // Priority status
    if (sizeA > 10) {
        displayText(renderer, font, "PRIORITY ACTIVE: Road A (AL2)", 20, 180);
}
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
    // Draw traffic lights
    // Top of intersection
    drawLight(renderer, WINDOW_WIDTH/2 - 50, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 - 20, redA); 

    // Bottom of intersection
    drawLight(renderer, WINDOW_WIDTH/2 + 20, WINDOW_HEIGHT/2 + ROAD_WIDTH/2 + 5, redB);

    // Right of intersection
    drawLight(renderer, WINDOW_WIDTH/2 + ROAD_WIDTH/2 + 5, WINDOW_HEIGHT/2 - 50, redC);

    // Left of intersection
    drawLight(renderer, WINDOW_WIDTH/2 - ROAD_WIDTH/2 - 20, WINDOW_HEIGHT/2 + 20, redD);
           // Left (D)

    drawHUD(renderer, font, sharedData);

}

//pass the queue on this function for sharing the data
int readAndParseFiles(void* arg)
{
    SharedData* sharedData = (SharedData*)arg;

    while (1) {
        // Read each lane file safely
        const char* laneFiles[4] = {"laneA.txt", "laneB.txt", "laneC.txt", "laneD.txt"};
        VehicleQueue* laneQueues[4] = { &queueA, &queueB, &queueC, &queueD};
        SDL_mutex* mutexes[4] = {mutexA, mutexB, mutexC, mutexD};

        for (int i = 0; i < 4; i++) {
            FILE* fp = fopen(laneFiles[i], "r");
            if (!fp) continue; // file might not exist yet

            char line[128];
            while (fgets(line, sizeof(line), fp)) {
                line[strcspn(line, "\n")] = 0; // remove newline
                if (strlen(line) == 0) continue;

                Vehicle v;
                memset(&v, 0, sizeof(Vehicle));
                strncpy(v.vehicleNumber, line, sizeof(v.vehicleNumber)-1);
                v.road[0] = 'A' + i; // road letter

                SDL_LockMutex(mutexes[i]);
                enqueue(laneQueues[i], v);
                SDL_UnlockMutex(mutexes[i]);
            }
            fclose(fp);

            // Clear the file after reading to avoid duplicates
            fp = fopen(laneFiles[i], "w");
            if (fp) fclose(fp);
        }

        SDL_Delay(100); // wait 100ms before next check
    }

    return 0;
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

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) 
    {
    SDL_Log("SDL_Init Error: %s", SDL_GetError());
    return -1;
    }

    if (TTF_Init() == -1){
        SDL_Log("TTF_Init Error: %s", TTF_GetError());
        return -1;
    } 

    SDL_Window* window = SDL_CreateWindow("Traffic Simulation",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
            SDL_Log("Window creation failed: %s", SDL_GetError());
            TTF_Quit();
            SDL_Quit();
            return -1;
            }


    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
            SDL_Log("Renderer creation failed: %s", SDL_GetError());
            SDL_DestroyWindow(window);
            TTF_Quit();
            SDL_Quit();
            return -1;
        }
    SharedData sharedData = {LIGHT_ALL_RED, LIGHT_ALL_RED, 0, 0, 0};

    // Start threads
    SDL_Thread* tQueue = SDL_CreateThread(chequeQueue, "QueueThread", &sharedData);
    SDL_Thread* tReadFile = SDL_CreateThread(readAndParseFiles, "FileThread", &sharedData);
    SDL_Thread* tReceiver = SDL_CreateThread((int(*)(void*))start_receiver, "ReceiverThread", NULL); 
    SDL_Thread* tGenerator = SDL_CreateThread(start_traffic_generator, "TrafficGeneratorThread", NULL);

    // Load font
    TTF_Font* font = TTF_OpenFont(MAIN_FONT, 24);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
    }

    bool localRunning = true;
    SDL_Event event;

    while (localRunning) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                localRunning = false;
            }
        }
        updateSprites(&sharedData);
        refreshLights(renderer, font, &sharedData);
        drawSprites(renderer);
        SDL_RenderPresent(renderer);

        SDL_Delay(50);
        sharedData.remainingTime -= 50;
        if (sharedData.remainingTime < 0)
        sharedData.remainingTime = 0;
    }
    // Wait for threads to finish
    SDL_WaitThread(tQueue, NULL);
    SDL_WaitThread(tReadFile, NULL);
    SDL_WaitThread(tReceiver, NULL);

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

    printf("Simulation exited cleanly.\n");
    return 0;
}