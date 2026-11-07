
import streamlit as st
import textwrap

st.set_page_config(
    page_title="Brilho Solar | UNIFEI",
    page_icon="☀️",
    layout="wide",
    initial_sidebar_state="expanded",
)

# Navegação
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
    "[Repositório científico no GitHub]"
    "(https://github.com/Crepaldi2025/TCC2026)"
)


if pagina == "Visão geral":

    st.markdown(
        textwrap.dedent("""
        <style>
        .hero {
            padding: 48px 36px;
            border-radius: 18px;
            background: linear-gradient(
                125deg, #102D50, #206A91
            );
            color: white;
            text-align: center;
            margin-bottom: 28px;
        }

        .hero .sun {
            font-size: 58px;
            margin-bottom: 12px;
        }

        .hero h1 {
            color: white;
            font-size: clamp(30px, 4vw, 48px);
            font-weight: 700;
            line-height: 1.2;
            margin-bottom: 18px;
        }

        .hero .subtitle {
            font-size: 19px;
            color: #E1EFF8;
            line-height: 1.7;
        }

        .hero .author {
            margin-top: 36px;
            font-size: 21px;
            font-weight: 600;
        }

        .hero .year {
            margin-top: 6px;
            font-size: 17px;
            color: #D5E9F5;
        }

        .intro {
            padding: 12px 8px;
            font-size: 17px;
            line-height: 1.8;
        }
        </style>

        <div class="hero">
            <div class="sun">☀️</div>

            <h1>
                Estimativa da Duração
                do Brilho Solar
            </h1>

            <div class="subtitle">
                Instrumentação de baixo custo,
                geometria solar e aprendizado
                de máquina
            </div>

            <div class="author">
                Paulo César Crepaldi
            </div>

            <div class="year">
                Universidade Federal de Itajubá
                <br>
                UNIFEI · 2026
            </div>
        </div>
                """),
        unsafe_allow_html=True,
    )

    st.markdown(
        """
        ### Sobre o projeto

        Este trabalho apresenta o desenvolvimento
        e a avaliação de uma metodologia para
        estimar a duração do brilho solar por meio
        de um sensor de iluminância de baixo custo,
        associado à geometria solar e a técnicas
        de aprendizado de máquina.

        A pesquisa utiliza observações experimentais
        do sensor **BH1750 (J11)**, registros de
        referência do **CSD3** e informações
        complementares de nebulosidade do
        satélite **GOES-19**.

        A aplicação disponibiliza ferramentas
        para exploração das observações,
        análise dos modelos e avaliação
        dos resultados científicos.
        """
    )

    st.divider()

    st.markdown(
        """
        **Trabalho de Conclusão de Curso — 2026**

        Universidade Federal de Itajubá (UNIFEI)

        [Consultar o repositório científico no GitHub](
        https://github.com/Crepaldi2025/TCC2026
        )
        """
    )




elif pagina == "Dados experimentais":
    import pandas as pd
    import plotly.express as px
    from pathlib import Path

    st.title("Dados experimentais")
    st.subheader("Iluminância — sensor BH1750 J11")

    arquivo = (
        Path(__file__).resolve().parent.parent
        / "data"
        / "Base_Consolidada_J11_CSD3_CNR4.xlsx"
    )

    @st.cache_data
    def carregar_dados(caminho):
        return pd.read_excel(
            caminho,
            sheet_name="Dados_5min",
            usecols=[
                "timestamp_grade_5min_local",
                "j11_lux",
            ],
        )

    if not arquivo.exists():
        st.error("Arquivo experimental não encontrado.")
    else:
        df = carregar_dados(str(arquivo))

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
        ).sort_values("timestamp_grade_5min_local")

        
        datas = sorted(
            df["timestamp_grade_5min_local"]
            .dt.date.unique()
        )

        if len(datas) == 0:
            st.warning("Nenhuma data válida encontrada.")
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
                dados_selecionados = df.copy()

            elif modo == "Data específica":
                data = st.selectbox(
                    "Selecione a data:",
                    datas,
                    format_func=lambda d: d.strftime(
                        "%d/%m/%Y"
                    ),
                )

                dados_selecionados = df[
                    df["timestamp_grade_5min_local"]
                    .dt.date == data
                ]

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
                    st.error("Intervalo de datas inválido.")
                    st.stop()

                mascara = (
                    (df["timestamp_grade_5min_local"].dt.date
                     >= inicio)
                    &
                    (df["timestamp_grade_5min_local"].dt.date
                     <= fim)
                )

                dados_selecionados = df.loc[mascara].copy()

            if dados_selecionados.empty:
                st.warning(
                    "Não existem registros no período."
                )
            else:
                # Lacunas temporais superiores a 5 minutos
                # não devem ser ligadas por linhas.
                dados_plot = dados_selecionados[
                    ["timestamp_grade_5min_local", "j11_lux"]
                ].copy()

                dados_plot = dados_plot.sort_values(
                    "timestamp_grade_5min_local"
                )

                diferenca = dados_plot[
                    "timestamp_grade_5min_local"
                ].diff()

                lacunas = (
                    diferenca > pd.Timedelta(minutes=5)
                )

                dados_plot["segmento"] = (
                    lacunas.cumsum().astype(str)
                )

                fig = px.line(
                    dados_plot,
                    x="timestamp_grade_5min_local",
                    y="j11_lux",
                    color="segmento",
                    labels={
                        "timestamp_grade_5min_local":
                            "Data e hora local",
                        "j11_lux": "Iluminância (lux)",
                    },
                    title="Série temporal da iluminância J11",
                )

                fig.update_traces(
                    line=dict(color="#E69F00"),
                    showlegend=False,
                    connectgaps=False,
                )

                fig.update_layout(
                    hovermode="x",
                    xaxis_title="Data e hora local (UTC−3)",
                    yaxis_title="Iluminância (lux)",
                )

                fig.update_xaxes(
                    rangeslider_visible=True
                )

                st.plotly_chart(
                    fig,
                    use_container_width=True,
                )

                st.caption(
                    "Medições originais do sensor BH1750 J11. "
                    "Lacunas temporais não são interpoladas."
                )

        
                



elif pagina == "Modelos M0, M1 e M2":
    st.title("Modelos M0, M1 e M2")
    st.info("Módulo em desenvolvimento.")

elif pagina == "Resultados e validação":
    st.title("Resultados e validação")
    st.info("Módulo em desenvolvimento.")

elif pagina == "Simulador M2":
    st.title("Simulador do modelo M2")
    st.warning(
        "A execução do modelo será habilitada "
        "após a verificação do pipeline original."
    )

elif pagina == "Reprodutibilidade":
    st.title("Reprodutibilidade científica")
    st.markdown(
        "[Acessar o repositório TCC2026]"
        "(https://github.com/Crepaldi2025/TCC2026)"
    )
