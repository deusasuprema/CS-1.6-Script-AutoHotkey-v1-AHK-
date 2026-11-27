# CS 1.6 Script - AutoHotkey v1 / C++ (Win32)

Este repositório contém:
- a versão original em **AutoHotkey v1** (`cs1.6_ahk/`)
- uma versão portada para **C++ (Win32)** (`cs1.6_cpp/`) com foco em **temporização mais precisa** (ms) e execução **leve em segundo plano**

> [!WARNING]
> Scripts e macros podem violar as regras de servidores.
> Use somente em partidas em servidores autorizados ou em um ambiente
> controlado. O uso em servidores multiplayer pode resultar em punicoes.

## Versão C++ (Win32) — recomendada para precisão de tempo

A versão em C++ reimplementa o sistema com APIs nativas do Windows e um **mod menu** (estilo preto/vermelho):
- `Insert` abre/fecha o menu; as funções são ativadas marcando as caixas (NR/NS, Bhop, HUD, Screen Shake) e o perfil é selecionado no menu
- `F12` fecha o programa
- `timeBeginPeriod(1)` + loop com agendamento por deadline, thread prioritária/MMCSS e timers de alta resolução (quando disponíveis) para reduzir jitter
- Entrada via `SendInput` e HUD overlay leve (always-on-top, click-through)
- Salvamento em `%APPDATA%\cs16ahk\settings.ini` (inclui estados das opções do menu)

Código e instruções de compilação: [cs1.6_cpp/README.md](cs1.6_cpp/README.md)

> "Milissegundos exatos" dependem do agendador do Windows; a versão C++ melhora a consistência em relação ao script, mas não é tempo real rígido.
> O Windows padrão não é um sistema de tempo real rígido: preempções do agendador, interrupções e drivers podem impedir garantias determinísticas. A versão C++ busca o melhor comportamento possível em user-mode (baixa latência / "soft real-time"), mas não garante tempo real rígido.

## Script em AutoHotkey v1

Script em AutoHotkey v1 para auxiliar em controles do Counter-Strike 1.6 no Windows.
Ele disponibiliza perfis com compensação simulada de recoil/spread por movimento do mouse,
bunny hop, um HUD simples e teclas para ativar ou desativar as funcoes.

## Recursos (AHK / C++)

- Ativacao global com a tecla `Insert` (inclui bhop e compensacao NR/NS)
- Perfis de arma com padrão de movimento do mouse (compensação simulada de recoil/spread)
- Bunny hop (bhop) ao segurar `Espaco` quando o script estiver ativo
- Movimento automatico do mouse enquanto o botao esquerdo esta pressionado
- HUD sempre visivel na tela para mostrar o estado, o perfil ativo e o bhop
- Salvamento automatico de configuracoes em `%APPDATA%\cs16ahk\settings.ini`
- Encerramento rapido com `F12`
- Compativel com os executaveis `hl.exe`, `cstrike.exe` e `cs.exe`

## Controles (AHK v1)

| Tecla | Funcao |
|---|---|
| `Insert` | Ativa ou desativa o sistema (compensacao NR/NS e bhop) |
| `Home` | Alterna entre os perfis de arma |
| `End` | Mostra ou oculta o HUD |
| `F12` | Fecha o programa |
| `Botao esquerdo` | Aplica o padrão de compensação do perfil ativo enquanto segurado |
| `Espaco` | Executa o bunny hop enquanto segurado (quando ativo) |

## Controles (C++ mod menu)

| Tecla / Ação | Função |
|---|---|
| `Insert` | Abre/fecha o mod menu |
| `F12` | Fecha o programa |
| Mouse (no menu) | Marcar/desmarcar opções e trocar perfil |
| `Botao esquerdo` | Aplica NR/NS quando ativado no menu |
| `Espaco` | Executa o bunny hop quando ativado no menu |

## Perfis

| Perfil | Comportamento simulado |
|---|---|
| AK-47 | Recuo vertical com compensação horizontal progressiva |
| M4A1 | Recuo vertical moderado com leve compensação horizontal |
| SMG | Recuo leve com pequenas variações horizontais |
| Pistola | Recuo leve, majoritariamente vertical |

A compensação é uma **simulação por movimento do mouse** (aproximação de no-recoil/no-spread) e não remove o recoil/spread real do jogo.

O sistema aceita os gatilhos somente quando o jogo esta ativo. Quando o cursor esta
com o formato de seta ou texto, a execucao e interrompida.

## Executando (AHK)

### Executavel

Abra o arquivo:

```text
cs1.6_ahk/CS 1.6 H.exe
```

Inicie o Counter-Strike 1.6 e pressione `Insert` para ativar.

