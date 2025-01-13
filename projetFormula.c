#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define NUM_CARS 20
#define NUM_GP 22
#define NUM_RACES_PER_GP 3

typedef struct {
    int car_number;
    double sector1;
    double sector2;
    double sector3;
    double current_lap;
    double best_lap_time;
    double total_time;
    const char *driver;
    int points;
    int isOut;
    int isPit;
} Car;

typedef struct {
    Car cars[NUM_CARS];
} SharedRaceData;

const double circuits_length[] = {
    5.412, 6.174, 5.278, 6.003, 5.41, 3.337, 4.675, 4.361, 4.318, 5.891,
    4.381, 7.004, 4.259, 5.793, 5.063, 5.807, 5.418, 5.513, 4.304, 4.309,
    6.12, 5.281
};

const char* circuits[] = {
    "Bahrain International Circuit", "Bahrain", "Jeddah Corniche Circuit",
    "Albert Park Circuit", "Baku City Circuit", "Miami International Autodrome",
    "Circuit de Monaco", "Circuit de Barcelona-Catalunya", "Circuit Gilles-Villeneuve",
    "Red Bull Ring", "Silverstone Circuit", "Hungaroring", "Circuit de Spa-Francorchamps",
    "Circuit Zandvoort", "Autodromo Nazionale di Monza", "Marina Bay Street Circuit",
    "Suzuka International Racing Course", "Losail International Circuit",
    "Circuit of the Americas", "Autódromo Hermanos Rodríguez",
    "Autódromo José Carlos Pace", "Las Vegas Street Circuit", "Yas Marina Circuit"
};

const char* driver_names[] = {
    "Max Verstappen", "Sergio Pérez", "Lewis Hamilton", "George Russell",
    "Charles Leclerc", "Carlos Sainz Jr", "Lando Norris", "Oscar Piastri",
    "Fernando Alonso", "Lance Stroll", "Pierre Gasly", "Esteban Ocon",
    "Alexander Albon", "Logan Sargeant", "Yuki Tsunoda", "Daniel Ricciardo",
    "Valtteri Bottas", "Zhou Guanyu", "Kevin Magnussen", "Nico Hülkenberg"
};

const int car_num[] = {
    1, 11, 44, 63, 16, 55, 4, 81, 14, 18,
    10, 31, 23, 2, 22, 3, 77, 24, 20, 27
};

const int point_distribution[] = {25, 20, 15, 10, 8, 6, 5, 3, 2, 1};
const int point_distribution_sprint[]={8,7,6,5,4,3,2,1};

double generateRandomTime() {
    int seconds = rand() % 21 + 25; // Random seconds between 25 and 45
    int milliseconds = rand() % 1000;
    return seconds + milliseconds / 1000.0;
}

void initialize_cars(Car cars[]) {
    for (int i = 0; i < NUM_CARS; i++) {
        cars[i].car_number = car_num[i];
        cars[i].sector1 = 0;
        cars[i].sector2 = 0;
        cars[i].sector3 = 0;
        cars[i].current_lap = 0;
        cars[i].best_lap_time = 999.0; // Arbitrarily high for comparisons
        cars[i].total_time = 0;
        cars[i].driver = driver_names[i];
        cars[i].points = 0;
        cars[i].isOut = 0;
        cars[i].isPit = 0;
    }
}

void reset_cars_total_time(Car cars[]) {
    for (int i = 0; i < NUM_CARS; i++) {
        cars[i].total_time = 0;
        cars[i].isOut=0;
    }
}

void save_progress(int current_gp, int current_race, Car cars[]) {
    FILE *file = fopen("progress.txt", "w");
    if (file == NULL) {
        perror("Error saving progress to file");
        return;
    }

    fprintf(file, "%d %d\n", current_gp, current_race);
    for (int i = 0; i < NUM_CARS; i++) {
        fprintf(file, "%d %d\n", cars[i].car_number, cars[i].points);
    }
    fclose(file);
    printf("Progress saved successfully.\n");
}

