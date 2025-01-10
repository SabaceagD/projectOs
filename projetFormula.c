#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#define NUM_CHAMPIONATS 24
#define NUM_CARS 20
#define NUM_LAPS 6
#define NUM_SECTORS 3
#define CARS_PER_SESSION 5
#define PIPE_READ 0
#define PIPE_WRITE 1

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
} Car;

const char* driver_names[] = {
    "Max Verstappen", "Sergio Pérez", "Lewis Hamilton", "George Russell",
    "Charles Leclerc", "Carlos Sainz Jr", "Lando Norris", "Oscar Piastri",
    "Fernando Alonso", "Lance Stroll", "Pierre Gasly", "Esteban Ocon",
    "Alexander Albon", "Logan Sargeant", "Yuki Tsunoda", "Daniel Ricciardo",
    "Valtteri Bottas", "Zhou Guanyu", "Kevin Magnussen", "Nico Hülkenberg"
};
const double circuits_length[]={5.412,6.174,5.278,6.003,5.41,3.337,4.675,4.361,4.318,5.891,4.381,7.004,4.259,5.793,5.063,5.807,5.418,5.513,4.304,4.309,6.12,5.281};
const char* circuits[] = {"Bahrain International Circuit","Bahrain","Jeddah Corniche Circuit","Albert Park Circuit","Baku City Circuit","Miami International Autodrome","Circuit de Monaco","Circuit de Barcelona-Catalunya","Circuit Gilles-Villeneuve","Red Bull Ring","Silverstone Circuit","Hungaroring","Circuit de Spa-Francorchamps","Circuit Zandvoort","Autodromo Nazionale di Monza","Marina Bay Street Circuit","Suzuka International Racing Course","Losail International Circuit","Circuit of the Americas","Autódromo Hermanos Rodríguez","Autódromo José Carlos Pace","Las Vegas Street Circuit","Yas Marina Circuit"};
const int car_num[]={1, 11, 44, 63, 16, 55, 4, 81, 14, 18, 10, 31, 23, 2, 22, 3, 77, 24, 20, 27};
int listeCars[20];

double generateRandomTime() {
    int seconds = rand() % 21 + 25; // Random seconds between 25 and 45
    int milliseconds = rand() % 1000;
    return seconds + milliseconds / 1000.0;
}

void save_to_file(int chmap, int chrace, int chqualif) {
    FILE *file = fopen("data.txt", "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        exit(1);
    }
    if (chrace > 0) chrace++;
    if (chqualif > 0) chqualif++;
    fprintf(file, "%d %d %d\n", chmap, chrace, chqualif);
    fclose(file);
}

void read_from_file(int *chmap, int *chrace, int *chqualif) {
    FILE *file = fopen("data.txt", "r");
    if (file == NULL) {
        perror("Error opening file for reading. Initializing values to 0.");
        *chmap = 0;
        *chrace = 0;
        *chqualif = 0;
        return;
    }
    if (fscanf(file, "%d %d %d", chmap, chrace, chqualif) != 3) {
        printf("Error reading data from file. Initializing values to 0.\n");
        *chmap = 0;
        *chrace = 0;
        *chqualif = 0;
    }
    fclose(file);
}

void delete_file() {
    if (remove("data.txt") == 0) {
        printf("Save file deleted.\n");
    }
}
void delete_points() {
    if (remove("data.txt") == 0) {
        printf("Save file deleted.\n");
    }
}

