# CS 1.6 Helper - C++ (Win32)

Versão em C++ (Win32) com **mod menu** (estilo hack preto/vermelho), focada em maior precisão de temporização e execução leve em segundo plano.

## Recursos

- **Menu (Insert):** abre/fecha o mod menu; funções são ativadas marcando as caixas
- **NR/NS:** compensação por padrão de movimento do mouse ao segurar o botão esquerdo (quando ativado no menu)
- **Perfis:** AK-47, M4A1, SMG, Pistola (selecionáveis no menu)
- **Bunny Hop:** ao segurar `Espaço` (quando ativado no menu e dentro do jogo)
- **HUD Overlay:** always-on-top, click-through (ativar/desativar no menu)
- **Screen Shake:** variação extra opcional (ativar/desativar no menu)
- **Sair:** `F12` ou botão no menu
- Salvamento/carregamento de configurações em `%APPDATA%\cs16ahk\settings.ini`
- Instância única (mutex)

## Controles

| Tecla / Ação | Função |
|---|---|
| `Insert` | Abre/fecha o mod menu |
| `F12` | Fecha o programa |
| Mouse (no menu) | Marcar/desmarcar opções e trocar perfil |
| Botão esquerdo (no jogo) | Aplica NR/NS quando ativado |
| `Espaço` (no jogo) | Bunny hop quando ativado |

## Precisão de tempo

- `timeBeginPeriod(1)` para granularidade de temporizador
- Thread de trabalho com prioridade elevada e **MMCSS** (`AvSetMmThreadCharacteristics`, "Pro Audio"/"Games") para reduzir preempção
- Temporizador de alta resolução (`CreateWaitableTimerEx` com `CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`, quando disponível) + espera por deadline
- Agendamento por deadline (`std::chrono::steady_clock`) com espera precisa e spin curto próximo do alvo para menor jitter
- Movimento/entrada via `SendInput`

> **Por que não é "tempo real rígido" (hard real-time)?**  
> O Windows padrão é um sistema operacional de propósito geral, não um RTOS. Mesmo em user-mode com prioridades altas, o agendador, interrupções (ISRs/DPCs), drivers e outros processos podem causar preempções/latências imprevisíveis, então não há garantia determinística de cumprir prazos em milissegundos exatos em 100% dos casos.  
> Esta versão busca o melhor comportamento possível em user-mode (**baixa latência / "soft real-time"**), mas garantias de tempo real rígido normalmente exigiriam um RTOS ou componentes em kernel/driver dedicados.

## Compilação

### CMake (MSVC ou MinGW)

```bat
cd cs1.6_cpp
cmake -S . -B build
cmake --build build --config Release
```

### MSVC (Developer Command Prompt)

```bat
cd cs1.6_cpp
build_msvc.bat
```

### MinGW

```bat
cd cs1.6_cpp
build_mingw.bat
```

O executável será gerado em `cs1.6_cpp\build\CS16AHK_CPP.exe` (ou conforme gerador/configuração do CMake).

## Configuração (INI)

Arquivo: `%APPDATA%\cs16ahk\settings.ini`

Seção `[Settings]`:
- `CurrentProfile` (1..4)
- `UseHUD` (0/1)
- `UseScreenShake` (0/1)
- `RecoilDelay` (ms, 1..500)
- `BhopDelay` (ms, 1..500)
- `NrnsEnabled` (0/1)
- `BhopEnabled` (0/1)

## Notas

- NR/NS e Bhop só atuam quando o jogo em primeiro plano é `hl.exe`, `cstrike.exe` ou `cs.exe` e o cursor não está no modo seta/texto (menu/chat).
- Aplicativo user-mode leve em segundo plano (sem driver/kernel-mode).