int load_progress(int *current_gp, int *current_race, Car cars[]) {
    FILE *file = fopen("progress.txt", "r");
    if (file == NULL) {
        perror("No progress file found. Starting fresh.");
        return 0;
    }

    if (fscanf(file, "%d %d", current_gp, current_race) != 2) {
        printf("Error reading progress. Starting fresh.\n");
        fclose(file);
        return 0;
    }

    int car_number, points;
    while (fscanf(file, "%d %d", &car_number, &points) == 2) {
        for (int i = 0; i < NUM_CARS; i++) {
            if (cars[i].car_number == car_number) {
                cars[i].points = points;
                break;
            }
        }
    }
    fclose(file);
    printf("Progress loaded successfully. Resuming from GP %d, Race %d.\n", *current_gp, *current_race);
    return 1;
}

void sortCarsByTotalTime(Car cars[], int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (cars[j].total_time > cars[j + 1].total_time) {
                Car temp = cars[j];
                cars[j] = cars[j + 1];
                cars[j + 1] = temp;
            }
        }
    }
}
void sortCarsByBestTime(Car cars[], int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (cars[j].best_lap_time > cars[j + 1].best_lap_time) {
                Car temp = cars[j];
                cars[j] = cars[j + 1];
                cars[j + 1] = temp;
            }
        }
    }
}
void sortCarsByCurrentLapTime(Car cars[], int size) {
    for (int i = 0; i < size - 1; i++) {
        for (int j = 0; j < size - i - 1; j++) {
            if (cars[j].current_lap > cars[j + 1].current_lap) {
                Car temp = cars[j];
                cars[j] = cars[j + 1];
                cars[j + 1] = temp;
            }
        }
    }
}
void display_lap_standings(Car cars[], int size) {
    printf("\nLap Standings (Sorted by Current Lap Time):\n");
    for (int i = 0; i < size; i++) {
        double time_difference = cars[i].current_lap - cars[i-1].current_lap;
        printf("Car %d (%s): Lap Time = %.3f, Time Difference = %.3f\n",
               cars[i].car_number,
               cars[i].driver,
               cars[i].current_lap,
               (i == 0) ? 0.0 : time_difference);
    }
}
int prompt_continue() {
    int choice;
    printf("\nDo you want to continue to the next race? (1 = Yes, 2 = No): ");
    scanf("%d", &choice);
    return choice;
}

void race_session(SharedRaceData *shared_data, int num_laps, int sem_id) {
    struct sembuf sem_op;

    // Track best times and the drivers who achieved them for each sector
    double best_sector1_time = 999;
    double best_sector2_time = 999;
    double best_sector3_time = 999;

    int best_sector1_driver = -1;
    int best_sector2_driver = -1;
    int best_sector3_driver = -1;

    for (int lap = 0; lap < num_laps; lap++) {
        printf("\nLap %d Results:\n", lap + 1);

        for (int i = 0; i < NUM_CARS; i++) {
            if (shared_data->cars[i].isOut) continue;

            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));

                shared_data->cars[i].sector1 = generateRandomTime();
                shared_data->cars[i].sector2 = generateRandomTime();
                shared_data->cars[i].sector3 = generateRandomTime();
                shared_data->cars[i].current_lap = shared_data->cars[i].sector1 +
                                                    shared_data->cars[i].sector2 +
                                                    shared_data->cars[i].sector3;

                if (shared_data->cars[i].current_lap < shared_data->cars[i].best_lap_time) {
                    shared_data->cars[i].best_lap_time = shared_data->cars[i].current_lap;
                }
                shared_data->cars[i].total_time += shared_data->cars[i].current_lap;

                // Update shared memory safely
                sem_op.sem_num = 0;
                sem_op.sem_op = -1;
                sem_op.sem_flg = 0;
                semop(sem_id, &sem_op, 1);

                sem_op.sem_op = 1;
                semop(sem_id, &sem_op, 1);

                exit(0);
            }
        }

        for (int i = 0; i < NUM_CARS; i++) {
            if (!shared_data->cars[i].isOut) wait(NULL);
        }

        // Update best sector times
        for (int i = 0; i < NUM_CARS; i++) {
            if (shared_data->cars[i].sector1 < best_sector1_time) {
                best_sector1_time = shared_data->cars[i].sector1;
                best_sector1_driver = i;
            }
            if (shared_data->cars[i].sector2 < best_sector2_time) {
                best_sector2_time = shared_data->cars[i].sector2;
                best_sector2_driver = i;
            }
            if (shared_data->cars[i].sector3 < best_sector3_time) {
                best_sector3_time = shared_data->cars[i].sector3;
                best_sector3_driver = i;
            }
        }

        // Sort cars by current lap time and display standings
        sortCarsByCurrentLapTime(shared_data->cars, NUM_CARS);
        display_lap_standings(shared_data->cars, NUM_CARS);
    }

    // Final sort by total time
    sortCarsByTotalTime(shared_data->cars, NUM_CARS);

    // Allocate points for the race
    for (int i = 0; i < 10 && i < NUM_CARS; i++) {
        shared_data->cars[i].points += point_distribution[i];
    }

    // Award 1 bonus point for best sector times
    if (best_sector1_driver != -1) shared_data->cars[best_sector1_driver].points += 1;
    if (best_sector2_driver != -1) shared_data->cars[best_sector2_driver].points += 1;
    if (best_sector3_driver != -1) shared_data->cars[best_sector3_driver].points += 1;

    // Print final results
    printf("\nFinal Results and Points:\n");
    for (int i = 0; i < NUM_CARS; i++) {
        printf("Car %d (%s): Total Time = %.3f, Best Lap = %.3f, Points = %d\n",
               shared_data->cars[i].car_number,
               shared_data->cars[i].driver,
               shared_data->cars[i].total_time,
               shared_data->cars[i].best_lap_time,
               shared_data->cars[i].points);
    }

    // Display best sector drivers
    printf("\nBonus Points Awarded for Best Sector Times:\n");
    if (best_sector1_driver != -1) {
        printf("Best Sector 1: Car %d (%s) with %.3f seconds\n",
               shared_data->cars[best_sector1_driver].car_number,
               shared_data->cars[best_sector1_driver].driver,
               best_sector1_time);
    }
    if (best_sector2_driver != -1) {
        printf("Best Sector 2: Car %d (%s) with %.3f seconds\n",
               shared_data->cars[best_sector2_driver].car_number,
               shared_data->cars[best_sector2_driver].driver,
               best_sector2_time);
    }
    if (best_sector3_driver != -1) {
        printf("Best Sector 3: Car %d (%s) with %.3f seconds\n",
               shared_data->cars[best_sector3_driver].car_number,
               shared_data->cars[best_sector3_driver].driver,
               best_sector3_time);
    }
}

