Protótipo de baixo custo para estimativa da duração do brilho solar
Repositório associado ao Trabalho de Conclusão de Curso (TCC) “Desenvolvimento e avaliação de um protótipo de baixo custo baseado em sistema embarcado e aprendizagem de máquina para estimativa da duração do brilho solar”, desenvolvido na Universidade Federal de Itajubá (UNIFEI), em 2026.
Objetivo
O projeto tem como objetivo desenvolver e avaliar um sistema de baixo custo para estimar a duração do brilho solar a partir de medições de iluminância realizadas com um sensor óptico conectado a um ESP32-C6.
A avaliação do protótipo utiliza como referência o sensor Campbell-Stokes Digital CSD3 e incorpora, em etapas posteriores, informações de geometria solar e dados de nebulosidade obtidos a partir do satélite GOES-19/CPTEC.
Estrutura do repositório
tcc-brilho-solar/
├── README.md
├── requirements.txt
├── firmware/
│   └── esp32c6_brilho_solar/
│       ├── esp32c6_brilho_solar.ino
│       └── README.md
├── notebooks/
│   ├── 01_preprocessamento_GOES19_CPTEC.ipynb
│   ├── 02_modelagem_extratrees_M0_M1_M2.ipynb
│   ├── 03_analise_estatistica_M0_M1_M2.ipynb
│   └── 04_validacao_temporal_M2.ipynb
├── data/
│   └── README.md
└── figures/
Firmware do protótipo
O firmware foi desenvolvido para um ESP32-C6 e corresponde ao código efetivamente utilizado durante a coleta de dados empregada no TCC.
Principais componentes:
- ESP32-C6;
- sensor de iluminância BH1750;
- RTC PCF8563;
- microSD para armazenamento local;
- monitoramento da tensão da bateria;
- operação em ciclos com deep sleep.
A versão publicada no repositório corresponde ao arquivo originalmente utilizado em campo como:
Robusto_27062026.ino
No repositório, o arquivo foi renomeado para:
firmware/esp32c6_brilho_solar/esp32c6_brilho_solar.ino
A configuração utilizada na coleta foi de aquisição a cada 5 minutos, com operação diurna programada e armazenamento dos registros em cartão microSD.
Processamento dos dados GOES-19
O notebook
notebooks/01_preprocessamento_GOES19_CPTEC.ipynb
é responsável pelo processamento dos dados de classificação de nuvens do GOES-19 disponibilizados pelo CPTEC/INPE.
Entre as principais etapas estão:
- identificação dos arquivos disponíveis;
- associação temporal;
- extração da informação espacial correspondente à região de Itajubá;
- cálculo de variáveis relacionadas à presença de nuvens;
- geração de arquivos processados utilizados posteriormente na modelagem.
Modelagem por aprendizagem de máquina
O notebook principal é:
notebooks/02_modelagem_extratrees_M0_M1_M2.ipynb
Ele implementa o pipeline definitivo utilizado no TCC, com 46 dias completos, correspondentes ao período de 1º de julho a 15 de agosto de 2026, totalizando 7.728 registros em resolução de 5 minutos.
São avaliadas três configurações:
- M0 — variáveis do sensor J11 e geometria solar;
- M1 — M0 acrescido de informações do GOES-19;
- M2 — M0 com correção residual baseada em informações do GOES-19.
A validação é realizada com GroupKFold com 5 folds, utilizando o dia como unidade de agrupamento, de forma a impedir que registros do mesmo dia apareçam simultaneamente nos conjuntos de treinamento e validação.
O notebook também gera:
- previsões OOF (out-of-fold);
- métricas diárias;
- comparação entre M0, M1 e M2;
- gráficos de desempenho;
- arquivos de resultados;
- modelo final salvo para uso posterior.
Comparação estatística entre os modelos
O notebook
notebooks/03_analise_estatistica_M0_M1_M2.ipynb
é destinado à comparação estatística dos erros absolutos diários dos três modelos.
A análise utiliza:
- teste de Friedman para a comparação global;
- teste de Wilcoxon para comparações pareadas;
- correção de Holm para múltiplas comparações;
- nível de significância de 5%.
Esse notebook é separado do pipeline principal para manter clara a distinção entre treinamento/validação e inferência estatística.
Validação temporal independente
O notebook
notebooks/04_validacao_temporal_M2.ipynb
aplica o modelo M2 a um período posterior ao utilizado no treinamento e na validação OOF.
Essa etapa tem como objetivo avaliar o comportamento do modelo em dados temporalmente independentes, sem novo treinamento.
Dados
Os dados brutos completos não são necessariamente distribuídos neste repositório.
A pasta
data/
deve conter apenas informações sobre:
- origem dos dados;
- estrutura esperada dos arquivos;
- nomes das variáveis utilizadas;
- instruções para reprodução do pipeline quando os dados estiverem disponíveis.
Os arquivos de maior volume, bases institucionais ou dados sujeitos a restrições de redistribuição devem permanecer fora do repositório público.
Dependências
As principais bibliotecas Python utilizadas incluem:
- numpy;
- pandas;
- scipy;
- scikit-learn;
- matplotlib;
- pvlib;
- openpyxl;
- joblib;
- xarray;
- netCDF4;
- requests;
- beautifulsoup4.
As versões recomendadas devem ser registradas em:
requirements.txt
Reprodutibilidade
Para reproduzir a análise:
1. preparar os arquivos de entrada conforme descrito em data/README.md;
2. executar o processamento dos dados GOES-19;
3. executar o notebook de modelagem M0/M1/M2;
4. executar a análise estatística;
5. executar a validação temporal independente.
Os caminhos absolutos utilizados originalmente no Google Colab devem ser substituídos por caminhos relativos na versão publicada no GitHub.
Observação sobre versões intermediárias
Durante o desenvolvimento foram avaliadas diferentes alternativas, incluindo outros algoritmos e versões intermediárias do pipeline.
O repositório público prioriza apenas os scripts necessários para reproduzir os resultados finais apresentados no TCC, evitando a inclusão de arquivos duplicados, versões exploratórias ou código não utilizado na análise definitiva.
Autor
Paulo César Crepaldi
Universidade Federal de Itajubá — UNIFEI
Itajubá, Minas Gerais, Brasil
Citação
Caso este repositório seja utilizado em trabalhos acadêmicos, recomenda-se citar o TCC correspondente.
A referência bibliográfica completa poderá ser adicionada após a versão final e defesa do trabalho.
Licença
A licença do repositório será definida posteriormente.
