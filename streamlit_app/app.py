
from pathlib import Path

import pandas as pd
import plotly.graph_objects as go
import streamlit as st


# ============================================================
# CONFIGURACAO GERAL
# ============================================================

st.set_page_config(
    page_title="Brilho Solar | UNIFEI",
    page_icon="☀️",
    layout="wide",
    initial_sidebar_state="expanded",
)

RAIZ = Path(__file__).resolve().parent.parent

ARQUIVO_DADOS = (
    RAIZ / "data" / "Base_Consolidada_J11_CSD3_CNR4.xlsx"
)

GITHUB = "https://github.com/Crepaldi2025/TCC2026"


# ============================================================
# ESTILO VISUAL
# ============================================================

st.markdown(
    """
    
<style>
.block-container {
    padding-top: 2rem;
    padding-bottom: 3rem;
}


.hero {
    background: linear-gradient(120deg, #163B5C, #286888);
    padding: 20px 28px;
    border-radius: 10px;
    text-align: center;
    color: #FFFFFF;
    margin-bottom: 20px;
}

.hero h1 {
    color: #FFFFFF !important;
    font-size: clamp(25px, 3vw, 34px);
    line-height: 1.2;
    margin: 8px 0 10px 0;
}

.hero .sun {
    font-size: 32px;
    line-height: 1.1;
}

.hero .subtitle {
    font-size: 15px;
    line-height: 1.4;
    color: #E1EFF8;
}

.hero .author {
    font-size: 17px;
    font-weight: 600;
    margin-top: 16px;
}

.hero .institution {
    font-size: 14px;
    margin-top: 5px;
    color: #D5E9F5;
}

.hero .year {
    font-size: 16px;
    font-weight: 600;
    color: #FFDA79;
    margin-top: 5px;
}


.section-text {
    font-size: 17px;
    line-height: 1.7;
}

.hero .advisors {
    font-size: 13px;
    line-height: 1.6;
    color: #DDECF6;
    margin-top: 10px;
}

</style>
    """,
    unsafe_allow_html=True,
)


# ============================================================
# MENU LATERAL
# ============================================================

st.sidebar.title("☀️ Brilho Solar")
st.sidebar.caption("TCC — UNIFEI | 2026")

pagina = st.sidebar.radio(
    "Navegação",
    [
        "Visão geral",
        "Dados experimentais",
        "Modelos M0, M1 e M2",
        "Resultados e validação",
        "Simulador M2",
        "Reprodutibilidade",
    ],
)

st.sidebar.divider()

st.sidebar.markdown(
    f"[Repositório científico no GitHub]({GITHUB})"
)


# ============================================================
# CARREGAMENTO DOS DADOS
# ============================================================

@st.cache_data(show_spinner="Carregando dados experimentais...")
def carregar_dados(caminho):
    df = pd.read_excel(
        caminho,
        sheet_name="Dados_5min",
        usecols=[
            "timestamp_grade_5min_local",
            "j11_lux",
        ],
    )

    df["timestamp_grade_5min_local"] = pd.to_datetime(
        df["timestamp_grade_5min_local"],
        errors="coerce",
    )

    df["j11_lux"] = pd.to_numeric(
        df["j11_lux"],
        errors="coerce",
    )

    df = df.dropna(
        subset=["timestamp_grade_5min_local"]
    )

    df = df.sort_values(
        "timestamp_grade_5min_local"
    ).reset_index(drop=True)

    return df


# ============================================================
# PAGINA 1 - VISAO GERAL
# ============================================================

