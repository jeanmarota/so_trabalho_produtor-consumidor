/*
 * Produtor-Consumidor com Semáforos POSIX
 * Disciplina: Sistemas Operacionais - UERN
 * Trio 01: Jean Marota, Ferdinando e Jorge
 *
 * Estratégia:
 *   vazias -> conta espaços livres (começa com TAM_BUFFER)
 *   cheias -> conta itens disponíveis (começa com 0)
 *   mutex  -> semáforo binário (começa com 1) que protege o buffer
 *
 * Teste do caos: mude USAR_SEMAFOROS para 0, recompile e rode.
 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define TAM_BUFFER        5
#define NUM_PRODUTORES    2
#define NUM_CONSUMIDORES  2
#define ITERACOES         50

/* 1 = sincronizado | 0 = TESTE DO CAOS (trava desativada) */
#define USAR_SEMAFOROS 1

#if USAR_SEMAFOROS
  #define ESPERA(s) sem_wait(s)
  #define SINALIZA(s) sem_post(s)
#else
  #define ESPERA(s)   ((void)0)
  #define SINALIZA(s) ((void)0)
#endif

int buffer[TAM_BUFFER];
int entrada = 0, saida = 0;
int contagem = 0;            /* itens atualmente no buffer */

sem_t vazias, cheias, mutex;

void *produtor(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERACOES; i++) {
        int item = id * 1000 + i;
        usleep(rand() % 100000);          /* produzindo (fora da seção crítica) */

        ESPERA(&vazias);                  /* espera espaço livre */
        ESPERA(&mutex);                   /* entra na seção crítica */

        buffer[entrada % TAM_BUFFER] = item;
        entrada++;
        contagem++;
        printf("[+] Produtor %d inseriu item %d. (Espaços livres: %d)\n",
               id, item, TAM_BUFFER - contagem);
        usleep(rand() % 50000);           /* tempo dentro da seção crítica */

        SINALIZA(&mutex);                 /* sai da seção crítica */
        SINALIZA(&cheias);                /* avisa que há um item novo */
    }
    printf("[*] Produtor %d terminou.\n", id);
    return NULL;
}

void *consumidor(void *arg) {
    int id = *(int *)arg;
    for (int i = 0; i < ITERACOES; i++) {
        ESPERA(&cheias);                  /* espera haver item */
        ESPERA(&mutex);

        int item = buffer[saida % TAM_BUFFER];
        saida++;
        contagem--;
        printf("[-] Consumidor %d removeu item %d. (Espaços livres: %d)\n",
               id, item, TAM_BUFFER - contagem);
        usleep(rand() % 50000);

        SINALIZA(&mutex);
        SINALIZA(&vazias);                /* avisa que abriu um espaço */

        usleep(rand() % 100000);          /* consumindo (fora da seção crítica) */
    }
    printf("[*] Consumidor %d terminou.\n", id);
    return NULL;
}

int main(void) {
    pthread_t prod[NUM_PRODUTORES], cons[NUM_CONSUMIDORES];
    int ids_p[NUM_PRODUTORES], ids_c[NUM_CONSUMIDORES];

    sem_init(&vazias, 0, TAM_BUFFER);
    sem_init(&cheias, 0, 0);
    sem_init(&mutex, 0, 1);

    printf("=== Produtor-Consumidor (buffer=%d, %d prod, %d cons, %d iterações) ===\n",
           TAM_BUFFER, NUM_PRODUTORES, NUM_CONSUMIDORES, ITERACOES);
    if (!USAR_SEMAFOROS) printf("!!! MODO CAOS: SEM SINCRONIZAÇÃO !!!\n");

    for (int i = 0; i < NUM_PRODUTORES; i++) {
        ids_p[i] = i + 1;
        pthread_create(&prod[i], NULL, produtor, &ids_p[i]);
    }
    for (int i = 0; i < NUM_CONSUMIDORES; i++) {
        ids_c[i] = i + 1;
        pthread_create(&cons[i], NULL, consumidor, &ids_c[i]);
    }

    for (int i = 0; i < NUM_PRODUTORES; i++)   pthread_join(prod[i], NULL);
    for (int i = 0; i < NUM_CONSUMIDORES; i++) pthread_join(cons[i], NULL);

    printf("=== Fim. Itens restantes no buffer: %d (esperado: 0) ===\n", contagem);

    sem_destroy(&vazias);
    sem_destroy(&cheias);
    sem_destroy(&mutex);
    return 0;
}
