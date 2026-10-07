# Identidade visual do Navalha 2: cópia do Navalha 2 PD

Estes arquivos são o ícone do app e o cabeçalho "arcade" do Navalha 2. Eles
vêm do **Navalha 2 PD** (`lucioaraujo/navalha2-pd`, `app/assets/brand/`),
a versão Pure Data/web do mesmo instrumento, com a mesma autoria e a mesma
licença.

## Por que uma cópia

Regra RASGO de instrumentos autônomos
(`RASGO_DOCUMENTATION/GOVERNANCA_E_TRANSVERSALIDADE.md §7.0`, 7 out. 2026).
Até aqui o build lia esses arquivos da pasta irmã `../NAVALHA2_PD`
(`NAVALHA_PD_PATH`), e a CI baixava o repositório do PD só para isso. Se
o PD mudasse ou movesse um ícone, o build JUCE quebrava ou mudava de ícone
sem aviso. Agora o Navalha 2 JUCE compila só com o que tem.

## Origem

- **Commit do PD:** `446cc79756a1b29cc5cb598c686e269153f5c235` ("Initial private import: Navalha 2 PD
  v0.28.1"). Cópia feita com `git show <commit>:<caminho>`.
- **Conferido:** os arquivos são idênticos aos da pasta de trabalho do PD no
  momento da cópia.
- **Licença:** GPL-3.0, a mesma nos dois projetos.
- **Uso:** o ícone do executável (512 e 32 px), o ícone instalado no Linux
  (128 px) e os recursos da interface (cabeçalho e ícone de 128 px). Ver
  `src/app/CMakeLists.txt`.

## Adaptações

Nenhuma (7 out. 2026). Uma nova versão da identidade entra por nova cópia,
registrada aqui.

## Checksums (SHA-256) no momento da cópia

```
927cd601d275f692c8a0a0380d8228450f0c3298f6879727cf976499121b689b  navalha2-app-icon-128.png
70e5194d7871c05147401fe4e932e8b06f317ba5582e6ee1e3f7eb849decf39d  navalha2-app-icon-32.png
eb248d00423e7f07b34f766549cde207b5c6c1f883bb226c4890b07ae97434ad  navalha2-app-icon-512.png
7160510f4afdfa9b80cb729fcf799b64a3b73b218d68775d3eb5313d3595b771  navalha2-header-arcade.svg
```
