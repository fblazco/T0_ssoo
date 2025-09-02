
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"
#include <string.h>


// Meter esto
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>

/*************STRUCTS***********/
// Guardar información de procesos
struct proceso {
    int pid;
    char nombre[256];
    time_t tiempo_inicio;
    time_t tiempo_fin; 
    int exit_code;
    int signal_value;
    int activo;
};

// Variables globales simples
struct proceso procesos[10];
int total_procesos = 0;
/*
launch felipe b¿
abort felipe b
shutdown felipe b
status felipe c
emergency felipe c
time_max felipe c (al final)
manejo de memoria los 2
*/

/*************FUNCIONES***********/





/*************MAIN***********/
int main(int argc, char const *argv[]){
    set_buffer(); // No borrar
    char** input = read_user_input();
    //printf("%s\n", input[0]);
    
    while (1) {
        printf("> ");
        char** input = read_user_input();
    
        if (input[0] == NULL) {
            free_user_input(input);
            continue;
        }
        // felipe b launch comando + argumentos
        /**
         * funcion: @launch
         * DASDJKASDKL
         */
        if (strcmp(input[0], "launch") == 0) {
            if (input[1] == NULL) {
                printf("Error: no se especificó ejecutable\n");
            } else {
                // Contar cuántos argumentos hay
                int count = 0;
                while (input[count + 1] != NULL) {
                    count++;
                }
                // Crear arreglo de argumentos para execvp
                char* args[count + 1];  // +1 para NULL final
                for (int i = 0; i < count; i++) {
                    args[i] = input[i + 1];  // Saltamos "launch"
                }
                args[count] = NULL;  // execvp requiere NULL al final

                pid_t pid = fork();
                if (pid == 0) {
                    // Proceso hijo: ejecuta el programa
                    execvp(args[0], args);
                    perror("Error al ejecutar el programa");
                    exit(EXIT_FAILURE);

                } else if (pid < 0) {
                    perror("Error al crear el proceso");
                } else {
                    // Proceso padre: no espera al hijo
                    printf("Ejecutando %s en segundo plano (PID %d)\n", args[0], pid);
                    // Guardar información del proceso
                    procesos[total_procesos].pid = pid;
                    strcpy(procesos[total_procesos].nombre, input[1]);
                    procesos[total_procesos].tiempo_inicio = time(NULL);
                    procesos[total_procesos].tiempo_fin = 0;
                    procesos[total_procesos].exit_code = -1;
                    procesos[total_procesos].signal_value = -1; 
                    procesos[total_procesos].activo = 1;
                    total_procesos++;
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

                // 1) Actualiza estados: recoge terminados sin bloquear
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    int status;
                    pid_t r = waitpid(procesos[i].pid, &status, WNOHANG);
                    if (r == procesos[i].pid) {
                        procesos[i].activo = 0;
                        procesos[i].tiempo_fin = time(NULL);
                        if (WIFEXITED(status)) {
                            procesos[i].exit_code = WEXITSTATUS(status);
                            procesos[i].signal_value = -1;
                        } else if (WIFSIGNALED(status)) {
                            procesos[i].exit_code = -1;
                            procesos[i].signal_value = WTERMSIG(status);
                }
            }
        }
    }

            // 2) Imprime TODOS (activos y terminados)
            time_t ahora = time(NULL);
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                time_t ahora = time(NULL);
                int tiempo_ejecucion = (int)(ahora - procesos[i].tiempo_inicio);
                printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                procesos[i].pid, procesos[i].nombre, tiempo_ejecucion,
                procesos[i].exit_code, procesos[i].signal_value);
            }
        } 

        /*
        * funcion: @abort <time>
        * Este comando permite terminar despues de time segundos, todos los procesos
        *
        * 
        * */
        // felipe b
        } else if (strcmp(input[0], "abort")==0){
            double t_minus = atof(input[1]);  // input[1] es el argumento <time>
            int activos = 0;
            for (int i = 0; i < total_procesos; i++){
                if(procesos[i].activo == 1){
                    activos=1;
                    break;
                }
            }
             if (!activos) {
                printf("No hay procesos en ejecución. Abort no se puede ejecutar\n");
                return;
            }
            sleep((unsigned int)t_minus);
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    int status;
                    pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                if (result == 0) {
                    // Proceso sigue activo → abortar
                    procesos[i].tiempo_fin = time(NULL);
                    procesos[i].signal_value = SIGTERM;
                    procesos[i].exit_code = -1;  // No terminó normalmente
                    double duracion = difftime(procesos[i].tiempo_fin, procesos[i].tiempo_inicio);
                    printf("Abort cumplido\n");
                    printf("PID: %d Nombre: %s Tiempo: %.2f ExitCode: %d Signal: %d\n",
                        procesos[i].pid,
                        procesos[i].nombre,
                        duracion,
                        procesos[i].exit_code,
                        procesos[i].signal_value);
                    kill(procesos[i].pid, SIGTERM);
                    procesos[i].activo = 0;
                } else if (result == procesos[i].pid) {
                    // Proceso ya terminó
                    procesos[i].tiempo_fin = time(NULL);
                    procesos[i].exit_code = WEXITSTATUS(status);
                    procesos[i].signal_value = 0;
                    procesos[i].activo = 0;
            }
            }
        }
        // felipe b 
        
        
        } else if (strcmp(input[0], "shutdown")==0){
        
        
            // felipe c
        } else if (strcmp(input[0], "emergency")==0){

        }
    free_user_input(input);
    
    }

}

