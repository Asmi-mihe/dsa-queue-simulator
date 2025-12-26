#define SDL_MAIN_HANDLED

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h> 
#include <string.h>
 
//Configuration constants
#define MAX_VEHICLES 100
#define MAX_LINE_LENGTH 100
#define MAIN_FONT "assets/fonts/DejaVuSans.ttf"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define ROAD_WIDTH 150
#define LANE_WIDTH 50

#define VEHICLE_PASS_TIME 800   // milliseconds per vehicle in normal mode
#define PRIORITY_PASS_TIME 600  // milliseconds per vehicle in priority mode
#define MAX_SPRITES 50 

//File based communication
const char* VEHICLE_FILE = "vehicles.data"; 

//Traffic light States
#define LIGHT_ALL_RED 0
#define LIGHT_A 1
#define LIGHT_B 2
#define LIGHT_C 3
#define LIGHT_D 4

void displayText(SDL_Renderer *renderer, TTF_Font *font, const char *text, int x, int y);


//Data structures
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
    int lastServedCount;   // number of vehicles served in last cycle
    int lastGreenDuration; // computed green time in ms
    int remainingTime;     // time left for current green in ms
} SharedData;

typedef struct {
    int x, y;          // position
    int dx, dy;        // movement per frame
    bool active;       // is this vehicle currently moving
    SDL_Color color; // car color
    int width, height; // randomized size
} VehicleSprite;

VehicleSprite sprites[MAX_SPRITES];

//Globals
VehicleQueue queueA, queueB, queueC, queueD;
SDL_mutex* mutexA;
SDL_mutex* mutexB;
SDL_mutex* mutexC;
SDL_mutex* mutexD;

//Queue Helpers
void initQueue(VehicleQueue* q) 
{ 
    q->front = q->rear = -1; 
}
bool isEmpty(VehicleQueue* q) { 
    return q->front == -1; 
}
bool isFull(VehicleQueue* q) { 
    return (q->rear + 1) % MAX_VEHICLES == q->front; 
}

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
int queueSize(VehicleQueue* q) {
    if (isEmpty(q)) 
    return 0;
    return (q->rear - q->front + MAX_VEHICLES) % MAX_VEHICLES + 1;
}
//Vehicle Sprite spwan
void spawnVehicleSprite(char road, SharedData* sharedData) {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (!sprites[i].active) {
            sprites[i].active = true;

        // Random size: small car (25x12) to truck (40x18)
        sprites[i].width = 25 + rand() % 16;   // 25–40
        sprites[i].height = 12 + rand() % 7;   // 12–18

    // Base speed factor: more vehicles = faster movement
            float speedFactor = 1.0f;
            if (sharedData->lastServedCount > 0) {
                speedFactor = 1.0f + (sharedData->lastServedCount / 5.0f); 
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
                    sprites[i].y = WINDOW_HEIGHT/2;
                    sprites[i].dx = (int)(-5 * speedFactor);
                    sprites[i].dy = 0;
                    sprites[i].color = (SDL_Color){0, 0, 255, 255}; // blue
                    break;
                case 'D': // from left
                    sprites[i].x = 0;
                    sprites[i].y = WINDOW_HEIGHT/2;
                    sprites[i].dx = (int)(5 * speedFactor);
                    sprites[i].dy = 0;
                    sprites[i].color = (SDL_Color){255, 255, 0, 255}; // yellow
                    break;
            }
            break;
        }
    }
}

