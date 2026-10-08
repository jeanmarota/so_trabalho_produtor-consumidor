/*
 * Produtor-Consumidor com Semáforos POSIX - versão "da animação"
 * Disciplina: Sistemas Operacionais - UERN
 * Trio 01: Jean Marota, Ferdinando e Jorge
 *
 * Esse arquivo é o mesmo programa que a animacao.html mostra:
 *   - 2 produtores (P1, P2), 2 consumidores (C1, C2) e buffer de 5 posições
 *   - 3 semáforos: vazias (começa em 5), cheias (começa em 0) e mutex (começa em 1)
 *   - cada thread faz UMA rodada, igual na animação
 *   - o log usa as mesmas frases e os mesmos nomes (ESPERA, SINALIZA, itens 1000, 2000...)
 *
 * Como rodar (cenários = abas da animação):
 *   ./animacao 1   melhor caso      (buffer com folga, ninguém dorme)
 *   ./animacao 2   pior: cheio      (começa com 4 itens e vazias = 1)
 *   ./animacao 3   pior: vazio      (consumidores chegam primeiro e dormem)
 *
 * Teste do caos (aba "Teste do caos"): compile com -DUSAR_SEMAFOROS=0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define N 5                       /* tamanho do buffer */
#define NUM_P 2                   /* produtores */
#define NUM_C 2                   /* consumidores */
#define RODADAS 1                 /* quantas vezes cada thread repete */

#ifndef ATRASO_MS
#define ATRASO_MS 800             /* tempo base das pausas */
#endif
#ifndef PASSO_MS
#define PASSO_MS 300              /* pausa depois de cada linha do log */
#endif

/* 1 = com semáforos | 0 = TESTE DO CAOS (ESPERA e SINALIZA viram "nada") */
#ifndef USAR_SEMAFOROS
#define USAR_SEMAFOROS 1
#endif

/* ---------- o buffer e as variáveis compartilhadas ---------- */
int buffer[N];                    /* 0 = posição vazia */
int entrada = 0, saida = 0;       /* onde o produtor grava / de onde o consumidor lê */
int contagem = 0;                 /* quantos itens existem no buffer */
int perdidos = 0;                 /* itens sobrescritos (só acontece no caos) */
sem_t vazias, cheias, mutex;

/* ---------- configuração de cada cenário ---------- */
int atraso_p[NUM_P + 1], atraso_c[NUM_C + 1];   /* quanto cada thread espera pra começar (ms) */
int primeiro_item[NUM_P + 1] = {0, 0, 0};       /* P1 começa em 1000+x, P2 em 2000+x */
int contagem_inicial = 0;

/* Dorme entre 0,5x e 1,5x do tempo base (simula o "tempo de processamento") */
static void pausa(unsigned *seed, int base_ms) {
    usleep((base_ms / 2 + rand_r(seed) % base_ms) * 1000);
}

/* Escreve UMA frase de narração + o estado do sistema, tudo numa chamada só */
static void passo(const char *quem, const char *fmt, ...) {
    char msg[256], slots[96] = "";
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    for (int i = 0; i < N; i++) {
        if (buffer[i]) sprintf(slots + strlen(slots), "[%d] ", buffer[i]);
        else           strcat(slots, "[ -- ] ");
    }
#if USAR_SEMAFOROS
    int v = 0, c = 0, m = 0;
    sem_getvalue(&vazias, &v);
    sem_getvalue(&cheias, &c);
    sem_getvalue(&mutex, &m);
    printf("[%s] %s\n     vazias=%d cheias=%d mutex=%d | buffer: %s| contagem=%d\n\n",
           quem, msg, v, c, m, slots, contagem);
#else
    printf("[%s] %s\n     semáforos DESATIVADOS | buffer: %s| contagem=%d\n\n",
           quem, msg, slots, contagem);
#endif
    usleep(PASSO_MS * 1000);
}