if pagina == "Visão geral":

    # HTML montado sem indentacao para evitar
    # interpretacao como bloco de codigo Markdown.

    hero = (
        '<div class="hero">'
        '<div class="sun">☀️</div>'
        '<h1>Estimativa da Duração<br>do Brilho Solar</h1>'
        '<div class="subtitle">'
        'Instrumentação de baixo custo, geometria solar'
        '<br>e aprendizado de máquina'
        '</div>'
        '<div class="author">Paulo César Crepaldi</div>'
        '<div class="advisors">'
        'Orientador: Prof. Dr. Marcelo de Paula Corrêa'
        '<br>'
        'Coorientadora: Profa. Dra. Fabrina Bolzan Martins'
        '</div>'
        '<div class="institution">'
        'Universidade Federal de Itajubá — UNIFEI'
        '</div>'
        '<div class="year">2026</div>'
        '</div>'
    )

    st.markdown(
        hero,
        unsafe_allow_html=True,
    )

    st.header("Sobre o projeto")

    st.markdown(
        """
Este trabalho apresenta o desenvolvimento e a avaliação
de uma metodologia para estimar a duração do brilho solar
utilizando um sensor de iluminância de baixo custo,
geometria solar e técnicas de aprendizado de máquina.

A pesquisa utiliza observações experimentais do sensor
**BH1750 (J11)**, registros de referência do **CSD3**
e informações complementares de nebulosidade obtidas
pelo satélite **GOES-19**.

A aplicação disponibiliza ferramentas para exploração
dos dados experimentais, compreensão dos modelos
e avaliação dos resultados científicos.
        """
    )

    st.divider()

    st.subheader("Estrutura da aplicação")

    c1, c2, c3 = st.columns(3)

    with c1:
        st.markdown(
            """
            #### 📊 Dados

            Visualização interativa das medições
            de iluminância e das observações
            meteorológicas.
            """
        )

    with c2:
        st.markdown(
            """
            #### 🌳 Modelos

            Descrição das metodologias M0, M1 e M2,
            baseadas em aprendizado de máquina.
            """
        )

    with c3:
        st.markdown(
            """
            #### 🔬 Validação

            Comparação dos modelos e análise
            dos resultados experimentais.
            """
        )

    st.divider()

    st.caption(
        "Trabalho de Conclusão de Curso | "
        "Universidade Federal de Itajubá | 2026"
    )


# ============================================================
# PAGINA 2 - DADOS EXPERIMENTAIS
# ============================================================

elif pagina == "Dados experimentais":

    st.title("Dados experimentais")

    st.subheader("Iluminância — BH1750 J11")

    if not ARQUIVO_DADOS.exists():

        st.error(
            "O arquivo de dados experimentais não foi encontrado."
        )

    else:

        df = carregar_dados(str(ARQUIVO_DADOS))

        datas = sorted(
            df["timestamp_grade_5min_local"]
            .dt.date.unique()
        )

        if len(datas) == 0:

            st.warning(
                "Nenhuma data válida foi encontrada."
            )

        else:

            st.subheader("Seleção temporal")

            modo = st.radio(
                "Visualização",
                [
                    "Série completa",
                    "Data específica",
                    "Período personalizado",
                ],
                horizontal=True,
            )

            if modo == "Série completa":

                dados = df.copy()

            elif modo == "Data específica":

                data = st.selectbox(
                    "Selecione a data:",
                    datas,
                    format_func=lambda d: d.strftime(
                        "%d/%m/%Y"
                    ),
                )

                dados = df[
                    df["timestamp_grade_5min_local"]
                    .dt.date == data
                ].copy()

            else:

                col1, col2 = st.columns(2)

                with col1:
                    inicio = st.date_input(
                        "Data inicial",
                        value=datas[0],
                        min_value=datas[0],
                        max_value=datas[-1],
                    )

                with col2:
                    fim = st.date_input(
                        "Data final",
                        value=datas[-1],
                        min_value=datas[0],
                        max_value=datas[-1],
                    )

                if inicio > fim:

                    st.error(
                        "A data inicial deve ser anterior "
                        "ou igual à data final."
                    )
                    st.stop()

                mascara = (
                    (
                        df["timestamp_grade_5min_local"].dt.date
                        >= inicio
                    )
                    &
                    (
                        df["timestamp_grade_5min_local"].dt.date
                        <= fim
                    )
                )

                dados = df.loc[mascara].copy()

            # ------------------------------------------------
            # INFORMACOES DO PERIODO
            # ------------------------------------------------

            if dados.empty:

                st.warning(
                    "Não existem registros no período selecionado."
                )

            else:

                inicio_real = dados[
                    "timestamp_grade_5min_local"
                ].min()

                fim_real = dados[
                    "timestamp_grade_5min_local"
                ].max()

                n_validos = dados["j11_lux"].notna().sum()

                c1, c2, c3 = st.columns(3)

                c1.metric(
                    "Primeiro registro",
                    inicio_real.strftime("%d/%m/%Y"),
                )

                c2.metric(
                    "Último registro",
                    fim_real.strftime("%d/%m/%Y"),
                )

                c3.metric(
                    "Medições válidas J11",
                    f"{n_validos:,}".replace(",", "."),
                )

                # --------------------------------------------
                # GRAFICO INTERATIVO
                # --------------------------------------------

                grafico = dados[
                    [
                        "timestamp_grade_5min_local",
                        "j11_lux",
                    ]
                ].copy()

                grafico = grafico.sort_values(
                    "timestamp_grade_5min_local"
                )

                # Insere separadores nas lacunas maiores
                # que o intervalo nominal de 5 minutos.

                diferencas = grafico[
                    "timestamp_grade_5min_local"
                ].diff()

                lacunas = (
                    diferencas > pd.Timedelta(minutes=5)
                )

                linhas = []

                for i, registro in grafico.iterrows():

                    if lacunas.loc[i]:
                        linhas.append(
                            {
                                "tempo": pd.NaT,
                                "lux": float("nan"),
                            }
                        )

                    linhas.append(
                        {
                            "tempo": registro[
                                "timestamp_grade_5min_local"
                            ],
                            "lux": registro["j11_lux"],
                        }
                    )

                dados_plot = pd.DataFrame(linhas)

                fig = go.Figure()

                fig.add_trace(
                    go.Scattergl(
                        x=dados_plot["tempo"],
                        y=dados_plot["lux"],
                        mode="lines",
                        name="J11",
                        line=dict(
                            color="#E69F00",
                            width=1.5,
                        ),
                        connectgaps=False,
                    )
                )

                fig.update_layout(
                    title="Série temporal da iluminância J11",
                    xaxis_title="Data e hora local (UTC−3)",
                    yaxis_title="Iluminância (lux)",
                    hovermode="x",
                    height=520,
                    margin=dict(
                        l=30,
                        r=20,
                        t=60,
                        b=30,
                    ),
                )

                fig.update_xaxes(
                    rangeslider_visible=True
                )

                st.plotly_chart(
                    fig,
                    use_container_width=True,
                )

                st.caption(
                    "Fonte: base experimental J11/CSD3/CNR4. "
                    "Os dados são apresentados sem interpolação. "
                    "Lacunas temporais não são conectadas."
                )

                with st.expander(
                    "Consultar registros selecionados"
                ):

                    st.dataframe(
                        dados,
                        use_container_width=True,
                    )


