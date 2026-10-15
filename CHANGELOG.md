# Changelog

Формат — [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/), версии — [SemVer](https://semver.org/lang/ru/).
Теги релизов — `v<версия>-ue<движок>`, см. [Docs/Versioning.md](Docs/Versioning.md).

## [Unreleased]

### Добавлено
- Свои иконки кнопок Play Here и Place Actor (SVG из [Tabler Icons](https://github.com/tabler/tabler-icons), MIT —
  `Resources/Icons/LICENSE-tabler-icons.txt`) и Slate-стиль `IluUnrealToolStyle`.

### Изменено
- README: убран способ установки в проект (клон/junction в `Plugins/`) — плагин ставится только в движок, как в CLAUDE.md.
- README: убран совет выключать плагин строкой `"Enabled": false` в `.uproject` — проекты плагин не правит.

## [0.2.0] — 2026-10-15 — UE 5.8

Первый выпуск под именем IluUnrealTool: универсальный плагин, не привязанный к проекту.

### Изменено
- Плагин разделён на модули: `IluUnrealToolCore` (общий слой), `IluUnrealToolAgent` (MCP-тулсеты для ИИ-агентов),
  `IluUnrealToolDeveloper` (инструменты тулбара).
- «Vehicle Here» → **Play Here**: ставит любого дефолтного павна из GameMode, не только машину.
- «Gabarit» → **Place Actor**: имя переменной варианта (было жёстко `Type`) и число вариантов (было 0/1)
  настраиваются в «⋮»; поддержаны int, byte и enum.
- Play Here и Place Actor работают на общей базе `FIluPlacementToolBase` вместо двух копий кода.
- Операции над мешами вынесены в `FIluMeshOperations` — их вызывают и меню «⋮», и MCP-тулсет.
- MCP-тулсеты переименованы: `IluUnrealToolAgent.IluMeshToolset`, `IluUnrealToolAgent.IluMaterialToolset`.
- Лог-категория — `LogIluUnrealTool`, секции настроек — `IluUnrealTool.*` (настройки прежнего плагина не переносятся).

### Добавлено
- `IluMeshToolset.RemoveLods` — удаление LOD теперь доступно и агентам.
- Agent skill `UIluUnrealToolAgentSkill` с инструкцией по тулсетам.
- `Agent/` — справка для агентов, CLI-обёртки MCP (`mcp.sh`, `run.sh`, адрес через `ILU_MCP_URL`), пример скрипта.
- `Scripts/Package.ps1` и `Scripts/Install.ps1` — сборка пакета под движок и установка в движок.

### Удалено
- Проектные скрипты сборки материалов и палитр (остались в своём проекте).
