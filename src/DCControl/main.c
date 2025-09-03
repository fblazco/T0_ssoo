
#include <stdio.h>
#include <stdlib.h>
#include "../input_manager/manager.h"
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
/*
TODO
launch  ok
abort  ok
shutdown 
status  ok
emergency 
time_max (al final)
manejo de memoria los 2
*/
/*************STRUCTS***********/
struct proceso {
    int pid;
    char nombre[256];
    time_t tiempo_inicio;
    time_t tiempo_fin;
    int exit_code;
    int signal_value;
    int activo;
};
/******* GLOBALES ******/
struct proceso procesos[10];
int total_procesos = 0;
int shutdown_en_progreso = 0;
time_t shutdown_inicio;


/*************FUNCIONES***********/





/*************MAIN***********/
int main(int argc, char const *argv[]){
    set_buffer(); // No borrar
    //printf("%s\n", input[0]);
    
    while (1) {
        /**
         * funcion: @shutdown
         *  - el programa debe funcionar distinto si existe un @shutdown en proceso
         */
        if (shutdown_en_progreso) {
            time_t ahora = time(NULL);
            if (difftime(ahora, shutdown_inicio) >= 10) {
                // Enviar SIGKILL a procesos aún activos
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        kill(procesos[i].pid, SIGKILL);
                        procesos[i].tiempo_fin = ahora;
                        procesos[i].signal_value = SIGKILL;
                        procesos[i].exit_code = -1;
                        procesos[i].activo = 0;}
                }
                printf("\nDCControl finalizado shutdown complete.\n");
                printf("PID\tNombre\t\tTiempo\tExit Code\tSignal\n");
                printf("---\t------\t\t------\t---------\t------\n");
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        kill(procesos[i].pid, SIGINT);
                        int status;
                        pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                        if (result == procesos[i].pid) {
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
                }
                // Mostrar información de todos los procesos
                for (int i = 0; i < total_procesos; i++) {
                    time_t ahora = time(NULL);
                    int tiempo_ejecucion = (int)(ahora - procesos[i].tiempo_inicio);
                    printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                    procesos[i].pid, procesos[i].nombre, tiempo_ejecucion,
                    procesos[i].exit_code, procesos[i].signal_value); 
                    }
                    
                exit(0);return 0;;}
        }
        printf("> ");
        char** input = read_user_input();
        if (input[0] == NULL) {
            free_user_input(input);
            continue;
        }
        // felipe b launch comando + argumentos
        /**
         * funcion: @launch
         * se cuenta cuantos argumentos hay
         * se crea un arreglo para execvp 
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
          
          // Primero limpiar procesos zombie para actualizar información
          for (int i = 0; i < total_procesos; i++) {
              if (procesos[i].activo == 1) {
                  int status;
                  int resultado = waitpid(procesos[i].pid, &status, WNOHANG);
                  if (resultado == procesos[i].pid) {
                      procesos[i].activo = 0;
                      if (WIFEXITED(status)) {
                          procesos[i].exit_code = WEXITSTATUS(status);
                      } else if (WIFSIGNALED(status)) {
                          procesos[i].signal_value = WTERMSIG(status);
                          procesos[i].exit_code = -1;
                      }
                  }
              }
          } 
        // Mostrar información de todos los procesos
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
        * funcion: @abort
        * permite terminar luego de t_minus segundos todos los procesos que se esten ejecutando
        * cuenta los procesos activos con que exista 1 prosigue
        * si no hay procesos activos imprime que @abort no se puede ejecutar
        * si hay procesos activos
        * espero el t_minus segudos
        * busco las pid de los procesos y si estos siguen activos los aborto
        * */
        } else if (strcmp(input[0], "abort")==0){
            double t_minus = atof(input[1]);  // input[1] es el argumento <time>
            int activos = 0;
            for (int i = 0; i < total_procesos; i++){
                if(procesos[i].activo == 1){
                    activos=1;
                    break;}
            }if (!activos) { 
                printf("No hay procesos en ejecución. Abort no se puede ejecutar\n");
            } else{
                sleep((unsigned int)t_minus);
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        int status;
                        pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                        if (result == 0) {
                            // Proceso sigue activo → abortar
                            //procesos[i].tiempo_fin = time(NULL);
                            procesos[i].signal_value = SIGTERM;
                            procesos[i].exit_code = -1;  // No terminó normalmente
                            //double duracion = difftime(procesos[i].tiempo_fin, procesos[i].tiempo_inicio);
                            printf("Abort cumplido\n");
                            printf("PID: %d Nombre: %s ExitCode: %d Signal: %d\n",
                            procesos[i].pid,
                            procesos[i].nombre,
                            //duracion,
                            procesos[i].exit_code,
                            procesos[i].signal_value);
                            kill(procesos[i].pid, SIGTERM);
                            procesos[i].activo = 0;
                        } else if (result == procesos[i].pid) {
                            // Proceso ya terminó
                            //procesos[i].tiempo_fin = time(NULL);
                            procesos[i].exit_code = WEXITSTATUS(status);
                            procesos[i].signal_value = 0;
                            procesos[i].activo = 0;
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
        } else if (strcmp(input[0], "shutdown")==0){
            // 1. verificar
            int activos = 0;
            for (int i = 0; i < total_procesos; i++){
                if(procesos[i].activo == 1){
                    activos=1;
                    break;}
            }
            // 2. no hay procesos imprimir stats
            if(!activos){
                printf("DCControl finalizado shutdown sin procesos.\n");
                printf("PID\tName\t\tTime\tExit Code\tSignal\n");
                printf("---\t----\t\t----\t---------\t------\n");
                // Mostrar información de todos los procesos
                for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    kill(procesos[i].pid, SIGINT);
                    int status;
                    pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                    if (result == procesos[i].pid) {
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
                }
                for (int i = 0; i < total_procesos; i++) {
                    time_t ahora = time(NULL);
                    int tiempo_ejecucion = (int)(ahora - procesos[i].tiempo_inicio);
                    printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                    procesos[i].pid, procesos[i].nombre, tiempo_ejecucion,
                    procesos[i].exit_code, procesos[i].signal_value); 
                }
                exit(0);
            // 3. si hay procesos SIGINT
            }else{
                // mando SIGINT A TODOS
                // ESPERO 10 SEGUNDOS PERO SIGO ACEPTANDO COMANDOS
                // SIGKILL POST 10S
                // IMPRIMIR STATS
                for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        kill(procesos[i].pid, SIGINT);
                        int status;
                        pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                        if (result == procesos[i].pid) {
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
                }
                shutdown_en_progreso = 1;
                shutdown_inicio = time(NULL);
                printf("Shutdown iniciado. Esperando 10 segundos...\n");
            
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
        } else if (strcmp(input[0], "emergency")==0){
            // 1.envio SIGKILL
            for (int i = 0; i < total_procesos; i++) {
                    if (procesos[i].activo == 1) {
                        kill(procesos[i].pid, SIGKILL);
                    }
            }
            // 2.se imprimen las stats
            printf("¡Emergencia!\n");
            printf("DCControl finalizado.\n");
            printf("PID\tName\t\tTime\tExit Code\tSignal\n");
            printf("---\t----\t\t----\t---------\t------\n");
            // 3.Mostrar información de todos los procesos
            for (int i = 0; i < total_procesos; i++) {
                if (procesos[i].activo == 1) {
                    kill(procesos[i].pid, SIGINT);
                    int status;
                    pid_t result = waitpid(procesos[i].pid, &status, WNOHANG);
                    if (result == procesos[i].pid) {
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
            }
            for (int i = 0; i < total_procesos; i++) {
                time_t ahora = time(NULL);
                int tiempo_ejecucion = (int)(ahora - procesos[i].tiempo_inicio);
                printf("%d\t%s\t\t%d\t%d\t\t%d\n",
                procesos[i].pid, procesos[i].nombre, tiempo_ejecucion,
                procesos[i].exit_code, procesos[i].signal_value); 
            }
            exit(0);}
        
        free_user_input(input);
        
    }

}
