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

O arquivo csv de dados (devices.csv) é carregado para dentro do programa com um leitura sequencial. O programa lê linha a linha do arquivo e vai consumindo os dados, colocando-os em memória. 

São feitas filtragens dos dados:
- Apenas registros a partir de Março/2024
- Registros sem valores nos sensores são desconsiderados


A carga de dados é dividida entre as threads utilizando o dispositivo como critério. Utilizamos um esquema de filas(Queues) para implementar o multi-threading do programa. Temos uma thread principal responsável por ler o arquivo de entrada `devices.csv` sequencialmente e ir enviando os dados de cada dispositivo para sua respectiva thread.

Para decidirmos qual thread os dados do dispositivos serão enviados utilizamos o algoritmo de MurMurHash, no qual definimos um valor normalizado(hash) entre 1 e N (N sendo o número de threads que o programa utilza) para o dispotivo. Dessa forma, conseguimos garantir que todos os dados relacionados a um dispositivo irão ser analisados sempre por uma mesma Thread/Queue pois os valores de hash nunca irão variar 

A thread principal lê os dados e identifica para qual thread de processamento vai enviá-los. Os dados são enviados para uma Queue da Thread. Na thread ela lê os dados dessa queue e agrega as informações por `dispositivo-mês-ano` 

As threads rodam em modo usuário na maioria do tempo, visto que é um programa em C com pthreads. Porém, em alguns momentos, como na utilização de `pthread_mutex_lock` o programa alterna para modo Kernel temporariamente(tudo é feito pela biblioteca).

Esses resultados ficam em memória no programa, após o processamento de todos os dados por parte de todas as threads e suas respectivas queues, os dados são escritos no arquivo de saída `output.csv` sequencialmente

