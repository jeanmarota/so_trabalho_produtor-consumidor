# Produtor-Consumidor com Semáforos (POSIX)

Projeto Prático de Concorrência e Sincronização
Disciplina: Sistemas Operacionais
Professora: Artemísia Kimberlly

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

O programa roda 2 produtores e 2 consumidores, cada um com 50 iterações, e termina sozinho. No final imprime `Itens restantes no buffer: 0 (esperado: 0)`.

### Formato do log

```
[+] Produtor 1 inseriu item 1003. (Espaços livres: 4)
[-] Consumidor 2 removeu item 1003. (Espaços livres: 5)
[*] Produtor 1 terminou.
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

Recompile e execute de novo. Sem os semáforos, o log mostra a condição de corrida: espaços livres fora do intervalo de 0 a 5 e consumidores removendo `item 0` (posição vazia). Rode mais de uma vez, pois às vezes o programa termina com 0 itens restantes por sorte.

## Estratégia utilizada

O buffer compartilhado é uma fila circular de 5 posições. A sincronização usa **três semáforos POSIX** (`semaphore.h`), sem nenhum `pthread_mutex_t`:

| Semáforo | Valor inicial | Função |
|---|---|---|
| `vazias` | 5 | Conta os espaços livres. O produtor espera quando o buffer está cheio |
| `cheias` | 0 | Conta os itens disponíveis. O consumidor espera quando o buffer está vazio |
| `mutex` | 1 | Semáforo binário que garante uma thread por vez na seção crítica |

**Produtor:** `sem_wait(vazias)` → `sem_wait(mutex)` → insere → `sem_post(mutex)` → `sem_post(cheias)`

**Consumidor:** `sem_wait(cheias)` → `sem_wait(mutex)` → remove → `sem_post(mutex)` → `sem_post(vazias)`

### Como o código evita os problemas clássicos

- **Condição de corrida:** o `mutex` deixa só uma thread por vez mexer no buffer.
- **Busy waiting:** `sem_wait` bloqueia a thread até um `sem_post` acordá-la, sem laço de teste e sem gastar CPU.
- **Deadlock:** os semáforos de contagem (`vazias` e `cheias`) são pegos **antes** do `mutex`. Assim nenhuma thread dorme segurando a trava.
- **Inanição:** todas as threads têm número finito de iterações e esperam em filas de semáforo, então todas terminam. Nos testes, nenhuma ficou esperando indefinidamente.

### Simulação do tempo de processamento

Há `usleep()` com valores aleatórios **fora** da seção crítica (produzindo e consumindo) e **dentro** dela. Isso força trocas de contexto e torna visível o efeito da sincronização, e também o da sua ausência no teste do caos.

## Arquivos

- `produtor_consumidor.c`: código-fonte comentado
- `README.md`: este arquivo
