#include "traffic_generator.h"
#include "simulator.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>   // For sleep()

#define FILENAME "vehicles.data"

// Generate a random vehicle number
void generateVehicleNumber(char* buffer) {
    buffer[0] = 'A' + rand() % 26;
    buffer[1] = 'A' + rand() % 26;
    buffer[2] = '0' + rand() % 10;
    buffer[3] = 'A' + rand() % 26;
    buffer[4] = 'A' + rand() % 26;
    buffer[5] = '0' + rand() % 10;
    buffer[6] = '0' + rand() % 10;
    buffer[7] = '0' + rand() % 10;
    buffer[8] = '\0';
}

// Generate a random lane
char generateLane() {
    char lanes[] = {'A', 'B', 'C', 'D'};
    return lanes[rand() % 4];
}


int start_traffic_generator(void* arg) {
    FILE* file = fopen(FILENAME, "a");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    srand(time(NULL)); // Initialize random seed

    while (1) {
        // Generate vehicle
        Vehicle v;
        generateVehicleNumber(v.vehicleNumber);
        v.road[0] = generateLane();   // single char
        v.road[1] = '\0';  

        // Write to file
        fprintf(file, "%s:%c\n", v.vehicleNumber, v.lane);
        fflush(file);

// Enqueue vehicle in the correct simulator queue
        if (v.road [0] == 'A') {
            SDL_LockMutex(mutexA);
            enqueue(&queueA, v);
            SDL_UnlockMutex(mutexA);
        } else if (v.road[0] == 'B') {
            SDL_LockMutex(mutexB);
            enqueue(&queueB, v);
            SDL_UnlockMutex(mutexB);
        } else if (v.road[0] == 'C') {
            SDL_LockMutex(mutexC);
            enqueue(&queueC, v);
            SDL_UnlockMutex(mutexC);
        } else if (v.road[0] == 'D') {
            SDL_LockMutex(mutexD);
            enqueue(&queueD, v);
            SDL_UnlockMutex(mutexD);
        }

        // Print status to console
        printf("Generated & Enqueued: %s:%c\n", v.vehicleNumber, v.road[0]);
        Sleep(1000); // Wait 1 second before generating next entry
    }

    fclose(file);
    return 0;
}
