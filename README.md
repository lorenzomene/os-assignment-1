# Trabalho de análise de dados de sensoriamento utilizando pthreads 

Este projeto implementa um analisador multithreaded de dados IoT que processa um arquivo CSV contendo leituras de sensores de dispositivos de IoT. O programa utiliza pthreads para distribuir a carga de processamento entre os processadores disponíveis no sistema.

## Compilação e Execução

Requisitos:
- GCC compiler
- Makefile tools
- Arquivo de dados `devices.csv` na pasta raíz do projeto (fora de `/src`)

Para compilar o programa:

```bash
make
```

Para executar o programa com o arquivo CSV padrão (devices.csv):

```bash
make run
```

Para limpar o build:

```bash
make clean
```

## Sobre o projeto

O arquivo de dados (`devices.csv`) é aberto na thread principal e lido sequencialmente.

Nele são feitas filtragens dos dados:
- Apenas registros a partir de Março/2024
- Registros sem valores nos sensores são desconsiderados

Cada linha então é despachada para os workers (threads) utilizando filas, cada dispositivo com seu worker especifico e cada worker escuta a sua propria fila.

Para decidirmos qual thread os dados do dispositivos serão enviados computamos o [MurMurHash](https://en.wikipedia.org/wiki/MurmurHash) do ID do dispositivo e em seguida normalizamos o `int` resultante entre 1 e N (sendo N o número de threads). Dessa forma, conseguimos garantir que todos os dados relacionados a um dispositivo irão ser analisados sempre por uma mesma Thread pois os valores de hash nunca irão variar 

As threads rodam em modo usuário visto que é um programa em C com pthreads.

Os resultados agregados por dispositivos ficam na memoria local do worker até a thread principal sinalizar que acabou de ler o arquivo todo;

Assim que o arquivo todo é lido, todas as threads escrevem na memoria compartilhada e uma thread de escrita é criada, a mesma irá ordenar e escrever todos os dados no arquivo de saida `output.csv` sincronamente.