void sprint_session(SharedRaceData *shared_data, int num_laps, int sem_id) {
    struct sembuf sem_op;

    for (int lap = 0; lap < num_laps; lap++) {
        printf("\nLap %d Results:\n", lap + 1);

        for (int i = 0; i < NUM_CARS; i++) {
            if (shared_data->cars[i].isOut) continue;

            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));

                shared_data->cars[i].sector1 = generateRandomTime();
                shared_data->cars[i].sector2 = generateRandomTime();
                shared_data->cars[i].sector3 = generateRandomTime();
                shared_data->cars[i].current_lap = shared_data->cars[i].sector1 +
                                                    shared_data->cars[i].sector2 +
                                                    shared_data->cars[i].sector3;

                if (shared_data->cars[i].current_lap < shared_data->cars[i].best_lap_time) {
                    shared_data->cars[i].best_lap_time = shared_data->cars[i].current_lap;
                }
                shared_data->cars[i].total_time += shared_data->cars[i].current_lap;

                // Update shared memory safely
                sem_op.sem_num = 0;
                sem_op.sem_op = -1;
                sem_op.sem_flg = 0;
                semop(sem_id, &sem_op, 1);

                sem_op.sem_op = 1;
                semop(sem_id, &sem_op, 1);

                exit(0);
            }
        }

        for (int i = 0; i < NUM_CARS; i++) {
            if (!shared_data->cars[i].isOut) wait(NULL);
        }

        // Sort cars by current lap time and display standings
        sortCarsByCurrentLapTime(shared_data->cars, NUM_CARS);
        display_lap_standings(shared_data->cars, NUM_CARS);
    }

    // Final sort by total time
    sortCarsByTotalTime(shared_data->cars, NUM_CARS);

    // Allocate points
    for (int i = 0; i < 10 && i < NUM_CARS; i++) {
        shared_data->cars[i].points += point_distribution[i];
    }

    printf("\nFinal Results and Points:\n");
    for (int i = 0; i < NUM_CARS; i++) {
        printf("Car %d (%s): Total Time = %.3f, Best Lap = %.3f, Points = %d\n",
               shared_data->cars[i].car_number,
               shared_data->cars[i].driver,
               shared_data->cars[i].total_time,
               shared_data->cars[i].best_lap_time,
               shared_data->cars[i].points);
    }
}
void qualifying_session(SharedRaceData *shared_data, int q1, int q2, int q3, int sem_id) {
    struct sembuf sem_op;
    int num_laps[3] = {q1, q2, q3};

    for (int session = 0; session < 3; session++) {
        printf("\n=== Starting Q%d (%d laps) ===\n", session + 1, num_laps[session]);

        for (int lap = 0; lap < num_laps[session]; lap++) {
            printf("\nLap %d Results:\n", lap + 1);

            for (int i = 0; i < NUM_CARS; i++) {
                if (shared_data->cars[i].isOut) continue; // Skip eliminated cars

                pid_t pid = fork();
                if (pid == 0) {
                    srand(time(NULL) ^ (getpid() << 16));

                    // Generate sector times
                    shared_data->cars[i].sector1 = generateRandomTime();
                    shared_data->cars[i].sector2 = generateRandomTime();
                    shared_data->cars[i].sector3 = generateRandomTime();
                    shared_data->cars[i].current_lap = shared_data->cars[i].sector1 +
                                                        shared_data->cars[i].sector2 +
                                                        shared_data->cars[i].sector3;

                    if (shared_data->cars[i].current_lap < shared_data->cars[i].best_lap_time) {
                        shared_data->cars[i].best_lap_time = shared_data->cars[i].current_lap;
                    }
                    shared_data->cars[i].total_time += shared_data->cars[i].current_lap;

                    // Synchronize updates with semaphore
                    sem_op.sem_num = 0;
                    sem_op.sem_op = -1;
                    sem_op.sem_flg = 0;
                    semop(sem_id, &sem_op, 1);

                    sem_op.sem_op = 1;
                    semop(sem_id, &sem_op, 1);

                    exit(0);
                }
            }

            for (int i = 0; i < NUM_CARS; i++) {
                if (!shared_data->cars[i].isOut) wait(NULL);
            }

            // Sort by current lap time and display lap standings
            sortCarsByCurrentLapTime(shared_data->cars, NUM_CARS);
            display_lap_standings(shared_data->cars, NUM_CARS);
        }

        // Sort cars by total time after the session
        sortCarsByTotalTime(shared_data->cars, NUM_CARS);

        // Eliminate the slowest 5 cars in Q1 and Q2
        if (session < 2) {
            int eliminated = 0;
            for (int i = NUM_CARS - 1; i >= 0 && eliminated < 5; i--) {
                if (!shared_data->cars[i].isOut) {
                    shared_data->cars[i].isOut = 1; // Mark as eliminated
                    eliminated++;
                }
            }
            printf("\n=== Q%d Eliminations ===\n", session + 1);
            for (int i = NUM_CARS - eliminated; i < NUM_CARS; i++) {
                printf("Eliminated: Car %d (%s) %.3f \n", shared_data->cars[i].car_number,
                       shared_data->cars[i].driver,shared_data->cars[i].total_time);
            }
        }

        // Display results after the session
        printf("\n=== Q%d Results ===\n", session + 1);
        for (int i = 0; i < NUM_CARS; i++) {
            printf("Car %d (%s): Total Time = %.3f, Best Lap = %.3f %s\n",
                   shared_data->cars[i].car_number, shared_data->cars[i].driver,
                   shared_data->cars[i].total_time, shared_data->cars[i].best_lap_time,
                   shared_data->cars[i].isOut ? "(Eliminated)" : "");
        }
    }

    // Save final standings for the starting grid
    FILE *file = fopen("qualifications_results.txt", "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        exit(1);
    }

    fprintf(file, "Final Starting Grid:\n");
    for (int i = 0; i < NUM_CARS; i++) {
        if (!shared_data->cars[i].isOut) { // Only include qualified cars
            fprintf(file, "Car %d (%s): Best Lap = %.3f\n",
                    shared_data->cars[i].car_number,
                    shared_data->cars[i].driver,
                    shared_data->cars[i].best_lap_time);
        }
    }
    fclose(file);
    printf("\nFinal Starting Grid saved to qualifications_results.txt\n");
}

