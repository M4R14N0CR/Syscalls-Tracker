#define _POSIX_C_SOURCE 200809L

#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reg.h>      
#include <sys/user.h>     
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#define MAX_SYSCALL 1024 // Máximo número de system calls a rastrear
#define MAX_DICT_ENTRIES 1024 // Máximo número de entradas en el diccionario de syscalls


// Estructura que representa una entrada en el diccionario de syscalls
typedef struct {
    int number;
    char name[128];
    char description[256];
} syscall_entry;


// Función para cargar el diccionario de syscalls desde un archivo CSV
// Parámetros:
//   filename: nombre del archivo CSV con los datos
//   dict: arreglo de estructuras syscall_entry donde se guardarán las entradas
//   max_entries: número máximo de entradas a cargar
// Retorna el número de entradas cargadas o -1 en caso de error
int load_syscall_dictionary(const char* filename, syscall_entry dict[], int max_entries) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
         perror("fopen");
         return -1;
    }
    char line[512];
    int count = 0;
    
    if (fgets(line, sizeof(line), fp) == NULL) {
        fclose(fp);
        return 0;
    }
    
    // Lee línea por línea el archivo hasta alcanzar max_entries
    while (fgets(line, sizeof(line), fp) && count < max_entries) {
         
         line[strcspn(line, "\r\n")] = 0;
         char *token = strtok(line, ",");
         if (!token) continue;
         int num = atoi(token);
         dict[count].number = num;
         token = strtok(NULL, ",");
         if (token) {
             strncpy(dict[count].name, token, sizeof(dict[count].name) - 1);
             dict[count].name[sizeof(dict[count].name) - 1] = '\0';
         } else {
             dict[count].name[0] = '\0';
         }
         token = strtok(NULL, ",");
         if (token) {
             strncpy(dict[count].description, token, sizeof(dict[count].description) - 1);
             dict[count].description[sizeof(dict[count].description) - 1] = '\0';
         } else {
             dict[count].description[0] = '\0';
         }
         count++;
    }
    fclose(fp);
    return count;
}

