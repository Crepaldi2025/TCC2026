
import streamlit as st

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
    st.title("Estimativa da Duração do Brilho Solar")
    st.subheader("Projeto de TCC — UNIFEI")

    st.markdown(
        """
        Aplicação científica para análise experimental e
        estimativa da duração do brilho solar utilizando
        sensores de iluminância, geometria solar,
        informações de satélite e aprendizado de máquina.
        """
    )

    st.divider()
    st.header("Indicadores do modelo M2")

    c1, c2, c3, c4 = st.columns(4)

    c1.metric("Dias avaliados", "46")
    c2.metric("MAE diário", "24,95 min")
    c3.metric("RMSE diário", "36,77 min")
    c4.metric("R² diário", "0,9166")

    st.caption(
        "Resultados consolidados do M2 na avaliação "
        "original de 46 dias. Não representam "
        "desempenho em dados futuros."
    )

    st.divider()
    st.header("Metodologia")

    st.markdown(
        """
        **Aquisição:** sensor BH1750 (J11), com
        registros de iluminância a cada 5 minutos.

        **Geometria solar:** determinação da posição
        aparente do Sol por algoritmos astronômicos.

        **Aprendizado de máquina:** modelos baseados
        em Extra Trees para estimar a duração
        do brilho solar.

        **Informações complementares:** variáveis
        de nebulosidade derivadas do GOES-19.

        **Referência:** registros do instrumento
        Campbell-Stokes digital CSD3.
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
