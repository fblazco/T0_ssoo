#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
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


/*********FUNCIONES ********/

void* shutdown_handler(void* arg) {
    // 1. Enviar SIGINT a procesos activos en el momento
    for (int i = 0; i < total_procesos; i++) {
        if (procesos[i].activo == 1) {
            kill(procesos[i].pid, SIGINT);
        }
    }

    // 2. Esperar 10 segundos sin bloquear la shell
    sleep(10);

    // 3. Enviar SIGKILL a procesos que aún estén activos
    for (int i = 0; i < total_procesos; i++) {
        if (procesos[i].activo == 1) {
            kill(procesos[i].pid, SIGKILL);
            procesos[i].tiempo_fin = time(NULL);
            procesos[i].signal_value = SIGKILL;
            procesos[i].exit_code = -1;
            procesos[i].activo = 0;
        }
    }

    // 4. Imprimir estadísticas
    printf("\nDCControl finalizado.\n");
    printf("PID\tNombre\t\tTiempo\tExit Code\tSignal\n");
    printf("---\t------\t\t------\t---------\t------\n");

    for (int i = 0; i < total_procesos; i++) {
        time_t ahora = procesos[i].tiempo_fin ? procesos[i].tiempo_fin : time(NULL);
        int tiempo_ejecucion = (int)(ahora - procesos[i].tiempo_inicio);
        printf("%d\t%s\t\t%d\t%d\t\t%d\n",
               procesos[i].pid,
               procesos[i].nombre,
               tiempo_ejecucion,
               procesos[i].exit_code,
               procesos[i].signal_value);
    }

    // 5. Finalizar el programa
    exit(0);
}

void* time_max_handler(void* arg){
    while (1){
        sleep(1);
        time_t ahora = time(NULL);
        
        for (int i = 0; i < total_procesos; i++){
            if (procesos[i].activo == 1){
                int duracion = (int)(ahora - procesos[i].tiempo_inicio);
                if (!procesos[i].term_enviado && time_max > 0 && duracion >= time_max){
                    kill(procesos[i].pid, SIGTERM);
                    procesos[i].signal_value = SIGTERM;
                    procesos[i].term_enviado = 1;
                    procesos[i].term_ts = ahora;
                    //printf("Proceso %s (PID %d) excedió time_max. SIGTERM enviado.\n", procesos[i].nombre, procesos[i].pid);
                } else if (procesos[i].term_enviado && (ahora - procesos[i].term_ts) >= 5){
                    kill(procesos[i].pid, SIGKILL);
                    procesos[i].signal_value = SIGKILL;
                    procesos[i].tiempo_fin = ahora;
                    procesos[i].exit_code = -1;
                    procesos[i].activo = 0;
                    printf("Proceso %s (PID %d) no terminó tras SIGTERM. SIGKILL enviado.\n",
                           procesos[i].nombre, procesos[i].pid);
                }
            }
        }
    }
    return NULL;
}

/*************MAIN***********/
int main(int argc, char const *argv[]){
    set_buffer(); // No borrar
    if (argc >= 2) {
        time_max = atoi(argv[1]); // si no entregan, queda -1 (sin límite)
    }
    pthread_t time_max_thread;
    pthread_create(&time_max_thread, NULL, time_max_handler, NULL);
    while (1) {
        // revisar time_max para cada proceso

        printf("> ");
        char** input = read_user_input();
        if (input[0] == NULL) {
            free_user_input(input);
            continue;
        }

        /**
         * funcion: @launch
         * se cuenta cuantos argumentos hay
         * se crea un arreglo para execvp 
         */
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
                    // Proceso hijo
                    execvp(args[0], args);
                    perror("Error al ejecutar el programa");
                    exit(127);  // Código común para "command not found"
                } else if (pid > 0) {
                    // Proceso padre
                    sleep(1);  // Esperamos brevemente para detectar fallo inmediato

                    int status;
                    pid_t result = waitpid(pid, &status, WNOHANG);

                    if (result == pid && WIFEXITED(status) && WEXITSTATUS(status) == 127) {
                        printf("Error: el ejecutable '%s' no existe o falló al ejecutarse\n", args[0]);
                    } else {
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
                    }
                } else {
                    perror("Error al crear el proceso");
                }
        }
        /*
        * funcion: @status
        * este comando entrega al usuario un listado de todos los programas que hayan sido ejecutados 
        * desde DCCcontrol, incluyendo tanto los que siguen en ejecucion como los que ya han finlazo.
        * pid proceso
        * nombre ejecutable
        * tiempo de ejecucion en segundos
        * exit code
        * signal value
        **/
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

        /*
        * funcion: @abort
        * permite terminar luego de t_minus segundos todos los procesos que se esten ejecutando
        * cuenta los procesos activos con que exista 1 prosigue
        * si no hay procesos activos imprime que @abort no se puede ejecutar
        * si hay procesos activos
        * espero el t_minus segudos
        * busco las pid de los procesos y si estos siguen activos los aborto
        * */
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

        /*
        * funcion: @shutdown
        * Termina el programa principal DCControl 
        * Verifica si hay procesos en ejecucion
        * Si hay procesos:
        *   - manda SIGINT a todos los procesos ejecutandose
        *   - no se debe bloquear y debe seguir aceptando comandos durante 10s
        *   - a los procesos que sigan activos se les debe mandar SIGKILL
        *   - se deben imprimir las stats de todos los procesos
        *   - abort queda ANULADO 
        * Si no hay procesos:
        *   - imprime las estadisticas de todos los procesos y termina inmediatamente
        * 
        * */
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
                // Crear hilo para manejar el shutdown en segundo plano
                pthread_t shutdown_thread;
                int result = pthread_create(&shutdown_thread, NULL, shutdown_handler, NULL);
                if (result != 0) {
                    perror("Error al crear el hilo de shutdown");
                } else {
                    printf("Shutdown iniciado. Esperando 10 segundos...\n");
                }
            }


        /*
        * funcion: @emergency
        * Termina inmediatamente todos los procesos que se esten ejecutando
        * Envia SIGKILL de manera directa
        * No hay tiempo de gracia
        * No hay senales previas
        * Se imprimen las estadisticas
        * finaliza DCControl
        * */
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