void training_session(SharedRaceData *shared_data, int sem_id) {
    struct sembuf sem_op;

    for (int lap = 0; lap < 60; lap++) {
        printf("\nLap %d Results:\n", lap + 1);
        for (int i = 0; i < NUM_CARS; i++) {
            if (shared_data->cars[i].isOut) continue;

            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));

                shared_data->cars[i].sector1 = generateRandomTime();
                shared_data->cars[i].sector2 = generateRandomTime();
                shared_data->cars[i].sector3 = generateRandomTime();
                shared_data->cars[i].current_lap = shared_data->cars[i].sector1 +
                                                    shared_data->cars[i].sector2 +
                                                    shared_data->cars[i].sector3;

                if (shared_data->cars[i].current_lap < shared_data->cars[i].best_lap_time) {
                    shared_data->cars[i].best_lap_time = shared_data->cars[i].current_lap;
                }
                shared_data->cars[i].total_time += shared_data->cars[i].current_lap;
                sem_op.sem_num = 0;
                sem_op.sem_op = -1;
                sem_op.sem_flg = 0;
                semop(sem_id, &sem_op, 1);
                sortCarsByBestTime(shared_data->cars, NUM_CARS);
                printf("Car %d: Lap = %.3f, Total = %.3f, Best = %.3f\n",
                       shared_data->cars[i].car_number,
                       shared_data->cars[i].current_lap,
                       shared_data->cars[i].total_time,
                       shared_data->cars[i].best_lap_time);

                sem_op.sem_op = 1;
                semop(sem_id, &sem_op, 1);

                exit(0);
            }
        }

        for (int i = 0; i < NUM_CARS; i++) {
            if (!shared_data->cars[i].isOut) wait(NULL);
        }
    }
    sortCarsByBestTime(shared_data->cars, NUM_CARS);
    printf("\nFinal Results and Points:\n");
    for (int i = 0; i < NUM_CARS; i++) {
        printf("Car %d (%s): Total Time = %.3f, Best Lap = %.3f, Points = %d\n",
               shared_data->cars[i].car_number,
               shared_data->cars[i].driver,
               shared_data->cars[i].total_time,
               shared_data->cars[i].best_lap_time,
               shared_data->cars[i].points);
    }
}
void simulate_championship(SharedRaceData *shared_data, int sem_id, int start_gp, int start_race) {
    for (int gp = start_gp; gp < NUM_GP; gp++) {
        int num_laps = (int)(300 / circuits_length[gp]);
        int num_laps_sprint = (int)(100 / circuits_length[gp]);
        printf("\n=== %s ===\n", circuits[gp]);

        for (int race = (gp == start_gp ? start_race : 0); race < NUM_RACES_PER_GP; race++) {
            printf("\n--- Race %d at %s (%d laps) ---\n", race + 1, circuits[gp], num_laps);

            if(race==0){reset_cars_total_time(shared_data->cars);
            race_session(shared_data, num_laps, sem_id);
            save_progress(gp, race + 1, shared_data->cars);}
            if(race==1){reset_cars_total_time(shared_data->cars);
            training_session(shared_data, sem_id);
            save_progress(gp, race + 1, shared_data->cars);}
            if(race==2){reset_cars_total_time(shared_data->cars);
            qualifying_session(shared_data,22,18,12, sem_id);
            save_progress(gp, race + 1, shared_data->cars);
            }

            if (prompt_continue() == 2) {
                printf("\nExiting the championship.\n");
                return;
            }
        }
    }
}

int main() {
    // Initialize shared memory and semaphores
    key_t key = ftok("shmfile", 65);
    int shmid = shmget(key, sizeof(SharedRaceData), 0666 | IPC_CREAT);
    SharedRaceData *shared_data = (SharedRaceData *)shmat(shmid, NULL, 0);

    int sem_id = semget(IPC_PRIVATE, 1, 0666 | IPC_CREAT);
    semctl(sem_id, 0, SETVAL, 1);

    // Initialize cars
    initialize_cars(shared_data->cars);

    // Prompt user to load progress or start fresh
    int choice, start_gp = 0, start_race = 0;
    printf("Do you want to (1) Start fresh or (2) Resume progress? ");
    scanf("%d", &choice);
    if (choice == 2) {
        load_progress(&start_gp, &start_race, shared_data->cars);
    }

    // Simulate the championship
    simulate_championship(shared_data, sem_id, start_gp, start_race);

    // Cleanup
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(sem_id, 0, IPC_RMID);

    return 0;
}
