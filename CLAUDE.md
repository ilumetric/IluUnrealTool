# IluUnrealTool — правила для ИИ-агента

Этот файл читает агент (Claude Code и др.), которого в **любом** Unreal-проекте попросили изменить или дополнить плагин.
Порядок работы одинаковый везде — дома и на работе, в любом проекте. Язык общения и документов — русский.

## Что это и где живёт

- Универсальный редакторный плагин: инструменты разработчика (тулбар) и MCP-тулсеты для агентов. Устройство — [README.md](README.md).
- Источник правды — этот git-репозиторий (GitHub `ilumetric/IluUnrealTool`, ветка `main`). Клон на каждой машине свой.
- **Плагин ставится только в движок** (`<Engine>\Engine\Plugins\Marketplace\IluUnrealTool`) скриптом `Scripts/Install.ps1`.
  В проекты его **не добавлять никогда**: ни копией, ни junction/symlink в `Plugins/`, ни строкой в `.uproject`.
  В рабочих проектах посторонние плагины запрещены — проект плагин не должен «видеть» в своих файлах.
- Плагин не знает про конкретные проекты: никаких имён, путей `/Game/...`, ассетов и скриптов отдельного проекта.
  Проектное — в репозитории проекта.

## Порядок правки

1. **Сначала словами.** Скажи человеку, что хочешь изменить или добавить и зачем (1–3 строки). Правь после его «да».
2. **Синхронизация.** `git -C <репо> status` — чисто; `git -C <репо> pull --ff-only`. Есть чужие незакоммиченные правки — спроси.
3. **Правка.** Читай код перед правкой. Куда что класть:
   - логика, нужная и человеку, и агенту, — `Source/IluUnrealToolCore` (как `FIluMeshOperations`);
   - MCP-инструмент — `Source/IluUnrealToolAgent`, порядок — [Agent/README.md](Agent/README.md), раздел «Как добавить инструмент»;
   - кнопки, меню, режимы тулбара — `Source/IluUnrealToolDeveloper`.
   Модули Agent и Developer друг от друга не зависят.
4. **Документация в том же коммите:** таблицы в `README.md` / `Agent/README.md` / `Docs/Developer.md`,
   инструкция агенту в `Source/IluUnrealToolAgent/Private/IluAgentSkill.h` (если менялись тулсеты),
   запись в `CHANGELOG.md` → `[Unreleased]`.
5. **Сборка и установка в движок.**
   - Движок — тот, на котором открыт текущий проект: `EngineAssociation` в его `.uproject`, путь — у человека
     (дома `D:\Software\UE_5.8`). Не знаешь путь — спроси.
   - Попроси человека **закрыть все редакторы этого движка** (скрипт удаляет старую копию; открытый редактор держит DLL).
   - `powershell -ExecutionPolicy Bypass -File <репо>\Scripts\Install.ps1 -EngineDir "<движок>"` — собирает
     `RunUAT BuildPlugin` (1–3 мин) и ставит в движок. Ошибки компиляции — чинить и повторять.
6. **Проверка без окна** (редактор ещё закрыт), из папки текущего проекта:
   ```
   <движок>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe <проект>.uproject -run=pythonscript -script="print('ilu ok')" -unattended -nosplash -nullrhi -abslog=<лог>
   ```
   В логе должны быть `Mounting Engine plugin IluUnrealTool`, `LogIluUnrealTool: Agent: MCP-тулсетов зарегистрировано: N`
   и ни одной ошибки `LogIluUnrealTool`. Потом человек открывает редактор: новый MCP-инструмент — вызвать через MCP,
   кнопку или меню — пусть посмотрит человек. Что не проверено — так и скажи.
7. **Коммит и push** — сам, без спроса, после успешной проверки: `git commit` (сообщение по-английски, кратко, как в истории)
   и `git push origin main`. На другой машине изменения подтягиваются `git pull` + `Install.ps1`.
8. **Версии и релизы** (повысить `Version`/`VersionName`, тег, GitHub Release) — только по просьбе человека,
   порядок — [Docs/Versioning.md](Docs/Versioning.md). Без просьбы копить изменения в `[Unreleased]`.

## Конвенции кода

- Идентификаторы — по-английски, префикс `Ilu` (`FIluMeshOperations`, `UIluMeshToolset`).
- Комментарии и тултипы — две строки: EN, затем RU. Логи `UE_LOG` — по-русски, категория `LogIluUnrealTool`.
- MCP-функции: `UFUNCTION(meta = (AICallable))`, `static`, doc-комментарий с `@param`/`@return` (из него строится описание
  для агента); возвращают `"OK: …"` или `"Error: …"`.
- Настройки инструментов — per-project, секции `IluUnrealTool.*` в `EditorPerProjectUserSettings.ini`.
- Модули только `Editor` — в сборку игры плагин не попадает.
- Новый UCLASS или модуль — полная пересборка (Install.ps1), Live Coding тут не используется.

## Новая машина

1. `git clone https://github.com/ilumetric/IluUnrealTool.git <папка>`.
2. В глобальный `~/.claude/CLAUDE.md` добавить (путь — свой):
   ```
   ## Плагин IluUnrealTool (общий для всех проектов Unreal)
   Исходники и git: <папка> (GitHub ilumetric/IluUnrealTool). Ставится только в движок, в проекты не добавляется.
   Если просят изменить или добавить что-то в IluUnrealTool — сначала прочитай <папка>\CLAUDE.md и действуй по нему.
   ```
3. Закрыть редакторы и `.\Scripts\Install.ps1 -EngineDir "<движок>"`.