// Función para buscar una syscall en el diccionario por su número
// Parámetros:
//   dict: arreglo de entradas del diccionario
//   dict_count: número de entradas cargadas en el diccionario
//   number: número de syscall a buscar
// Retorna un puntero a la entrada encontrada o NULL si no se encuentra
syscall_entry* find_syscall(syscall_entry dict[], int dict_count, int number) {
    for (int i = 0; i < dict_count; i++) {
        if (dict[i].number == number)
            return &dict[i];
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int verbose = 0; // Bandera para modo verbose (detallado)
    int interactive = 0; // Bandera para modo interactivo (espera entrada del usuario)
      
     // Verifica que se haya proporcionado al menos un argumento (además del nombre del programa)
    if (argc < 2) {
        fprintf(stderr, "Uso: %s [opciones rastreador] Prog [opciones de Prog]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    // Determina la posición del programa a ejecutar
    int prog_index = 1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {  // Opción verbose
            verbose = 1;
            prog_index++;
        } else if (strcmp(argv[i], "-V") == 0) {  // Opción verbose e interactiva
            verbose = 1;
            interactive = 1;
            prog_index++;
        } else {
            // Cuando ya no se encuentran opciones del rastreador, se asume que el siguiente argumento es el programa a rastrear
            break;
        }
    }
    
    // Verifica que se haya especificado el programa a ejecutar
    if (prog_index >= argc) {
        fprintf(stderr, "Error: no se especificó el programa a ejecutar.\n");
        exit(EXIT_FAILURE);
    }
    
    // Obtiene los argumentos del programa a ejecutar
    char **prog_args = &argv[prog_index];
    
    // Crea un nuevo proceso (fork)
    pid_t child = fork();
    if (child == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    
    if (child == 0) {
        // Proceso hijo: se prepara para ser rastreado
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) == -1) {
            perror("ptrace TRACEME");
            exit(EXIT_FAILURE);
        }
        kill(getpid(), SIGSTOP);         // Se detiene el proceso para que el padre pueda iniciar el rastreo
        execvp(prog_args[0], prog_args); // Ejecuta el programa indicado, reemplazando el proceso hijo
        perror("execvp"); // Si execvp falla, se muestra el error
        exit(EXIT_FAILURE);
    } else {

         // Proceso padre: se encarga de rastrear al hijo
        int status;
        
        waitpid(child, &status, 0); // Espera a que el hijo se detenga (por SIGSTOP)
        
        // Establece opciones de ptrace para que se pueda distinguir entre una parada normal y una syscall
        if (ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_TRACESYSGOOD) == -1) {
            perror("ptrace SETOPTIONS");
            exit(EXIT_FAILURE);
        }
        
        // Declara el diccionario de syscalls y variables asociadas
        syscall_entry dict[MAX_DICT_ENTRIES];
        int dict_count = 0;
        
        //Se carga el Diccionario de Syscalls
        dict_count = load_syscall_dictionary("syscalls.csv", dict, MAX_DICT_ENTRIES);
        if (dict_count < 0) {
            fprintf(stderr, "No se pudo cargar el diccionario de syscalls.\n");
        }
        
        
        // Arreglo para contar la cantidad de invocaciones de cada syscall
        unsigned long syscall_counts[MAX_SYSCALL] = {0};
        int in_syscall = 0;  // Bandera para determinar si se está entrando o saliendo de una syscall
        struct user_regs_struct regs; // Estructura para almacenar los registros del proceso hijo
        
        // Bucle principal para rastrear el proceso hijo
        while (1) {

            // Indica al hijo que continúe y se detenga en la siguiente entrada/salida de syscall
            if (ptrace(PTRACE_SYSCALL, child, 0, 0) == -1) {
                perror("ptrace SYSCALL");
                break;
            }
            
            // Espera a que el proceso hijo cambie de estado
            waitpid(child, &status, 0);
            if (WIFEXITED(status))
                break; // Si el hijo ha terminado, rompe el bucle
            
            // Comprueba si la detención fue por una syscall
            if (WIFSTOPPED(status) && WSTOPSIG(status) == (SIGTRAP | 0x80)) {
                // Obtiene los registros del proceso hijo para determinar la syscall
                if (ptrace(PTRACE_GETREGS, child, 0, &regs) == -1) {
                    perror("ptrace GETREGS");
                    break;
                }
                if (in_syscall == 0) {
                    // Primera parada: se está entrando a la syscall
                    long syscall_num = regs.orig_rax; // Obtiene el número de la syscall desde el registro
                    if (syscall_num >= 0 && syscall_num < MAX_SYSCALL) {
                        syscall_counts[syscall_num]++;  // Incrementa el contador de la syscall correspondiente
                    }
                    if (verbose) {
                        // Si se activó el modo verbose, se busca la descripción de la syscall en el diccionario
                        syscall_entry *entry = NULL;
                        if (dict_count > 0)
                            entry = find_syscall(dict, dict_count, syscall_num);
                        if (entry) {
                            // Muestra el número, nombre y descripción de la syscall
                            printf("Syscall detectada: %ld - %s: %s\n", syscall_num, entry->name, entry->description);
                        } else {
                            // Si no se encuentra en el diccionario, se muestra como "Desconocido"
                            printf("Syscall detectada: %ld - Desconocido: Sin descripción\n", syscall_num);
                        }
                        if (interactive) {
                            // En modo interactivo, se espera a que el usuario presione Enter para continuar
                            printf("Presione Enter para continuar...");
                            getchar();
                        }
                    }
                    in_syscall = 1; // Indica que ya se ha procesado la entrada de la syscall
                } else {
                    // Segunda parada: se está saliendo de la syscall, se reinicia la bandera
                    in_syscall = 0;
                }
            }
        }
        
        // Al finalizar el rastreo, se muestra una tabla acumulativa de todas las syscalls interceptadas
        printf("\nTabla acumulativa de System Calls:\n");
        printf("%-10s %-20s %-50s %s\n", "Número", "Nombre", "Descripción", "Cantidad");
        for (int i = 0; i < MAX_SYSCALL; i++) {
            if (syscall_counts[i] > 0) {
                syscall_entry *entry = NULL;
                if (dict_count > 0)
                    entry = find_syscall(dict, dict_count, i);
                if (entry) {
                    printf("%-10d %-20s %-50s %lu\n", i, entry->name, entry->description, syscall_counts[i]);
                } else {
                    printf("%-10d %-20s %-50s %lu\n", i, "Desconocido", "Sin descripción", syscall_counts[i]);
                }
            }
        }
    }
    
    return 0;
}
