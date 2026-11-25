# CS 1.6 Script - AutoHotkey v1

Script em AutoHotkey v1 para auxiliar em controles do Counter-Strike 1.6 no Windows.
Ele disponibiliza perfis com compensação simulada de recoil/spread por movimento do mouse,
bunny hop, um HUD simples e teclas para ativar ou desativar as funcoes.

> [!WARNING]
> Scripts e macros podem violar as regras de servidores.
> Use somente em partidas em servidores autorizados ou em um ambiente
> controlado. O uso em servidores multiplayer pode resultar em punicoes.

## Recursos

- Ativacao global com a tecla `Insert` (inclui bhop e compensacao simulada)
- Perfis de arma com padrão de movimento do mouse (compensação simulada de recoil/spread)
- Bunny hop (bhop) ao segurar `Espaco` quando o script estiver ativo
- Movimento automatico do mouse enquanto o botao esquerdo esta pressionado
- HUD sempre visivel na tela para mostrar o estado, o perfil ativo e o bhop
- Salvamento automatico de configuracoes em `%APPDATA%\cs16ahk\settings.ini`
- Encerramento rapido com `F12`
- Compativel com os executaveis `hl.exe`, `cstrike.exe` e `cs.exe`

## Controles

| Tecla | Funcao |
|---|---|
| `Insert` | Ativa ou desativa o script (compensacao simulada e bhop) |
| `Home` | Alterna entre os perfis de arma |
| `End` | Mostra ou oculta o HUD |
| `F12` | Fecha o script |
| `Botao esquerdo` | Aplica o padrão de compensação do perfil ativo enquanto segurado |
| `Espaco` | Executa o bunny hop enquanto segurado (quando o script esta ativo) |

## Perfis

| Perfil | Comportamento simulado |
|---|---|
| AK-47 | Recuo vertical com compensação horizontal progressiva |
| M4A1 | Recuo vertical moderado com leve compensação horizontal |
| SMG | Recuo leve com pequenas variações horizontais |
| Pistola | Recuo leve, majoritariamente vertical |

A compensação é uma **simulação por movimento do mouse** (aproximação de no-recoil/no-spread) e não remove o recoil/spread real do jogo.

O script aceita os gatilhos somente quando o jogo esta ativo. Quando o cursor esta
com o formato de seta ou texto, a execucao e interrompida.

## Executando

### Executavel

Abra o arquivo:

```text
cs1.6_ahk/CS 1.6 H.exe
```

Inicie o Counter-Strike 1.6 e pressione `Insert` para ativar o script.

### Codigo-fonte

Requisitos:

- Windows
- AutoHotkey v1.1

Execute:

```text
cs1.6_ahk/CS 1.6 H.ahk
```

A versao de compilacao usada no codigo-fonte declara AutoHotkey 1.1.37.02.

## Compilacao

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

## Estrutura

```text
.
├── README.md
└── cs1.6_ahk/
    ├── CS 1.6 H.ahk       # Codigo-fonte do script
    ├── CS 1.6 H.exe       # Executavel compilado
    ├── compilar.bat       # Script de compilacao no Windows
    └── .extracted/
        └── .rsrc/ICON/    # Icones usados pelo projeto
```

## Personalizacao

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

### Salvamento de configuracoes

O script cria/carrega automaticamente o arquivo:

```text
%APPDATA%\cs16ahk\settings.ini
```

Configuracoes salvas:
- Perfil de arma selecionado (`CurrentProfile`)
- Visibilidade do HUD (`UseHUD`)
- Variacao aleatoria (`UseScreenShake`)
- Intervalo do padrao de compensacao (`RecoilDelay`)
- Intervalo do bunny hop (`BhopDelay`)

As configuracoes sao salvas ao alternar perfil/HUD e ao fechar o script.

## Solucao de problemas

- **O script nao ativa no jogo:** confirme que o processo ativo e `hl.exe`,
  `cstrike.exe` ou `cs.exe`.
- **As teclas nao funcionam:** execute o AutoHotkey ou o executavel como
  administrador.
- **A compensação não parece precisa:** isso é esperado em uma simulação por padrão; ajuste `RecoilDelay`, `UseScreenShake` ou os valores em `GetRecoilStep`.
- **O bhop nao funciona:** ative o script com `Insert` e segure `Espaco` dentro do jogo.
- **A compilacao falha:** confirme os caminhos de `Ahk2Exe.exe` e
  `AutoHotkeySC.bin` em `compilar.bat`.
- **O HUD nao aparece:** pressione `End` para mostrar o HUD novamente.

## Licenca

Nenhum arquivo de licenca foi incluido no repositorio. Adicione uma licenca antes
de distribuir ou publicar o projeto publicamente.
