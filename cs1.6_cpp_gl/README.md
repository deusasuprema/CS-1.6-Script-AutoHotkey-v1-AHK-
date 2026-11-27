# CS 1.6 OpenGL Overlay (Fullscreen) - Experimental

Versão com **overlay interno via hook OpenGL** para funcionar em **tela cheia (fullscreen)** no CS 1.6 (renderizador OpenGL).

> **Status:** implementação inicial/experimental. Teste com o CS 1.6 em OpenGL. Pode exigir ajustes conforme a versão do jogo/driver.

## Arquitetura

- `overlay_dll/` (`cs16_overlay.dll`, **32-bit**): DLL injetada no processo do jogo
  - Hook de `wglSwapBuffers` (OpenGL) para desenhar HUD/menu dentro do jogo
  - Menu in-game (Insert abre/fecha, arrastar pela barra de título, clique nas caixas)
  - NR/NS, Bunny Hop, perfis e salvamento em `%APPDATA%\cs16ahk\settings.ini`
- `injector/` (`CS16_Overlay_Injector.exe`, **32-bit**): localiza `hl.exe`/`cstrike.exe`/`cs.exe` e injeta a DLL

## Requisitos importantes

- **CS 1.6 32-bit (`hl.exe`)**: a DLL e o injetor devem ser **32-bit (x86)**.
- **Toolchain 32-bit** para compilar (o LLVM-MinGW x86_64 que você instalou não gera binários 32-bit por padrão).
  - Opções comuns:
    - LLVM-MinGW (build **i686/32-bit**, UCRT) - ex.: release i686 do Martin Storsjo
    - WinLibs **i686**
    - w64devkit **i686**
  - Após instalar, garanta que `i686-w64-mingw32-g++` (ou equivalente 32-bit) esteja no `PATH`, ou ajuste o script.

## Compilação (MinGW 32-bit)

No PowerShell/CMD (com toolchain 32-bit no `PATH`):

```bat
cd cs1.6_cpp_gl
build_mingw32.bat
```

Saída:
- `build\cs16_overlay.dll`
- `build\CS16_Overlay_Injector.exe`

> Se o seu compilador 32-bit tiver outro nome, edite `build_mingw32.bat` ou compile manualmente.

### Exemplo de compilação manual (quando `i686-w64-mingw32-g++` está disponível)

```powershell
cd cs1.6_cpp_gl
if (!(Test-Path build)) { New-Item -ItemType Directory -Path build | Out-Null }

i686-w64-mingw32-g++ -std=c++17 -O2 -shared -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN `
  overlay_dll\main.cpp -o build\cs16_overlay.dll `
  -mwindows -luser32 -lgdi32 -lopengl32 -lwinmm -ladvapi32 -lpsapi -lavrt

i686-w64-mingw32-g++ -std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN `
  injector\main.cpp -o build\CS16_Overlay_Injector.exe `
  -mwindows -luser32 -ladvapi32
```

## Uso

1. Abra o **CS 1.6** (OpenGL, fullscreen ou janela).
2. Execute `CS16_Overlay_Injector.exe` (coloque na mesma pasta de `cs16_overlay.dll`).
   - Se falhar, tente executar como **Administrador**.
3. No jogo:
   - **Insert:** abrir/fechar menu in-game
   - Clique nas caixas para ativar NR/NS, Bhop, HUD, Screen Shake
   - Troque o perfil com `<` / `>`
   - **F12:** fechar/desinjetar o overlay

## Observações

- **Overlay em fullscreen:** feito por hook OpenGL (`wglSwapBuffers`), deve aparecer no modo OpenGL (inclusive fullscreen) se a injeção funcionar.
- **Segurança/compatibilidade:** injeção e hooks podem causar instabilidade em alguns sistemas/configurações. Use por sua conta e risco.
- **Bhop/NR-NS:** usam simulação de input (`SendInput`) e dependem do jogo estar em foco.
- **Menu travando input:** quando o menu está aberto, o overlay bloqueia parte dos inputs para permitir clique; feche o menu (Insert) para voltar a jogar normalmente.

## Problemas comuns

- **"Falha ao injetar..."**: execute como Administrador e confirme que **jogo, DLL e injetor são 32-bit**.
- **Overlay não aparece:** confirme que o CS está em **OpenGL**; verifique se a DLL foi injetada (ex.: Process Explorer/Process Hacker mostra `cs16_overlay.dll` no `hl.exe`).
- **Compilador errado (64-bit):** erros de injeção/arquitetura indicam binários x64; recompile com toolchain i686.