void updateSprites() {
    for (int i = 0; i < MAX_SPRITES; i++) {
        if (sprites[i].active) {
            sprites[i].x += sprites[i].dx;
            sprites[i].y += sprites[i].dy;
            if (sprites[i].x < 0 || sprites[i].x > WINDOW_WIDTH || sprites[i].y < 0 || sprites[i].y > WINDOW_HEIGHT) {
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
            SDL_Rect car = {sprites[i].x, sprites[i].y, sprites[i].width, sprites[i].height};
            SDL_RenderFillRect(renderer, &car);

        // Headlights (white rectangles at front depending on direction)
            SDL_SetRenderDrawColor(renderer, 255, 255, 200, 255); // pale yellow
            SDL_Rect headlight1, headlight2;

            if (sprites[i].dx > 0) { // moving right
                headlight1 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y, 5, 5};
                headlight2 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y + sprites[i].height - 5, 5, 5};
            } 
            else if 
            (sprites[i].dx < 0) { // moving left
                headlight1 = (SDL_Rect){sprites[i].x - sprites[i].width, sprites[i].y, 5, 5};
                headlight2 = (SDL_Rect){sprites[i].x - sprites[i].width, sprites[i].y + sprites[i].height - 5, 5, 5};
            } 
            else if 
            (sprites[i].dy > 0) { // moving down
                headlight1 = (SDL_Rect){sprites[i].x, sprites[i].y + sprites[i].height, 5, 5};
                headlight2 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y + sprites[i].height, 5, 5};
            } else if 
            (sprites[i].dy < 0) { // moving up
                headlight1 = (SDL_Rect){sprites[i].x, sprites[i].y - sprites[i].height, 5, 5};
                headlight2 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y - sprites[i].height, 5, 5};
            }

            SDL_RenderFillRect(renderer, &headlight1);
            SDL_RenderFillRect(renderer, &headlight2);

            // Tail-lights (back)
            SDL_SetRenderDrawColor(renderer, 200, 0, 0, 255);
            SDL_Rect taillight1, taillight2;
        if (sprites[i].dx > 0) { // moving right → tail at left
                taillight1 = (SDL_Rect){sprites[i].x - 5, sprites[i].y, 5, 5};
                taillight2 = (SDL_Rect){sprites[i].x - 5, sprites[i].y + sprites[i].height, 5, 5};
            } else if (sprites[i].dx < 0) { // moving left → tail at right
                taillight1 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y, 5, 5};
                taillight2 = (SDL_Rect){sprites[i].x + sprites[i].width, sprites[i].y + sprites[i].height, 5, 5};
            } else if (sprites[i].dy > 0) { // moving down → tail at top
                taillight1 = (SDL_Rect){sprites[i].x, sprites[i].y - 5, 5, 5};
                taillight2 = (SDL_Rect){sprites[i].x + sprites[i].width - 5, sprites[i].y - 5, 5, 5};
            } else if (sprites[i].dy < 0) { // moving up → tail at bottom
                taillight1 = (SDL_Rect){sprites[i].x, sprites[i].y + sprites[i].height, 5, 5};
                taillight2 = (SDL_Rect){sprites[i].x + sprites[i].width - 5, sprites[i].y + sprites[i].height, 5, 5};
            }

            SDL_RenderFillRect(renderer, &taillight1);
            SDL_RenderFillRect(renderer, &taillight2);

        }
    }
}


