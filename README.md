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

Para limpar o build:

```bash
make clean
```