# ============================================================
# PAGINA 3 - MODELOS
# ============================================================

elif pagina == "Modelos M0, M1 e M2":

    st.title("Modelos M0, M1 e M2")

    st.markdown(
        """
        Os modelos foram desenvolvidos para estimar
        a duração do brilho solar utilizando
        informações experimentais e variáveis
        físicas e meteorológicas.

        **M0 — Extra Trees**

        Modelo baseado em medições do sensor J11
        e variáveis associadas à geometria solar.

        **M1 — Pós-processamento físico**

        Aplicação de critérios físicos e temporais
        às estimativas produzidas pelo modelo M0.

        **M2 — Extra Trees com correção residual GOES**

        Combinação das estimativas de base com
        informações adicionais de nebulosidade
        provenientes do satélite GOES-19.
        """
    )

    st.info(
        "Diagramas, preditores e informações técnicas "
        "serão incorporados nesta página."
    )


# ============================================================
# PAGINA 4 - RESULTADOS E VALIDACAO
# ============================================================

elif pagina == "Resultados e validação":

    st.title("Resultados e validação")

    st.markdown(
        """
        Esta seção apresentará a avaliação científica
        dos modelos M0, M1 e M2.

        Serão incluídos os indicadores de desempenho,
        os gráficos comparativos e a análise dos
        erros de estimativa.
        """
    )

    st.info(
        "Os resultados serão carregados dos arquivos "
        "científicos originais do projeto."
    )


# ============================================================
# PAGINA 5 - SIMULADOR M2
# ============================================================

elif pagina == "Simulador M2":

    st.title("Simulador do modelo M2")

    st.markdown(
        """
        Este módulo permitirá executar o modelo
        treinado utilizando novos registros
        experimentais compatíveis com o pipeline
        científico original.
        """
    )

    st.warning(
        "A execução do modelo M2 ainda não está habilitada. "
        "O pipeline original e as dependências precisam "
        "ser verificados antes da disponibilização."
    )


# ============================================================
# PAGINA 6 - REPRODUTIBILIDADE
# ============================================================

elif pagina == "Reprodutibilidade":

    st.title("Reprodutibilidade científica")

    st.markdown(
        """
        O projeto disponibiliza código-fonte,
        conjuntos de dados, documentação técnica
        e informações necessárias à reprodução
        dos procedimentos científicos.
        """
    )

    st.markdown(
        f"[Acessar o repositório TCC2026]({GITHUB})"
    )

    st.info(
        "A documentação da execução independente "
        "e a auditoria dos resultados serão "
        "apresentadas nesta seção."
    )
