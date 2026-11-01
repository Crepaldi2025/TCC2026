# Sensor de duração do brilho solar CSD3

## Descrição

O CSD3, fabricado pela Kipp & Zonen, é um instrumento destinado à medição da duração do brilho solar. Seu funcionamento baseia-se na detecção da irradiância solar direta normal, adotando o limiar de 120 W/m² estabelecido pela Organização Meteorológica Mundial (OMM).

O instrumento utiliza fotodiodos e difusores ópticos e possui saídas elétricas para indicação da presença de brilho solar e estimativa da irradiância solar direta.

## Especificações técnicas

| Parâmetro | Especificação |
|---|---|
| Fabricante | Kipp & Zonen |
| Modelo | CSD3 |
| Grandeza principal | Duração do brilho solar |
| Faixa espectral | 400 a 1.100 nm |
| Temperatura de operação | −40 °C a +70 °C |
| Limiar de brilho solar | Irradiância direta normal superior a 120 W/m² |
| Sinal elétrico de presença de brilho | 1 ± 0,1 V |
| Sensibilidade da saída de irradiância direta | 1 mV/(W/m²) |
| Exatidão dos totais mensais de brilho solar | Superior a 90%, conforme especificação do fabricante |
| Exatidão da irradiância direta em céu claro | Superior a 90%, conforme especificação do fabricante |
| Estabilidade anual | Variação inferior a 2% |
| Dependência térmica | Inferior a 0,1%/°C |
| Tempo de resposta | Inferior a 1 ms |
| Alimentação nominal | 12 V DC |

As especificações correspondem à documentação técnica do fabricante. A faixa de temperatura de operação pode diferir entre versões dos documentos técnicos.

## Aplicação no TCC

O CSD3 instalado na Estação Meteorológica da Universidade Federal de Itajubá foi utilizado como instrumento de referência para o desenvolvimento e a avaliação dos modelos de estimativa da duração do brilho solar.

Na etapa inicial, foi utilizada a variável horária `Suntime_Tot`. Na modelagem definitiva, foi empregado o sinal `CSDSig_Avg`, registrado em intervalos de um minuto.

A classificação dos registros de um minuto utilizou um limiar elétrico de 500 mV para distinguir os estados associados à presença e à ausência de brilho solar. Cinco classificações consecutivas foram reunidas para construir a fração de brilho solar de cada bloco de cinco minutos.

O limiar elétrico de 500 mV não deve ser confundido com o limiar físico de irradiância direta normal de 120 W/m².

As estimativas produzidas pelo modelo M2 original foram avaliadas em relação à duração de brilho solar derivada dos registros do CSD3.

## Referências

KIPP & ZONEN B.V. *CSD3 Sunshine Duration Sensor*. Documentação técnica do fabricante.

CAMPBELL SCIENTIFIC. *CSD3 Sunshine Duration Sensor: User Guide*. Manual técnico.

WORLD METEOROLOGICAL ORGANIZATION. *Guide to Instruments and Methods of Observation*. WMO-No. 8, 2024.
