# Dados do projeto

Esta pasta documenta os conjuntos de dados utilizados no TCC.

Os dados brutos completos não são distribuídos neste repositório público. O objetivo é registrar a origem, a estrutura esperada e o papel de cada conjunto de dados no fluxo de processamento e modelagem.

## Conjuntos de dados utilizados

### Protótipo J11

Dados de iluminância adquiridos pelo protótipo baseado em ESP32-C6 e sensor BH1750.

Principais informações utilizadas:

- timestamp local;
- iluminância do sensor J11;
- tensão da bateria;
- indicadores de estado do sistema.

No período principal de modelagem, os dados foram organizados em resolução de 5 minutos.

### CSD3

Dados do Campbell-Stokes Digital CSD3 utilizados como referência para a duração do brilho solar.

Os registros de 1 minuto foram agregados em blocos de 5 minutos para compatibilização com a resolução temporal do protótipo.

### GOES-19 / CPTEC

Dados de classificação de nuvens provenientes do GOES-19, processados para a região de Itajubá/MG.

O processamento inclui associação temporal e extração espacial das informações correspondentes ao local do protótipo.

### CNR4

Dados radiométricos utilizados em análises complementares e na verificação de condições de céu.

## Período principal de modelagem

A versão de referência do TCC utiliza o período de:

- 01/07/2026 a 15/08/2026;
- 46 dias completos;
- 7.728 registros em resolução de 5 minutos.

Esse período deve ser entendido como a versão de referência utilizada na modelagem apresentada no TCC.

Novos dados adquiridos posteriormente pelo protótipo poderão ser incorporados em versões futuras do repositório, sem substituir silenciosamente a base utilizada na versão original do trabalho.

## Estrutura esperada

Os notebooks utilizam arquivos equivalentes a:

```text
Base_Consolidada_J11_CSD3_CNR4.xlsx
GOES19_Itajuba_acumulado_nativo.csv
UV_Itajuba_2026.xlsx
```

Os nomes e caminhos podem variar em versões futuras. Sempre que necessário, devem ser ajustados nos parâmetros de configuração dos notebooks.

## Disponibilidade dos dados

Alguns conjuntos de dados podem ter restrições de redistribuição, tamanho elevado ou origem institucional.

Por esse motivo, este repositório prioriza a disponibilização do código e da documentação necessários para reproduzir o fluxo metodológico.

Quando for possível disponibilizar conjuntos derivados ou amostras de dados sem restrições, eles poderão ser adicionados em versões posteriores.

## Versionamento

Os dados utilizados na versão final do TCC devem permanecer associados a uma versão ou release específica do repositório.

A incorporação de novos períodos de coleta deve gerar uma nova versão, preservando a rastreabilidade dos resultados originalmente apresentados.
