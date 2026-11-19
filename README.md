# CS 1.6 Script - AutoHotkey v1

Script em AutoHotkey v1 para auxiliar em controles do Counter-Strike 1.6 no Windows.
Ele disponibiliza perfis de recoil, um HUD simples, bunny hop automatico e uma tecla
para ativar ou desativar todas as funcoes.

> [!WARNING]
> Scripts e macros podem violar as regras de servidores ou anti-cheat.
> Use somente em partidas offline, em servidores autorizados ou em um ambiente
> controlado. O uso em servidores multiplayer pode resultar em punicoes.

## Recursos

- Ativacao global com a tecla `Insert`
- Perfis de recoil para diferentes tipos de arma
- Movimento automatico do mouse enquanto o botao esquerdo esta pressionado
- HUD sempre visivel na tela para mostrar o estado e o perfil ativo
- Bunny hop automatico enquanto `Space` esta pressionado
- Encerramento rapido com `F12`
- Compativel com os executaveis `hl.exe`, `cstrike.exe` e `cs.exe`

## Controles

| Tecla | Funcao |
|---|---|
| `Insert` | Ativa ou desativa o script |
| `Home` | Alterna entre os perfis de recoil |
| `End` | Mostra ou oculta o HUD |
| `F12` | Fecha o script |
| `Botao esquerdo` | Aplica o recoil do perfil ativo enquanto segurado |
| `Space` | Executa bunny hop enquanto segurado |

## Perfis

| Perfil | Recoil configurado |
|---|---:|
| AK-47 | 4 |
| M4A1 | 3 |
| SMG | 2 |
| Pistola | 1 |

O script aceita o gatilho somente quando o jogo esta ativo. Quando o cursor esta
com o formato de seta ou texto, a execucao do recoil e interrompida.

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

No inicio de `CS 1.6 H.ahk` e possivel alterar:

```ahk
RecoilAmount := 4
RecoilName := "AK-47"
RecoilDelay := 20
JumpDelay := 10
UseScreenShake := True
UseHUD := True
```

- `RecoilAmount`: distancia vertical aplicada pelo recoil
- `RecoilDelay`: intervalo entre movimentos, em milissegundos
- `JumpDelay`: intervalo entre os pulos, em milissegundos
- `UseScreenShake`: ativa ou desativa pequenas variacoes aleatorias
- `UseHUD`: define se o HUD inicia visivel

## Solucao de problemas

- **O script nao ativa no jogo:** confirme que o processo ativo e `hl.exe`,
  `cstrike.exe` ou `cs.exe`.
- **As teclas nao funcionam:** execute o AutoHotkey ou o executavel como
  administrador.
- **A compilacao falha:** confirme os caminhos de `Ahk2Exe.exe` e
  `AutoHotkeySC.bin` em `compilar.bat`.
- **O HUD nao aparece:** pressione `End` para mostrar o HUD novamente.

## Licenca

Nenhum arquivo de licenca foi incluido no repositorio. Adicione uma licenca antes
de distribuir ou publicar o projeto publicamente.
