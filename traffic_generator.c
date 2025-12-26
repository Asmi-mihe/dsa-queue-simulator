#include "traffic_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>   // For sleep()

#define FILENAME "vehicles.data"

// Vehicle structure
typedef struct Vehicle {
    char number[9];   // Vehicle number
    char lane;        // Lane (A, B, C, D)
    struct Vehicle* next;
} Vehicle;

// Queue pointers
Vehicle* front = NULL;
Vehicle* rear = NULL;

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

// Enqueue a vehicle
void enqueue(Vehicle v) {
    Vehicle* newNode = (Vehicle*)malloc(sizeof(Vehicle));
    strcpy(newNode->number, v.number);
    newNode->lane = v.lane;
    newNode->next = NULL;

    if (rear == NULL) {
        front = rear = newNode;
    } else {
        rear->next = newNode;
        rear = newNode;
    }
}

// Dequeue a vehicle
Vehicle dequeue() {
    Vehicle empty = {"", ' '};
    if (front == NULL) {
        return empty;
    }
    Vehicle v = *front;
    Vehicle* temp = front;
    front = front->next;
    if (front == NULL) rear = NULL;
    free(temp);
    return v;
}

// Display the queue
void displayQueue() {
    Vehicle* temp = front;
    printf("Current Queue: ");
    while (temp != NULL) {
        printf("[%s:%c] -> ", temp->number, temp->lane);
        temp = temp->next;
    }
    printf("NULL\n");
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
        generateVehicleNumber(v.number);
        v.lane = generateLane();

        // Write to file
        fprintf(file, "%s:%c\n", v.number, v.lane);
        fflush(file);

        // Enqueue vehicle
        enqueue(v);

        // Print status
        printf("Generated & Enqueued: %s:%c\n", v.number, v.lane);
        displayQueue();

        Sleep(1000); // Wait 1 second before generating next entry
    }

    fclose(file);
    return 0;
}
