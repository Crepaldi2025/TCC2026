
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
    st.title("Dados experimentais")
    st.info("Módulo em desenvolvimento.")

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
