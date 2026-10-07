# Firmware do protótipo ESP32-C6

Este diretório contém o firmware utilizado no protótipo de baixo custo desenvolvido para estimativa da duração do brilho solar.

O arquivo principal é:

`esp32c6_brilho_solar.ino`

A versão publicada corresponde ao código efetivamente utilizado durante a campanha de coleta empregada no TCC, originalmente salvo como `Robusto_27062026.ino`.

## Hardware principal

- ESP32-C6
- sensor BH1750
- RTC PCF8563
- microSD
- monitoramento da tensão da bateria

## Configuração de aquisição

- intervalo entre aquisições: 5 minutos
- período de operação: aproximadamente 05:00–19:00
- armazenamento local em cartão microSD
- uso de deep sleep entre ciclos
- watchdog habilitado
- tratamento de falhas de leitura e escrita

## Observação

Novas versões do firmware podem ser desenvolvidas posteriormente, especialmente para reduzir o intervalo de amostragem para 1 minuto e incorporar sensores internos de temperatura e umidade.

As versões futuras devem ser adicionadas sem substituir silenciosamente esta versão de referência, de forma a preservar a rastreabilidade da configuração utilizada no TCC.
