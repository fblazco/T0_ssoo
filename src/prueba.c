#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

/*************STRUCTS***********/
struct proceso {
    int pid;
    char nombre[256];
    time_t tiempo_inicio;
    time_t tiempo_fin;
    int exit_code;
    int signal_value;
    int activo;
    int term_enviado;   // para time_max: 1 si ya mandamos SIGTERM
    time_t term_ts;     // cuándo mandamos SIGTERM
};
/******* GLOBALES ******/
struct proceso procesos[10];
int total_procesos = 0;
int shutdown_en_progreso = 0;
time_t shutdown_inicio;
int time_max = -1; // límite de segundos, opcional argv[1]

/*************MAIN***********/
int main(int argc, char const *argv[]){
    set_buffer(); // No borrar
    
    if (argc >= 2) {
        time_max = atoi(argv[1]); // si no entregan, queda -1 (sin límite)
    }

    while (1) {
        // revisar si shutdown pasó los 10s
        if (shutdown_en_progreso) {
            time_t ahora = time(NULL);
            if (difftime(ahora, shutdown_inicio) >= 10) {
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        kill(procesos[i].pid, SIGKILL);
                        procesos[i].tiempo_fin = ahora;
                        procesos[i].signal_value = SIGKILL;
                        procesos[i].exit_code = -1;
                        procesos[i].activo = 0;
                    }
                }
                printf("\nDCControl finalizado shutdown complete.\n");
                printf("PID\tNombre\t\tTiempo\tExit Code\tSignal\n");
                printf("---\t------\t\t------\t---------\t------\n");
                for (int i = 0; i < total_procesos; i++) {
                    time_t fin = procesos[i].tiempo_fin ? procesos[i].tiempo_fin : ahora;
                    int tiempo_ejecucion = (int)(fin - procesos[i].tiempo_inicio);
                    printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                           procesos[i].pid, procesos[i].nombre, tiempo_ejecucion,
                           procesos[i].exit_code, procesos[i].signal_value); 
                }
                exit(0);
            }
        }

        // revisar time_max para cada proceso
        if (time_max > 0) {
            time_t ahora = time(NULL);
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    int duracion = (int)(ahora - procesos[i].tiempo_inicio);
                    if (!procesos[i].term_enviado && duracion >= time_max) {
                        // primero SIGTERM
                        kill(procesos[i].pid, SIGTERM);
                        procesos[i].signal_value = SIGTERM;
                        procesos[i].term_enviado = 1;
                        procesos[i].term_ts = ahora;
                    } else if (procesos[i].term_enviado && (ahora - procesos[i].term_ts) >= 5) {
                        // después de 5s, SIGKILL si sigue vivo
                        kill(procesos[i].pid, SIGKILL);
                        procesos[i].signal_value = SIGKILL;
                    }
                }
            }
        }

        printf("> ");
        char** input = read_user_input();
        if (input[0] == NULL) {
            free_user_input(input);
            continue;
        }

        // LAUNCH
        if (strcmp(input[0], "launch") == 0) {
            if (input[1] == NULL) {
                printf("Error: no se especificó ejecutable\n");
            } else {
                int count = 0;
                while (input[count + 1] != NULL) count++;
                char* args[count + 1];
                for (int i = 0; i < count; i++) args[i] = input[i + 1];
                args[count] = NULL;

                pid_t pid = fork();
                if (pid == 0) {
                    execvp(args[0], args);
                    perror("Error al ejecutar el programa");
                    exit(EXIT_FAILURE);
                } else if (pid > 0) {
                    printf("Ejecutando %s en segundo plano (PID %d)\n", args[0], pid);
                    procesos[total_procesos].pid = pid;
                    strcpy(procesos[total_procesos].nombre, input[1]);
                    procesos[total_procesos].tiempo_inicio = time(NULL);
                    procesos[total_procesos].tiempo_fin = 0;
                    procesos[total_procesos].exit_code = -1;
                    procesos[total_procesos].signal_value = -1;
                    procesos[total_procesos].activo = 1;
                    procesos[total_procesos].term_enviado = 0;
                    procesos[total_procesos].term_ts = 0;
                    total_procesos++;
                } else {
                    perror("Error al crear el proceso");
                }
            }

        // STATUS
        } else if (strcmp(input[0], "status") == 0) {
            printf("PID\tName\t\tTime\tExit Code\tSignal\n");
            printf("---\t----\t\t----\t---------\t------\n");
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    int status;
                    pid_t res = waitpid(procesos[i].pid, &status, WNOHANG);
                    if (res == procesos[i].pid) {
                        procesos[i].activo = 0;
                        procesos[i].tiempo_fin = time(NULL);
                        if (WIFEXITED(status)) {
                            procesos[i].exit_code = WEXITSTATUS(status);
                            procesos[i].signal_value = 0;
                        } else if (WIFSIGNALED(status)) {
                            procesos[i].exit_code = -1;
                            procesos[i].signal_value = WTERMSIG(status);
                        }
                    }
                }
                time_t fin = procesos[i].tiempo_fin ? procesos[i].tiempo_fin : time(NULL);
                int tiempo_ejec = (int)(fin - procesos[i].tiempo_inicio);
                printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                       procesos[i].pid, procesos[i].nombre, tiempo_ejec,
                       procesos[i].exit_code, procesos[i].signal_value); 
            }

        // ABORT
        } else if (strcmp(input[0], "abort") == 0) {
            if (input[1] == NULL) {
                printf("Uso: abort <tiempo>\n");
            } else {
                double t_minus = atof(input[1]);
                int activos = 0;
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) { activos=1; break; }
                }
                if (!activos) {
                    printf("No hay procesos en ejecución. Abort no se puede ejecutar\n");
                } else {
                    sleep((unsigned int)t_minus);
                    for (int i = 0; i < total_procesos; i++) {
                        if (procesos[i].activo == 1) {
                            kill(procesos[i].pid, SIGTERM);
                            procesos[i].signal_value = SIGTERM;
                            procesos[i].exit_code = -1;
                            procesos[i].activo = 0;
                            procesos[i].tiempo_fin = time(NULL);
                            printf("Abort: PID %d Nombre %s finalizado con SIGTERM\n",
                                   procesos[i].pid, procesos[i].nombre);
                        }
                    }
                }
            }

        // SHUTDOWN
        } else if (strcmp(input[0], "shutdown") == 0) {
            int activos = 0;
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) { activos=1; break; }
            }
            if (!activos) {
                printf("DCControl finalizado shutdown sin procesos.\n");
                for (int i = 0; i < total_procesos; i++) {
                    int tiempo = (int)(time(NULL) - procesos[i].tiempo_inicio);
                    printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                           procesos[i].pid, procesos[i].nombre, tiempo,
                           procesos[i].exit_code, procesos[i].signal_value); 
                }
                exit(0);
            } else {
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) kill(procesos[i].pid, SIGINT);
                }
                shutdown_en_progreso = 1;
                shutdown_inicio = time(NULL);
                printf("Shutdown iniciado. Esperando 10 segundos...\n");
            }

        // EMERGENCY
        } else if (strcmp(input[0], "emergency") == 0) {
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    kill(procesos[i].pid, SIGKILL);
                    procesos[i].signal_value = SIGKILL;
                    procesos[i].exit_code = -1;
                    procesos[i].activo = 0;
                    procesos[i].tiempo_fin = time(NULL);
                }
            }
            printf("¡Emergencia!\nDCControl finalizado.\n");
            for (int i = 0; i < total_procesos; i++) {
                int tiempo = (int)((procesos[i].tiempo_fin?procesos[i].tiempo_fin:time(NULL))
                                    - procesos[i].tiempo_inicio);
                printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                       procesos[i].pid, procesos[i].nombre, tiempo,
                       procesos[i].exit_code, procesos[i].signal_value);
            }
            exit(0);
        }

        free_user_input(input);
    }
}

