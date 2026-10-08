# Produtor-Consumidor com Semáforos (POSIX)

Projeto Prático de Concorrência e Sincronização
**Disciplina:** Sistemas Operacionais, UERN
**Professora:** Artemísia Kimberlly

## Integrantes (Trio 01)

- Jean Marota
- Ferdinando
- Jorge

## Como compilar e executar

### Requisitos

- Linux, ou Windows com WSL (Ubuntu)
- `gcc` com suporte a `pthread` (pacote `build-essential`)

Para instalar no Ubuntu/WSL:

```bash
sudo apt update
sudo apt install build-essential
```

> **macOS:** não é recomendado. `sem_init` (semáforo sem nome) não funciona lá. Use Linux ou WSL.

### Compilar

```bash
gcc produtor_consumidor.c -o produtor_consumidor -pthread
```

### Executar

```bash
./produtor_consumidor
```

O programa roda 2 produtores e 2 consumidores, cada um com 10 iterações, e termina sozinho (cerca de 1 minuto). No final imprime `itens restantes no buffer: 0 (esperado: 0)`.

### Formato do log

A cada passo de cada thread o programa imprime um bloco de 4 linhas, separado do próximo por uma linha em branco:

```
[P1] sem_wait(vazias): valor > 0, PASSOU
     semáforos: vazias=4 cheias=0 mutex=1
     buffer: [P2#0] [ -- ] [ -- ] [ -- ] [ -- ]
     na seção crítica: - | esperando: C1(cheias) C2(cheias)
```

- **1ª linha:** quem fez o quê. `P1` é o Produtor 1 e `C2` é o Consumidor 2. `P1#3` é o item 3 do produtor 1.
- **semáforos:** valor atual de `vazias` (espaços livres, começa em 5), `cheias` (itens disponíveis, começa em 0) e `mutex` (1 = livre, 0 = ocupado).
- **buffer:** os 5 slots, mostrando de qual produtor é cada item (`[ -- ]` é slot vazio).
- **na seção crítica:** quem está mexendo no buffer agora.
- **esperando:** threads bloqueadas e o semáforo em que dormem, por exemplo `C1(cheias)`.

### Velocidade da execução

O programa é lento de propósito, para dar tempo de acompanhar o log. `ATRASO_MS` (1200) é o tempo base das pausas de produzir, consumir e ficar na seção crítica, e `PASSO_MS` (400) é a pausa depois de cada bloco impresso. Para mudar, edite os valores no código ou compile com:

```bash
gcc produtor_consumidor.c -o produtor_consumidor -pthread -DATRASO_MS=2000 -DPASSO_MS=800
```

### Teste do caos (sem sincronização)

No início de `produtor_consumidor.c`, altere a linha:

```c
#define USAR_SEMAFOROS 1
```

para:

```c
#define USAR_SEMAFOROS 0
```

Recompile e execute de novo. Sem os semáforos, o log mostra a condição de corrida: linhas `ERRO` (item sobrescrito ou consumidor removendo de slot vazio), vários nomes ao mesmo tempo em "na seção crítica" e valores dos semáforos que não acompanham o buffer. Rode mais de uma vez, pois às vezes o programa termina com 0 itens restantes por sorte. O que prova a quebra são as linhas `ERRO`.

## Estratégia utilizada

O buffer compartilhado é uma fila circular de 5 posições. A sincronização usa **três semáforos POSIX** (`semaphore.h`), sem nenhum `pthread_mutex_t`:

| Semáforo | Valor inicial | Função |
|---|---|---|
| `vazias` | 5 | Conta os espaços livres. O produtor espera quando o buffer está cheio |
| `cheias` | 0 | Conta os itens disponíveis. O consumidor espera quando o buffer está vazio |
| `mutex` | 1 | Semáforo binário que garante uma thread por vez na seção crítica |