### Codigo-fonte

Requisitos:

- Windows
- AutoHotkey v1.1

Execute:

```text
cs1.6_ahk/CS 1.6 H.ahk
```

A versao de compilacao usada no codigo-fonte declara AutoHotkey 1.1.37.02.

## Compilacao (AHK)

O arquivo `compilar.bat` usa o compilador oficial do AutoHotkey:

```text
C:\Program Files\AutoHotkey\Compiler\Ahk2Exe.exe
```

Ele gera `CS 1.6 H.exe` a partir de `CS 1.6 H.ahk` e usa um icone encontrado na
pasta `.extracted/.rsrc/ICON/`.

Para compilar, instale o AutoHotkey e o compilador nos caminhos configurados pelo
`.bat` e execute:

```bat
cd "cs1.6_ahk"
compilar.bat
```

Caso o compilador esteja instalado em outro local, ajuste as variaveis
`AHK2EXE`, `BASE` e `ICON` no inicio de `compilar.bat`.

## Compilacao (C++)

Veja [cs1.6_cpp/README.md](cs1.6_cpp/README.md) para opções com CMake, MSVC (`build_msvc.bat`) e MinGW (`build_mingw.bat`).

## Estrutura

```text
.
├── README.md
├── cs1.6_ahk/
│   ├── CS 1.6 H.ahk       # Codigo-fonte AHK v1
│   ├── CS 1.6 H.exe       # Executavel AHK compilado
│   ├── compilar.bat       # Script de compilacao AHK no Windows
│   └── .extracted/
│       └── .rsrc/ICON/    # Icones usados pelo projeto
└── cs1.6_cpp/
    ├── README.md          # Instrucoes da versao C++
    ├── CMakeLists.txt
    ├── build_msvc.bat
    ├── build_mingw.bat
    └── src/
        └── main.cpp       # Codigo-fonte C++ (Win32)
```

## Personalizacao (AHK)

No inicio de `CS 1.6 H.ahk` e possivel alterar os valores padrao:

```ahk
RecoilAmount := 4
RecoilName := "AK-47"
RecoilDelay := 20
UseScreenShake := True
UseHUD := True
BhopDelay := 20
```

- `RecoilAmount` / `RecoilName`: perfil inicial exibido ao iniciar o script (quando nao houver configuracao salva)
- `RecoilDelay`: intervalo entre os passos do padrão de compensação, em milissegundos
- `UseScreenShake`: ativa ou desativa pequenas variações aleatórias adicionais
- `UseHUD`: define se o HUD inicia visível
- `BhopDelay`: intervalo entre os saltos do bunny hop, em milissegundos

Os padrões simulados de movimento do mouse por perfil podem ser ajustados na função `GetRecoilStep` em `cs1.6_ahk/CS 1.6 H.ahk`.

Na versão C++, os padrões podem ser ajustados em `GetRecoilStep` em [cs1.6_cpp/src/main.cpp](cs1.6_cpp/src/main.cpp) e os valores persistidos em `%APPDATA%\cs16ahk\settings.ini`.

### Salvamento de configuracoes

O sistema cria/carrega automaticamente o arquivo:

```text
%APPDATA%\cs16ahk\settings.ini
```

Configuracoes salvas:
- Perfil de arma selecionado (`CurrentProfile`)
- Visibilidade do HUD (`UseHUD`)
- Variacao aleatoria (`UseScreenShake`)
- Intervalo do padrao de compensacao (`RecoilDelay`)
- Intervalo do bunny hop (`BhopDelay`)

As configuracoes sao salvas ao alternar perfil/HUD e ao fechar.

## Solucao de problemas

- **O sistema nao ativa no jogo:** confirme que o processo ativo e `hl.exe`,
  `cstrike.exe` ou `cs.exe`.
- **As teclas nao funcionam:** execute como administrador.
- **A compensação não parece precisa:** isso é esperado em uma simulação por padrão; ajuste delays/shake ou os valores de `GetRecoilStep`.
- **O bhop nao funciona:** ative com `Insert` e segure `Espaco` dentro do jogo.
- **A compilacao AHK falha:** confirme os caminhos de `Ahk2Exe.exe` e
  `AutoHotkeySC.bin` em `compilar.bat`.
- **A compilacao C++ falha:** instale MSVC Build Tools ou MinGW e use CMake/scripts em `cs1.6_cpp/`.
- **O HUD nao aparece:** pressione `End` para mostrar o HUD novamente.

## Licenca

Nenhum arquivo de licenca foi incluido no repositorio. Adicione uma licenca antes
de distribuir ou publicar o projeto publicamente.