//Queue Thread: check queues and manage lights
int chequeQueue(void* arg)
{
    SharedData* sharedData = (SharedData*)arg;
    int rrIndex=0;
    while (1) {
// Read sizes safely
        SDL_LockMutex(mutexA); 
        int sizeA = queueSize(&queueA); 
        SDL_UnlockMutex(mutexA);

        SDL_LockMutex(mutexB); 
        int sizeB = queueSize(&queueB); 
        SDL_UnlockMutex(mutexB);

        SDL_LockMutex(mutexC); 
        int sizeC = queueSize(&queueC); 
        SDL_UnlockMutex(mutexC);

        SDL_LockMutex(mutexD); 
        int sizeD = queueSize(&queueD); 
        SDL_UnlockMutex(mutexD);

     // Priority condition: if Road A has >10 vehicles
        if (sizeA > 10) {
            sharedData->nextLight = LIGHT_A;
            int servedCount = 0;
            while (1) {
                SDL_LockMutex(mutexA);
                sizeA = queueSize(&queueA);
                if (sizeA <= 5 || isEmpty(&queueA)){ 
                break; }
                
                SDL_LockMutex(mutexA);    
                Vehicle v = dequeue(&queueA);
                SDL_UnlockMutex(mutexA);

                printf("PRIORITY: Vehicle %s passed from Road A (AL2)\n", v.vehicleNumber);
                spawnVehicleSprite(v.road[0], sharedData);
                SDL_Delay(PRIORITY_PASS_TIME);
                servedCount++;
            }
            // After serving, record stats
                sharedData->lastServedCount = servedCount;
                sharedData->lastGreenDuration = servedCount * PRIORITY_PASS_TIME;
                sharedData->remainingTime = sharedData->lastGreenDuration;

            // After priority clearance, continue to normal loop
        } else {
            // Compute total waiting across normal lanes (B,C,D), A is also considered but without priority
            // Normal condition: round-robin fairness
            int totalWait = sizeA + sizeB + sizeC + sizeD;
            if (totalWait == 0) {
                sharedData->nextLight = LIGHT_ALL_RED;
                SDL_Delay(200);
                continue;
            }

            int order[4] = {LIGHT_B, LIGHT_C, LIGHT_D, LIGHT_A};
            bool served = false;
            Vehicle v;
            for (int k = 0; k < 4; k++) {
                int light = order[(rrIndex + k) % 4];
                int servedCount=0;
                switch (light) {
                    case LIGHT_B:
                        SDL_LockMutex(mutexB);
                        if (!isEmpty(&queueB)) { 
                            v = dequeue(&queueB); 
                            servedCount = 1; }
                        SDL_UnlockMutex(mutexB);
                        if (servedCount) { 
                            sharedData->nextLight = LIGHT_B; 
                            printf("Vehicle %s passed from Road B\n", v.vehicleNumber); 
                            spawnVehicleSprite(v.road[0], sharedData);
                            SDL_Delay(VEHICLE_PASS_TIME); 
                            sharedData->lastServedCount = servedCount;
                            sharedData->lastGreenDuration = servedCount *VEHICLE_PASS_TIME;
                            sharedData->remainingTime = sharedData->lastGreenDuration;
                            served = true;
                            break;

                        }

                    case LIGHT_C:
                        SDL_LockMutex(mutexC);
                        if (!isEmpty(&queueC)) { 
                            v = dequeue(&queueC); 
                            servedCount = 1; }
                        SDL_UnlockMutex(mutexC);
                        if (servedCount) { 
                            sharedData->nextLight = LIGHT_C; 
                            printf("Vehicle %s passed from Road C\n", v.vehicleNumber); 
                            spawnVehicleSprite(v.road[0], sharedData);
                            SDL_Delay(VEHICLE_PASS_TIME); 
                            sharedData->lastServedCount = servedCount;
                            sharedData->lastGreenDuration = servedCount * VEHICLE_PASS_TIME;
                            sharedData->remainingTime = sharedData->lastGreenDuration;
                            served = true;
                            break;
                        }

                    case LIGHT_D:
                        SDL_LockMutex(mutexD);
                        if (!isEmpty(&queueD)) { 
                            v = dequeue(&queueD); 
                            servedCount = 1; }
                        SDL_UnlockMutex(mutexD);
                        if (servedCount) { 
                            sharedData->nextLight = LIGHT_D; 
                            printf("Vehicle %s passed from Road D\n", v.vehicleNumber); 
                            spawnVehicleSprite(v.road[0], sharedData);
                            SDL_Delay(VEHICLE_PASS_TIME); 
                            sharedData->lastServedCount = servedCount;
                            sharedData->lastGreenDuration = servedCount * VEHICLE_PASS_TIME;
                            sharedData->remainingTime = sharedData->lastGreenDuration;
                            served = true;
                            break;
                        }
                        
                    case LIGHT_A:
                        // A in normal mode (acts as a normal lane when <=10)
                        SDL_LockMutex(mutexA);
                        if (!isEmpty(&queueA)) { 
                            v = dequeue(&queueA); 
                            servedCount = 1;
                            SDL_UnlockMutex(mutexA); 
                            break;
                        }
                        if (servedCount) { 
                            sharedData->nextLight = light; 
                            printf("Vehicle %s passed from Road A (normal)\n", v.vehicleNumber, light + 'A');
                            spawnVehicleSprite(v.road[0], sharedData);
                            SDL_Delay(VEHICLE_PASS_TIME); 
                            sharedData->lastServedCount = servedCount;
                            sharedData->lastGreenDuration = servedCount * VEHICLE_PASS_TIME;
                            sharedData->remainingTime = sharedData->lastGreenDuration;
                            served = true;
                            break;
                        }
            }
            rrIndex = (rrIndex + 1) % 4;
            }
            SDL_Delay(50); // brief pause between cycles
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

    // lane separators
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    for(int i=0; i<=3; i++){
        // Horizontal lanes
        SDL_RenderDrawLine(renderer, 
            0, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,  // x1,y1
            WINDOW_WIDTH/2 - ROAD_WIDTH/2, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i // x2, y2
        );

        SDL_RenderDrawLine(renderer,
            WINDOW_WIDTH, WINDOW_HEIGHT/2 - ROAD_WIDTH/2 + LANE_WIDTH*i,
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
    
     // Priority label
    displayText(renderer, font, "Priority Lane: AL2", 20, 20);
}
void drawCountdownBar(SDL_Renderer *renderer, TTF_Font *font, int x, int y, int width, int height, SharedData *sharedData) {
    if (sharedData->lastGreenDuration <= 0) 
    return;

    // Fraction of time left
    float fraction = (float)sharedData->remainingTime / (float)sharedData->lastGreenDuration;
    if (fraction < 0) 
    fraction = 0;

    // Draw shrinking bar
    SDL_SetRenderDrawColor(renderer, 0, 200, 0, 255);
    SDL_Rect bar = {x, y, (int)(width * fraction), height};
    SDL_RenderFillRect(renderer, &bar);

    // Outline
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_Rect outline = {x, y, width, height};
    SDL_RenderDrawRect(renderer, &outline);

    // Show seconds left
    char buffer[32];
    float secondsLeft = sharedData->remainingTime / 1000.0f;
    snprintf(buffer, sizeof(buffer), "%.1f s left", secondsLeft);
    displayText(renderer, font, buffer, x, y - 20);
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
    char buffer[64];

    // Road A
    SDL_LockMutex(mutexA);
    int sizeA = queueSize(&queueA);
    SDL_UnlockMutex(mutexA);
    snprintf(buffer, sizeof(buffer), "Road A: %d vehicles", sizeA);
    displayText(renderer, font, buffer, 20, 60);

    // Road B
    SDL_LockMutex(mutexB);
    int sizeB = queueSize(&queueB);
    SDL_UnlockMutex(mutexB);
    snprintf(buffer, sizeof(buffer), "Road B: %d vehicles", sizeB);
    displayText(renderer, font, buffer, 20, 90);

    // Road C
    SDL_LockMutex(mutexC);
    int sizeC = queueSize(&queueC);
    SDL_UnlockMutex(mutexC);
    snprintf(buffer, sizeof(buffer), "Road C: %d vehicles", sizeC);
    displayText(renderer, font, buffer, 20, 120);

    // Road D
    SDL_LockMutex(mutexD);
    int sizeD = queueSize(&queueD);
    SDL_UnlockMutex(mutexD);
    snprintf(buffer, sizeof(buffer), "Road D: %d vehicles", sizeD);
    displayText(renderer, font, buffer, 20, 150);

    // Priority status
    if (sizeA > 10) {
        displayText(renderer, font, "PRIORITY ACTIVE: Road A (AL2)", 20, 180);}

    float seconds = sharedData->lastGreenDuration / 1000.0f;  // convert ms → seconds
    snprintf(buffer, sizeof(buffer), "Last Green: %d vehicles, %1f s",
    sharedData->lastServedCount,
    seconds);
        displayText(renderer, font, buffer, 20, 210);
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

        if (sharedData->nextLight == LIGHT_A) {
            drawCountdownBar(renderer, font, WINDOW_WIDTH/2 - 40, 140, 80, 10, sharedData);
        }
        if (sharedData->nextLight == LIGHT_B) {
            drawCountdownBar(renderer, font, WINDOW_WIDTH/2 - 40, WINDOW_HEIGHT-100, 80, 10, sharedData);
        }
        if (sharedData->nextLight == LIGHT_C) {
            drawCountdownBar(renderer, font, WINDOW_WIDTH-100, WINDOW_HEIGHT/2 + 40, 80, 10, sharedData);
        }
        if (sharedData->nextLight == LIGHT_D) {
            drawCountdownBar(renderer, font, 40, WINDOW_HEIGHT/2 + 40, 80, 10, sharedData);
        }

    // Position lights near each road
    drawLight(renderer, WINDOW_WIDTH/2 - 15, 100, redA);          // Top (A)
    drawLight(renderer, WINDOW_WIDTH/2 - 15, WINDOW_HEIGHT-130, redB); // Bottom (B)
    drawLight(renderer, WINDOW_WIDTH-130, WINDOW_HEIGHT/2 - 15, redC); // Right (C)
    drawLight(renderer, 100, WINDOW_HEIGHT/2 - 15, redD);         // Left (D)

    drawHUD(renderer, font, sharedData);

}


//pass the queue on this function for sharing the data
int readAndParseFile(void* arg) {
    SharedData* sharedData = (SharedData*)arg;
    FILE* file = fopen(VEHICLE_FILE, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    fseek(file, 0, SEEK_END); // Skip old entries
    char line[MAX_LINE_LENGTH];

    while(1){
       while(fgets(line, sizeof(line), file)) {
            line[strcspn(line, "\n")] = 0;
            char* vehicleNumber = strtok(line, ":");
            char* road = strtok(NULL, ":");

            if (vehicleNumber && road) {
                Vehicle v;
                strcpy(v.vehicleNumber, vehicleNumber);
                strcpy(v.road, road);

                if (strcmp(road, "A") == 0) { 
                    SDL_LockMutex(mutexA); 
                    enqueue(&queueA, v); 
                    SDL_UnlockMutex(mutexA); 
                }
                else if (strcmp(road, "B") == 0) { 
                    SDL_LockMutex(mutexB); 
                    enqueue(&queueB, v); 
                    SDL_UnlockMutex(mutexB); 
                }
                else if (strcmp(road, "C") == 0) { 
                    SDL_LockMutex(mutexC); 
                    enqueue(&queueC, v); 
                    SDL_UnlockMutex(mutexC); 
                }
                else if (strcmp(road, "D") == 0) { 
                    SDL_LockMutex(mutexD); 
                    enqueue(&queueD, v); 
                    SDL_UnlockMutex(mutexD); 
                }

                printf("Enqueued Vehicle %s on Road %s\n", vehicleNumber, road);
            }
        }

            clearerr(file); // Clear EOF flag
            SDL_Delay(500); // Wait before retrying
    }
        fclose(file);
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
    SDL_Thread* tReadFile = SDL_CreateThread(readAndParseFile, "FileThread", &sharedData);

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
        updateSprites();
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