# IoT Device Statistics Analysis

Este projeto implementa um analisador multithreaded de dados IoT que processa um arquivo CSV contendo leituras de sensores de dispositivos de IoT. O programa utiliza pthreads para distribuir a carga de processamento entre os processadores disponíveis no sistema.

## Compilação e Execução

Para compilar o programa:

```bash
make
```

Para executar o programa com o arquivo CSV padrão (devices.csv):

```bash
make run
```

Para executar com um arquivo CSV específico:

```bash
./iot_stats nome_do_arquivo.csv
```

## Estrutura do Código

O código está organizado em vários arquivos:

- `main.c`: Contém o ponto de entrada do programa e a lógica de distribuição de trabalho
- `csv_parser.h/c`: Funções para análise e processamento do arquivo CSV
- `stats.h/c`: Funções para o cálculo e agregação das estatísticas dos dispositivos

## Carregamento do CSV

O arquivo CSV é processado da seguinte forma:

1. O thread principal (produtor) lê o arquivo CSV linha por linha
2. Cada linha é colocada em uma fila de trabalho compartilhada
3. Os worker threads (consumidores) pegam linhas da fila e as processam
4. O formato do CSV segue a estrutura especificada no enunciado:
   - `id;device;contagem;data;temperatura;umidade;luminosidade;ruido;eco2;etvoc;latitude;longitude`
5. Somente os registros a partir de março de 2024 são considerados na análise (conforme filtrado no `parse_csv_line`)

## Distribuição de Carga entre Threads

O programa distribui a carga de processamento da seguinte maneira:

1. A quantidade de threads utilizadas é determinada automaticamente com base no número de processadores disponíveis no sistema, usando a função `get_nprocs()`
2. A distribuição do trabalho é feita através de uma fila compartilhada, onde o thread principal insere as linhas do CSV e os worker threads as consomem
3. Este modelo de "fila de trabalho compartilhada" permite que os threads mais rápidos processem mais linhas, distribuindo naturalmente a carga de forma eficiente
4. Cada thread mantém suas próprias estruturas de dados estatísticos locais para minimizar a contenção por recursos compartilhados

## Análise de Dados

Para cada linha do CSV processada:

1. Os dados são analisados e convertidos em uma estrutura `Record` pela função `parse_csv_line`
2. Cada worker thread mantém suas próprias estatísticas locais para cada dispositivo e mês
3. Para cada dispositivo e cada mês, são registrados os valores mínimos, máximos e a soma/contagem para cálculo da média
4. A análise é feita para todos os tipos de sensores (temperatura, umidade, luminosidade, ruído, eco2 e etvoc)

## Geração do Arquivo de Resultados

O processo de geração do arquivo CSV de resultados ocorre da seguinte forma:

1. Após todos os worker threads terminarem o processamento, as estatísticas locais de cada thread são combinadas em uma estrutura global
2. Os valores médios são calculados na fase de geração do arquivo
3. O programa cria um diretório `output` (se necessário) e salva os resultados em `output/device_stats.csv`
4. O formato do arquivo de saída segue a especificação:
   - `device;ano-mes;sensor;valor_maximo;valor_medio;valor_minimo`

## Tipo de Threads

O programa utiliza threads do tipo kernel (kernel-level threads), que são criadas pela chamada `pthread_create()` da biblioteca POSIX Threads (pthreads). Estas threads:

- São gerenciadas pelo sistema operacional
- Podem ser executadas em paralelo em múltiplos processadores
- Permitem explorar melhor o hardware multicore disponível
- São mais adequadas para tarefas intensivas de processamento como esta

## Possíveis Problemas de Concorrência

Alguns problemas potenciais de concorrência que foram considerados no projeto:

1. Acesso à fila compartilhada: protegido com mutex para garantir que apenas um thread por vez acesse a fila
2. Condições de corrida nos dados estatísticos: evitadas pelo uso de estruturas de dados locais por thread
3. Sincronização na mesclagem dos resultados: garantida pelo uso de mutex durante a fase de mesclagem
4. Início/término de processamento: controlados usando as variáveis de condição (pthread_cond) e a flag `done`

O design do programa evita a maioria dos problemas de concorrência ao minimizar o compartilhamento de dados mutáveis entre threads e utilizando estruturas thread-local que são mescladas apenas no final do processamento.