Na animação, `ESPERA` é o `sem_wait` e `SINALIZA` é o `sem_post`.

**Produtor:** produz o item (fora da seção crítica) → `ESPERA(vazias)` → `ESPERA(mutex)` → `buffer[entrada % 5] = item` → `SINALIZA(mutex)` → `SINALIZA(cheias)`

**Consumidor:** `ESPERA(cheias)` → `ESPERA(mutex)` → `item = buffer[saida % 5]` → `SINALIZA(mutex)` → `SINALIZA(vazias)` → consome o item (fora da seção crítica)

### Como o código evita os problemas clássicos

- **Condição de corrida:** o `mutex` deixa só uma thread por vez mexer no buffer.
- **Busy waiting:** `sem_wait` bloqueia a thread até um `sem_post` acordá-la, sem laço de teste e sem gastar CPU.
- **Deadlock:** os semáforos de contagem (`vazias` e `cheias`) são pegos **antes** do `mutex`. Assim nenhuma thread dorme segurando a trava.
- **Inanição:** todas as threads têm número finito de iterações e esperam em filas de semáforo, então todas terminam. Nos testes, nenhuma ficou esperando indefinidamente.

### Simulação do tempo de processamento

Há `usleep()` (via a função `pausa`) com valores aleatórios **fora** da seção crítica (produzindo e consumindo) e **dentro** dela. Isso força trocas de contexto e torna visível o efeito da sincronização, e também o da sua ausência no teste do caos.

## Animação (para entender a lógica)

Além do código em C, o repositório tem uma animação que mostra o que acontece, passo a passo, com as 4 threads (P1, P2, C1 e C2), o buffer de 5 posições e os 3 semáforos.

**Como abrir:** dê duplo clique em `animacao.html`, ou abra o arquivo pelo navegador. Não precisa instalar nada nem ter internet.

**O que aparece na tela:**
- um cartão para cada thread, com as linhas do "código" dela. ✓ é linha que já passou, amarelo é a linha da vez e vermelho é onde a thread travou;
- o buffer circular, com as setas de `entrada` e `saída`, a `contagem` e a quantidade real de itens (se forem diferentes, o número fica vermelho);
- os semáforos `vazias`, `cheias` e `mutex` com seus valores, e quem está dormindo na fila de cada um;
- a seção crítica (quem está lá dentro) e uma frase em português explicando cada passo.

**As 4 abas:**
- **Melhor caso:** o buffer tem folga e ninguém é bloqueado. O P2 produz enquanto o P1 ainda está na seção crítica, porque só a parte que mexe no buffer é exclusiva.
- **Pior caso, buffer cheio:** o buffer começa com 4 itens e `vazias = 1`. O P2 dorme em `vazias` e o C1 dorme esperando o `mutex`. Há 2 bloqueios e nenhum dado se perde.
- **Pior caso, buffer vazio:** C1 e C2 chegam primeiro e dormem em `cheias`. Cada `SINALIZA(cheias)` de um produtor acorda um consumidor.
- **Teste do caos:** equivale a `USAR_SEMAFOROS 0`. Sem semáforos, o P2 sobrescreve o item do P1 e a `contagem` termina em -1 (esperado: 0), com 1 item perdido e 1 leitura de lixo.

**Controles:** **Anterior**, **Play/Pausar**, **Próximo** e **Reiniciar**, mais um slider de velocidade (Lento a Rápido).

**Diferenças em relação ao programa em C:**
- Na animação cada thread faz **uma rodada** (6 passos), e no C cada thread faz 10 iterações.
- Na animação os itens aparecem como `1000`, `1001`... (P1) e `2000`... (P2). No log do C o mesmo item aparece como `P1#0`, `P1#1`... A conta é produtor × 1000 + número.

## Arquivos

- `produtor_consumidor.c`: código-fonte em C, comentado
- `animacao.html`: animação interativa (abrir no navegador)
- `README.md`: este arquivo
