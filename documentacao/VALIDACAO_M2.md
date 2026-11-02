# Verificacao independente do modelo M2

**Data:** 02/11/2026
**Execucao:** Google Colab, notebook `notebooks/TCC2026_M2_Reprodutibilidade_Banca.ipynb`
**Referencia:** planilha `Indicadores_Modelos_5min_GOES_DEFINITIVO.xlsx` do pacote de reprodutibilidade da banca.

## Resultados

- Periodo: 01/07 a 15/08/2026; 46 dias e 7.728 registros de cinco minutos.
- Todos os 7.728 registros foram pareados pelo instante, sem ausencias.
- Metricas diarias do M2 reproduzidas dentro do arredondamento apresentado: MAE 24,95 min/dia; RMSE 36,77 min/dia; vies -1,54 min/dia; R2 0,9166; Pearson 0,9652.
- Diferencas entre predicoes individuais (reproducao menos referencia): media 0,0000244597; MAE 0,0011080420; RMSE 0,0031920205; maxima absoluta 0,0327924223, em escala de fracao 0 a 1.
- Diferenca media absoluta equivalente a 0,3324 s por intervalo de cinco minutos; maxima de 9,8377 s. Houve diferencas superiores a 1e-10 em 2.690 registros (34,81%).
- Hashes SHA-256 dos quatro arquivos em `data/` conferidos com `data/inventario_SHA256.csv`: todos coincidentes.

## Ambiente observado

Python 3.13.18; NumPy 2.1.3; pandas 2.2.3; SciPy 1.16.3; scikit-learn 1.6.1; joblib 1.6.0; openpyxl 3.1.5; pvlib 0.16.1; Matplotlib 3.10.0. Versoes registradas em `requirements_M2.txt` (nao necessariamente iguais as do treinamento original).

## Conclusao e alcance

A execucao independente apresentou concordancia das metricas agregadas e integridade dos dados, mas **nao identidade numerica das 7.728 predicoes individuais**. As divergencias sao pequenas na escala de cinco minutos; sua origem nao foi determinada. Nao houve alteracao do modelo M2, dos hiperparametros ou dos dados. Esta verificacao nao substitui uma comparacao completa de ambientes nem garante resultados bit a bit identicos em outras versoes de software.