/* ---------- ESPERA e SINALIZA ---------- */
#if USAR_SEMAFOROS
/* ESPERA = sem_wait. Conta uma história: passou direto ou foi dormir? */
static void espera(const char *quem, sem_t *s, const char *nome) {
    if (sem_trywait(s) == 0) {
        passo(quem, "ESPERA(%s): passou direto, sem esperar.%s", nome,
              strcmp(nome, "mutex") == 0 ? " Entrou na seção crítica (fechou a porta)." : "");
        return;
    }
    passo(quem, "ESPERA(%s): o valor já é 0, então %s é BLOQUEADA e dorme na fila de \"%s\" (sem gastar CPU).",
          nome, quem, nome);
    sem_wait(s);                         /* dorme aqui até alguém fazer sem_post */
    passo(quem, "ACORDOU! Alguém fez SINALIZA(%s), então %s segue em frente.", nome, quem);
}

/* SINALIZA = sem_post */
static void sinaliza(const char *quem, sem_t *s, const char *nome) {
    sem_post(s);
    passo(quem, "SINALIZA(%s).%s", nome,
          strcmp(nome, "mutex") == 0 ? " Saiu da seção crítica (abriu a porta)." : "");
}
#define ESPERA(s)   espera(quem, &s, #s)
#define SINALIZA(s) sinaliza(quem, &s, #s)
#else
#define ESPERA(s)   ((void)0)
#define SINALIZA(s) ((void)0)
#endif

/* ---------- Produtor ---------- */
void *produtor(void *arg) {
    int id = *(int *)arg;
    char quem[4];
    unsigned seed = id * 7919;
    sprintf(quem, "P%d", id);

    usleep(atraso_p[id] * 1000);
    for (int r = 0; r < RODADAS; r++) {
        int item = id * 1000 + primeiro_item[id] + r;

        pausa(&seed, ATRASO_MS / 2);                    /* produzindo, fora da seção crítica */
        passo(quem, "%s produz o item %d fora da seção crítica: nem toca no buffer, ninguém precisa esperar.",
              quem, item);

        ESPERA(vazias);                                 /* tem espaço livre? */
        ESPERA(mutex);                                  /* entra na seção crítica */

        int e = entrada;                                /* lê "entrada" */
        pausa(&seed, ATRASO_MS / 2);                    /* (sem mutex, outra thread pode ler o mesmo valor aqui!) */
        int i = e % N;                                  /* o "% N" faz o buffer dar a volta */
        if (buffer[i] != 0) {
            perdidos++;
            passo(quem, "ERRO: %s sobrescreveu o item %d em buffer[%d]. Item perdido!", quem, buffer[i], i);
        }
        buffer[i] = item;
        passo(quem, "%s (dentro da seção crítica) grava o item %d em buffer[%d].", quem, item, i);

        pausa(&seed, ATRASO_MS / 2);
        entrada = e + 1;                                /* se outra thread também leu "e", uma soma se perde */
        int c = contagem;
        pausa(&seed, ATRASO_MS / 4);
        contagem = c + 1;

        SINALIZA(mutex);                                /* sai da seção crítica */
        SINALIZA(cheias);                               /* avisa: tem item novo */
    }
    return NULL;
}

/* ---------- Consumidor ---------- */
void *consumidor(void *arg) {
    int id = *(int *)arg;
    char quem[4];
    unsigned seed = id * 104729;
    sprintf(quem, "C%d", id);

    usleep(atraso_c[id] * 1000);
    for (int r = 0; r < RODADAS; r++) {
        ESPERA(cheias);                                 /* tem item? */
        ESPERA(mutex);                                  /* entra na seção crítica */

        int s = saida;                                  /* lê "saida" */
        pausa(&seed, ATRASO_MS / 2);
        int i = s % N;
        int item = buffer[i];
        buffer[i] = 0;
        if (item == 0)
            passo(quem, "ERRO: %s lê buffer[%d], mas ele está VAZIO. Leu lixo!", quem, i);
        else
            passo(quem, "%s (dentro da seção crítica) retira o item %d de buffer[%d].", quem, item, i);

        saida = s + 1;
        int c = contagem;
        pausa(&seed, ATRASO_MS / 4);
        contagem = c - 1;

        SINALIZA(mutex);                                /* sai da seção crítica */
        SINALIZA(vazias);                               /* avisa: abriu um espaço */

        pausa(&seed, ATRASO_MS / 2);                    /* consumindo, fora da seção crítica */
        passo(quem, "%s consome o item %d fora da seção crítica.", quem, item);
    }
    return NULL;
}

