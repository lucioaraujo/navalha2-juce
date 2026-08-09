# Instalação interna do Navalha 2 no Linux

Pacote validado: `navalha2_0.1.0_x86_64.deb`.

Os requisitos de computador, tela e sistema estão em
[`REQUISITOS_MINIMOS_LINUX.md`](REQUISITOS_MINIMOS_LINUX.md).

## Antes de instalar

- usar uma distribuição Linux de 64 bits (`amd64`);
- copiar o arquivo `.deb` para uma pasta local, por exemplo `Downloads`;
- fechar qualquer instância anterior do Navalha 2.

Este pacote foi criado em um sistema Linux recente. O gerenciador de pacotes
resolverá as bibliotecas compatíveis automaticamente; se ele informar que
`libasound2t64` não existe, a máquina é mais antiga do que o ambiente de build.
Nesse caso, não forçar a instalação: registrar a distribuição/versão para que o
pacote seja gerado em uma base compatível.

Para a distribuição interna mais ampla, executar o workflow GitHub **Package
Debian (Ubuntu 22.04)**. Ele gera um segundo `.deb` com base Ubuntu 22.04, que
evita a dependência `libasound2t64` do pacote criado nesta máquina.

## Instalar

No terminal, dentro da pasta que contém o arquivo:

```bash
sudo apt install ./navalha2_0.1.0_x86_64.deb
```

Depois, abrir **Navalha 2** pelo menu de aplicações ou pelo terminal:

```bash
"Navalha 2"
```

## Verificação de dois minutos

- confirmar que o ícone e o nome aparecem no menu;
- abrir o app e escolher o dispositivo em `AUDIO`;
- carregar um WAV em `SOURCE A`, apertar `PLAY` e depois `STOP`;
- confirmar que não há erro, janela vazia ou áudio inesperado;
- fechar e abrir novamente uma vez.

Anotar apenas o que ocorreu, mais o sistema e sua versão. Em caso de falha de
abertura, enviar a saída deste comando:

```bash
"Navalha 2"
```

## Remover

```bash
sudo apt remove navalha2
```

Isso remove o aplicativo, mas não apaga projetos, takes ou áudios do usuário.