int ask() {
    int ans;
    printf("Do you want to continue? 0 = Yes, 1 = No: ");
    scanf("%d", &ans);
    return ans;
}
void sortCarsByBest(Car cars[], int size) {
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

void sortCarsByTotalTime(Car cars[], int num_cars) {
    for (int i = 0; i < num_cars - 1; i++) {
        for (int j = 0; j < num_cars - i - 1; j++) {
            if (cars[j].isOut == 0 && cars[j + 1].isOut == 0 && cars[j].total_time > cars[j + 1].total_time) {
                Car temp = cars[j];
                cars[j] = cars[j + 1];
                cars[j + 1] = temp;
            } else if (cars[j].isOut == 1 && cars[j + 1].isOut == 0) {
                Car temp = cars[j];
                cars[j] = cars[j + 1];
                cars[j + 1] = temp;
            }
        }
    }
}

void sortCarsByCurrent(Car cars[], int size) {
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
void initialize_cars(Car cars[]) {
    for (int i = 0; i < NUM_CARS; i++) {
        cars[i].car_number = car_num[i];
        cars[i].driver = driver_names[i];
        cars[i].sector1 = 0.0;
        cars[i].sector2 = 0.0;
        cars[i].sector3 = 0.0;
        cars[i].current_lap = 0.0;
        cars[i].best_lap_time = 9999;
        cars[i].total_time = 0.0;
        cars[i].points = 0;
        cars[i].isOut=0;
    }
}
void save_points(Car cars[], int size) {
    FILE *file = fopen("points.txt", "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        exit(1);
    }

    for (int i = 0; i < size; i++) {
        fprintf(file, "%d %d\n", cars[i].car_number, cars[i].points);
    }

    fclose(file);
    printf("Points saved successfully to points.txt.\n");
}

void read_points(Car cars[], int size) {
    FILE *file = fopen("points.txt", "r");
    if (file == NULL) {
        perror("Error opening file for reading. Initializing values to 0.");
        for (int i = 0; i < size; i++) {
            cars[i].points = 0;
        }
        return;
    }

    int car_number, points;
    while (fscanf(file, "%d %d", &car_number, &points) == 2) {
        for (int i = 0; i < size; i++) {
            if (cars[i].car_number == car_number) {
                cars[i].points = points;
                break;
            }
        }
    }

    fclose(file);
    printf("Points loaded successfully from points.txt.\n");
}
void seance_essai(Car cars[],int session) {
    printf("\nSimulating %d session:\n", session);
    for (int lap = 0; lap < 60; lap++) {
        printf("\nLap %d Results:\n", lap + 1);
        int pipefds[2];
        pipe(pipefds);

        for (int i = 0; i < NUM_CARS; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));
                cars[i].sector1 = generateRandomTime();
                cars[i].sector2 = generateRandomTime();
                cars[i].sector3 = generateRandomTime();
                cars[i].current_lap = cars[i].sector1 + cars[i].sector2 + cars[i].sector3;
                cars[i].total_time += cars[i].current_lap;
                if (cars[i].current_lap < cars[i].best_lap_time) {
                    cars[i].best_lap_time = cars[i].current_lap;
                }
                write(pipefds[PIPE_WRITE], &cars[i], sizeof(Car));
                close(pipefds[PIPE_WRITE]);
                exit(0);
            }
        }
        for (int i = 0; i < NUM_CARS; i++) {
            wait(NULL);
        }

        close(pipefds[PIPE_WRITE]);

        for (int i = 0; i < NUM_CARS; i++) {
            read(pipefds[PIPE_READ], &cars[i], sizeof(Car));
        }

        close(pipefds[PIPE_READ]);

        sortCarsByCurrent(cars, NUM_CARS);
        for (int i = 0; i < NUM_CARS; i++) {
            double timeDifference = (i == 0) ? 0.0 : cars[i].current_lap - cars[i-1].current_lap;
            printf("Car %d (%s): Lap Time = %.3f, Best Lap = %.3f, Total Time = %.3f",
                   cars[i].car_number, cars[i].driver,
                   cars[i].current_lap, cars[i].best_lap_time, cars[i].total_time);
            if (i > 0) {
                printf("  (+%.3f)", timeDifference);
            }
            printf("\n");
        }
        sortCarsByBest(cars, NUM_CARS);
        printf("Sceance-Essai P%d Classement Final",session);
        for(int i = 0; i < NUM_CARS; i++) {
            printf("Car %d (%s): Best Lap = %.3f, Total Time = %.3f",
                   cars[i].car_number, cars[i].driver,cars[i].best_lap_time, cars[i].total_time);
        }
        sleep(1);
    }
}
void coruse_dimanche(){
    Car cars[NUM_CARS];
    initialize_cars(cars);
    int choice;
    printf("Please type 0 to start fresh or 1 to pick up where you left: ");
    scanf("%d", &choice);
    if(choice==0){delete_points();}
    else{read_points(cars, NUM_CARS);}
    course(cars);
    save_points(cars, NUM_CARS);
}
void sprint(){
    Car cars[NUM_CARS];
    initialize_cars(cars);
    int choice;
    printf("Please type 0 to start fresh or 1 to pick up where you left: ");
    scanf("%d", &choice);
    if(choice==0){delete_points();}
    else{read_points(cars, NUM_CARS);}
    sprint(cars);
    save_points(cars, NUM_CARS);
}
void runSession(Car cars[], int num_cars, int num_laps, int session) {
    int pipes[num_cars][2];

    for (int i = 0; i < num_cars; i++) {
        if (cars[i].isOut == 1) continue;
        pipe(pipes[i]);

        if (fork() == 0) {
            close(pipes[i][0]);
            srand(time(NULL) ^ (getpid() << 16)); // Unique random seed

            double session_time = 0.0;

            for (int lap = 0; lap < num_laps; lap++) {
                cars[i].sector1 = generateRandomTime();
                cars[i].sector2 = generateRandomTime();
                cars[i].sector3 = generateRandomTime();
                cars[i].current_lap = cars[i].sector1 + cars[i].sector2 + cars[i].sector3;
                session_time += cars[i].current_lap;

                if (cars[i].current_lap < cars[i].best_lap_time) {
                    cars[i].best_lap_time = cars[i].current_lap;
                }
            }

            cars[i].total_time += session_time;
            write(pipes[i][1], &cars[i], sizeof(Car));
            close(pipes[i][1]);
            exit(0);
        }
        close(pipes[i][1]); 
    }
    for (int i = 0; i < num_cars; i++) {
        if (cars[i].isOut == 1) continue;
        read(pipes[i][0], &cars[i], sizeof(Car));
        close(pipes[i][0]);
    }
    sortCarsByTotalTime(cars, num_cars);
    if (session < NUM_SESSIONS) {
        int eliminated = 0;
        for (int i = num_cars - 1; i >= 0 && eliminated < CARS_PER_SESSION; i--) {
            if (cars[i].isOut == 0) {
                cars[i].isOut = 1;
                eliminated++;
            }
        }
    }
    printf("\nQ%d Results:\n", session);
    for (int i = 0; i < num_cars; i++) {
        printf("Car %d (%s): Total Time = %.3f, Best Lap = %.3f %s\n",
               cars[i].car_number, cars[i].driver, cars[i].total_time, cars[i].best_lap_time,
               cars[i].isOut ? "(Eliminated)" : "");
    }
}