int main(int argc, char **argv) {
    int cenario = (argc > 1) ? atoi(argv[1]) : 1;
    pthread_t prod[NUM_P], cons[NUM_C];
    int ids_p[NUM_P], ids_c[NUM_C];
    const char *nome_cenario;
    int rodada = ATRASO_MS * 3 + PASSO_MS * 9;          /* tempo (folgado) de uma rodada de uma thread */

    setvbuf(stdout, NULL, _IOLBF, 0);

    sem_init(&vazias, 0, N);
    sem_init(&cheias, 0, 0);
    sem_init(&mutex, 0, 1);

    switch (cenario) {
    case 2:                                             /* pior caso: buffer cheio */
        nome_cenario = "Pior caso A: buffer cheio (começa com 4 itens e vazias = 1)";
        for (int k = 0; k < 4; k++) buffer[k] = 1000 + k;
        entrada = 4; contagem = contagem_inicial = 4;
        primeiro_item[1] = 4;                           /* P1 produz o item 1004 */
        sem_destroy(&vazias); sem_init(&vazias, 0, 1);
        sem_destroy(&cheias); sem_init(&cheias, 0, 4);
        atraso_c[1] = atraso_c[2] = ATRASO_MS * 2;      /* consumidores chegam enquanto o P1 ainda está lá dentro */
        break;
    case 3:                                             /* pior caso: buffer vazio */
        nome_cenario = "Pior caso B: buffer vazio (consumidores chegam primeiro)";
        atraso_p[1] = atraso_p[2] = ATRASO_MS * 3;      /* produtores demoram a começar */
        break;
    default:                                            /* melhor caso */
        nome_cenario = "Melhor caso: buffer com folga, ninguém dorme (as threads entram uma de cada vez)";
        atraso_p[2] = rodada;                           /* cada thread entra só depois da anterior */
        atraso_c[1] = 2 * rodada;                       /* terminar, então ninguém precisa esperar */
        atraso_c[2] = 3 * rodada;
    }

#if !USAR_SEMAFOROS
    /* no caos todo mundo entra ao mesmo tempo, pra a condição de corrida aparecer.
       Os consumidores chegam depois dos produtores, igual na animação. */
    atraso_p[1] = atraso_p[2] = 0;
    atraso_c[1] = atraso_c[2] = rodada;
#endif

    printf("=== Produtor-Consumidor | %s ===\n", nome_cenario);
    printf("Buffer de %d posições | %d produtores | %d consumidores | semáforos: vazias, cheias e mutex\n",
           N, NUM_P, NUM_C);
    if (!USAR_SEMAFOROS) printf("!!! TESTE DO CAOS: ESPERA e SINALIZA viraram \"nada\" !!!\n");
    printf("\n");

    for (int i = 0; i < NUM_P; i++) { ids_p[i] = i + 1; pthread_create(&prod[i], NULL, produtor, &ids_p[i]); }
    for (int i = 0; i < NUM_C; i++) { ids_c[i] = i + 1; pthread_create(&cons[i], NULL, consumidor, &ids_c[i]); }
    for (int i = 0; i < NUM_P; i++) pthread_join(prod[i], NULL);
    for (int i = 0; i < NUM_C; i++) pthread_join(cons[i], NULL);

    int reais = 0;
    for (int i = 0; i < N; i++) if (buffer[i]) reais++;
    int esperado = contagem_inicial + (NUM_P - NUM_C) * RODADAS;
    printf("=== Fim | contagem = %d (esperado: %d) | itens reais no buffer: %d | itens perdidos: %d ===\n",
           contagem, esperado, reais, perdidos);

    sem_destroy(&vazias);
    sem_destroy(&cheias);
    sem_destroy(&mutex);
    return 0;
}
