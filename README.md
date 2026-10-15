# IluUnrealTool

Editor toolkit for Unreal Engine with two sections: tools for the developer and MCP toolsets for AI agents.

Набор инструментов редактора Unreal Engine из двух разделов: инструменты для разработчика и MCP-тулсеты для ИИ-агентов.
Плагин не привязан к проекту: ставится в движок и работает в любом проекте этой версии движка.

| | |
|---|---|
| Движок | Unreal Engine 5.8 (ветка `main`; другие версии — см. [версии](Docs/Versioning.md)) |
| Платформа | Win64, только редактор (в сборку игры не попадает) |
| Зависимость | `ToolsetRegistry` (Experimental, идёт с движком) — включается автоматически |

## Что внутри

**Раздел разработчика** — тулбар уровня, сразу за блоком Play ([подробно](Docs/Developer.md)):

- **Play Here** — превью дефолтного павна из GameMode ходит под курсором, ЛКМ запускает PIE с этой точки и с этим курсом.
- **Place Actor** — расставляет актор выбранного класса под курсором (с undo), клавиша Z листает его вариант (int/byte/enum-переменная).
- **«⋮»** — настройки инструментов и **Mesh Ops**: пакетные операции над статик-мешами выделенных акторов
  (слить слоты материалов, обнулить вертекс-колор, запечь AO в альфу, оставить UV0, удалить LOD, коллизия Project Default).
- **Time Of Day** — бесконечное колесо времени суток на тулбаре вьюпорта (Ultra Dynamic Sky и любой BP с переменной `Time of Day`).

**Раздел для ИИ-агентов** — MCP-тулсеты через `ToolsetRegistry` ([подробно](Agent/README.md)):

- `IluUnrealToolAgent.IluMeshToolset` — те же операции над статик-мешами плюс диагностика вертекс-колора.
- `IluUnrealToolAgent.IluMaterialToolset` — Named Reroute, размер Comment, ключи Linear Color Curve.
- Agent skill `IluUnrealToolAgentSkill` — инструкция для агента, находится через `AgentSkillToolset.ListSkills`.
- `Agent/Scripts` — CLI-обёртки для вызова MCP из терминала (`mcp.sh`, `run.sh`), `Agent/Examples` — пример скрипта.

ИИ-агенту, которого попросили изменить сам плагин, — порядок работы в [CLAUDE.md](CLAUDE.md).

## Структура

```
IluUnrealTool/
├─ IluUnrealTool.uplugin
├─ Source/
│  ├─ IluUnrealToolCore/       общий слой: лог LogIluUnrealTool, уведомления, операции над мешами (FIluMeshOperations)
│  ├─ IluUnrealToolAgent/      раздел для ИИ-агентов: MCP-тулсеты, их регистрация, agent skill
│  └─ IluUnrealToolDeveloper/  раздел разработчика: тулбар, Play Here, Place Actor, Time Of Day, меню Mesh Ops
├─ Agent/                      для агентов и MCP-клиентов: справка, CLI-скрипты, примеры
├─ Docs/                       документация разработчика и схема версий
├─ Resources/Icons/            SVG-иконки кнопок (Tabler Icons, MIT — см. LICENSE-tabler-icons.txt)
├─ Scripts/                    сборка пакета (Package.ps1) и установка в движок (Install.ps1)
└─ Config/FilterPlugin.ini     что ещё попадает в собранный пакет
```

Модули Agent и Developer друг от друга не зависят и делят только Core. Операция над мешем пишется один раз
в `FIluMeshOperations` — её видят и меню «⋮», и MCP-тулсет.

## Установка

### В движок (все проекты этой версии)

Закрыть редакторы этой версии движка и выполнить в PowerShell из корня репозитория:

```powershell
.\Scripts\Install.ps1 -EngineDir "D:\Software\UE_5.8"
```

Скрипт собирает плагин (`RunUAT BuildPlugin`) и кладёт его в `<Engine>\Engine\Plugins\Marketplace\IluUnrealTool`.
Плагин включён по умолчанию (`EnabledByDefault`), поэтому появляется во всех проектах. Выключить в отдельном
проекте — в `.uproject`:

```json
{ "Name": "IluUnrealTool", "Enabled": false }
```

### В проект (для разработки самого плагина)

Склонировать репозиторий (или сделать junction на него) в `<Project>\Plugins\IluUnrealTool` и собрать проект как обычно —
UBT соберёт плагин вместе с проектом, работают Live Coding и отладка. Копия в проекте перекрывает копию в движке
(движок отдаёт приоритет проектному плагину с тем же именем), так что разрабатывать можно поверх установленного релиза.

### Готовый пакет

`.\Scripts\Package.ps1 -EngineDir <путь к движку>` собирает `Build\IluUnrealTool-<версия>-UE<x.y>-Win64.zip`
для GitHub Release; содержимое zip распаковать в `<Engine>\Engine\Plugins\Marketplace\`.

## Версии и движки

`main` — разработка под актуальный движок, ветки `ue5.8`, `ue5.9`, … — линии под конкретные движки, релизы помечаются
тегами `v<версия>-ue<движок>`. Подробно — в [Docs/Versioning.md](Docs/Versioning.md).

## Настройки

Настройки инструментов хранятся per-project в `Saved/Config/<Platform>Editor/EditorPerProjectUserSettings.ini`,
секции `IluUnrealTool.*`. Логи — категория `LogIluUnrealTool`.