void qualifications(int a,int b,int c) {
    Car cars[NUM_CARS];
    srand(time(NULL));
    initialize_cars();
    printf("\nSimulating Qualifications:\n");

    runSession(cars, NUM_CARS, a, 1); // Q1: 22 ou 12 laps
    runSession(cars, NUM_CARS, b, 2); // Q2: 18 ou 8 laps
    runSession(cars, NUM_CARS, c, 3); // Q3: 12 ou 6 laps
    sortCarsByTotalTime(cars, NUM_CARS);
    FILE *file = fopen("qualifications_results.txt", "w");
    if (file == NULL) {
        perror("Error opening file for writing");
        exit(1);
    }

    fprintf(file, "Final Standings:\n");
    for (int i = 0; i < NUM_CARS; i++) {
        fprintf(file, "Car %d (%s): Total Time = %.3f, Best Lap = %.3f %s\n",
                cars[i].car_number, cars[i].driver, cars[i].total_time, cars[i].best_lap_time,
                cars[i].isOut ? "(Eliminated)" : "");
    }

    fclose(file);
    printf("\nResults saved to qualifications_results.txt\n");
}
void sprint(Car cars[]) {
    printf("\nSimulating %s session:\n", session_name);

    for (int lap = 0; lap < 25; lap++) {
        printf("\nLap %d Results:\n", lap + 1);
        int pipefds[2];
        pipe(pipefds);

        for (int i = 0; i < NUM_CARS; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));
                cars[i].sector1 = generateRandomTime();
                cars[i].sector2 = generateRandomTime();
                cars[i].sector3 = generateRandomTime();
                cars[i].current_lap = cars[i].sector1 + cars[i].sector2 + cars[i].sector3;
                cars[i].total_time += cars[i].current_lap;
                if (cars[i].current_lap < cars[i].best_lap_time) {
                    cars[i].best_lap_time = cars[i].current_lap;
                }
                write(pipefds[PIPE_WRITE], &cars[i], sizeof(Car));
                close(pipefds[PIPE_WRITE]);
                exit(0);
            }
        }
        for (int i = 0; i < NUM_CARS; i++) {
            wait(NULL);
        }

        close(pipefds[PIPE_WRITE]);
        for (int i = 0; i < NUM_CARS; i++) {
            read(pipefds[PIPE_READ], &cars[i], sizeof(Car));
        }
        close(pipefds[PIPE_READ]);
        sortCarsByCurrent(cars, NUM_CARS);
        for (int i = 0; i < NUM_CARS; i++) {
            double timeDifference = (i == 0) ? 0.0 : cars[i].current_lap - cars[i-1].current_lap;
            printf("Car %d (%s): Lap Time = %.3f, Best Lap = %.3f, Total Time = %.3f",
                   cars[i].car_number, cars[i].driver,
                   cars[i].current_lap, cars[i].best_lap_time, cars[i].total_time);
            if (i > 0) {
                printf("  (+%.3f)", timeDifference);
            }
            printf("\n");
        }
        sleep(1);
    }
    int pointDistribution[] = {25, 20, 15, 10, 8, 6, 5, 3, 2, 1};
    for (int i = 0; i < 10 && i < NUM_CARS; i++) {
        cars[i].points += pointDistribution[i];
        printf("Car %d (%s): Best Lap = %.3f, Points = %d\n",
               cars[i].car_number, cars[i].driver, cars[i].best_lap_time, cars[i].points);
    }
}
void course(Car cars[]) {
    printf("\nSimulating session:\n");

    for (int lap = 0; lap < 55; lap++) {
        printf("\nLap %d Results:\n", lap + 1);
        int pipefds[2];
        pipe(pipefds);

        for (int i = 0; i < NUM_CARS; i++) {
            pid_t pid = fork();
            if (pid == 0) {
                srand(time(NULL) ^ (getpid() << 16));
                cars[i].sector1 = generateRandomTime();
                cars[i].sector2 = generateRandomTime();
                cars[i].sector3 = generateRandomTime();
                cars[i].current_lap = cars[i].sector1 + cars[i].sector2 + cars[i].sector3;
                cars[i].total_time += cars[i].current_lap;
                if (cars[i].current_lap < cars[i].best_lap_time) {
                    cars[i].best_lap_time = cars[i].current_lap;
                }
                write(pipefds[PIPE_WRITE], &cars[i], sizeof(Car));
                close(pipefds[PIPE_WRITE]);
                exit(0);
            }
        }
        for (int i = 0; i < NUM_CARS; i++) {
            wait(NULL);
        }

        close(pipefds[PIPE_WRITE]);
        for (int i = 0; i < NUM_CARS; i++) {
            read(pipefds[PIPE_READ], &cars[i], sizeof(Car));
        }
        close(pipefds[PIPE_READ]);
        sortCarsByCurrent(cars, NUM_CARS);
        for (int i = 0; i < NUM_CARS; i++) {
            double timeDifference = (i == 0) ? 0.0 : cars[i].current_lap - cars[i-1].current_lap;
            printf("Car %d (%s): Lap Time = %.3f, Best Lap = %.3f, Total Time = %.3f",
                   cars[i].car_number, cars[i].driver,
                   cars[i].current_lap, cars[i].best_lap_time, cars[i].total_time);
            if (i > 0) {
                printf("  (+%.3f)", timeDifference);
            }
            printf("\n");
        }
        sleep(1);
    }
    int pointDistribution[] = {25, 20, 15, 10, 8, 6, 5, 3, 2, 1};
    for (int i = 0; i < 10 && i < NUM_CARS; i++) {
        cars[i].points += pointDistribution[i];
        printf("Car %d (%s): Best Lap = %.3f, Points = %d\n",
               cars[i].car_number, cars[i].driver, cars[i].best_lap_time, cars[i].points);
    }
}
void championship(int a, int b, int c) {
    int stop_program = 0;
    for (int i = a; i < NUM_CHAMPIONATS && !stop_program; i++) {
        if (i % 2 == 0) { // Type 1 Championship
            for (int j = b; j < 5; j++) {
                if (j == 0){ seance_essai(1);}
                else if (j == 1){ seance_essai(2);}
                else if (j == 2){seance_essai(3);}
                else if (j == 3){qualifications(22,18,12);}
                else if (j == 4){coruse_dimanche()}

                save_to_file(i, j, 0);
                if (ask() == 1) {
                    stop_program = 1;
                    break;
                }
            }
            b = 0;
        } else { // Type 2 Championship
            for (int j = b; j < 5; j++) {
                if (j == 0) seance_essai(1,);
                else if (j == 1) qualifications(12,8,6);
                else if (j == 2) sprint();
                else if (j == 3) qualifications(22,18,12);
                else if (j == 4) course_dimanche();

                save_to_file(i, j, 0);
                if (ask() == 1) {
                    stop_program = 1;
                    break;
                }
            }
            b = 0;
        }
    }
}
int main() {
    int answer,chmap, chrace, chqualif;
    printf("Welcome to the 2024 Grand Prix Around the World!\n");
    printf("Please type 0 to start fresh or 1 to pick up where you left: ");
    scanf("%d", &answer);

    if (answer == 0) {
        delete_file();
        championship(0, 0, 0);
    } else if (answer == 1) {
        read_from_file(&chmap, &chrace, &chqualif);
        championship(chmap, chrace, chqualif);
    }

    return 0;
}
