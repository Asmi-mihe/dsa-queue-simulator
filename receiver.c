#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define FILENAME "vehicles.data"
#define MAX_LINE_LENGTH 100

int main() {
    FILE *file;
    char line[MAX_LINE_LENGTH];

    printf("Receiver is running... Watching %s for new vehicles.\n", FILENAME);
    fflush(stdout);
    
    // Open file in read mode
    file = fopen(FILENAME, "r");
    if (!file) {
        perror("Error opening file");
        return 1;
    }

    // Move to the end of the file initially
    fseek(file, 0, SEEK_END);

    while (1) {
        // Try to read a new line
        if (fgets(line, sizeof(line), file)) {
            // Remove newline
            line[strcspn(line, "\n")] = 0;

            // Parse vehicle number and lane
            char *vehicleNumber = strtok(line, ":");
            char *lane = strtok(NULL, ":");

            if (vehicleNumber && lane) {
                printf("Received Vehicle %s on Lane %s\n", vehicleNumber, lane);
            }
        } else {
            // No new line yet, wait and retry
            clearerr(file);   // Clear EOF flag
            Sleep(1000); // Sleep for 1 second
        }
    }

    fclose(file);
    return 0;
}
