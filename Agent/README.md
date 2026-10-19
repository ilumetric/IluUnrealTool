# Раздел для ИИ-агентов (MCP)

Модуль `IluUnrealToolAgent` регистрирует свои тулсеты в `ToolsetRegistry` — реестре инструментов, который отдаёт
MCP-сервер редактора (плагин `ModelContextProtocol`, по умолчанию `http://127.0.0.1:8000/mcp`). Агент видит их рядом
со штатными тулсетами (`EditorToolset`, `MaterialTools`, …) и вызывает через `call_tool`.

Имя тулсета — `<модуль>.<класс без U>`, имя инструмента — имя функции. Ссылка на ассет передаётся как
`{"refPath": "/Game/Path/Asset.Asset"}`. Все инструменты возвращают строку `"OK…"` или `"Error: …"`.

## IluUnrealToolAgent.IluMeshToolset

Операции над **ассетом** статик-меша: меняются все его экземпляры на всех уровнях, ассет помечается изменённым,
отмены нет. Перед массовыми правками — согласовать с человеком, после — сохранить ассеты (`AssetTools.save_assets`).

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `InspectVertexColors` | `Mesh` | только чтение: мин/макс цвета в исходнике и в собранном LOD0, сколько компонентов уровня перекрывают цвет у экземпляра |
| `MergeMaterialSlots` | `Mesh`, `KeepSlot` | вся геометрия всех LOD — в один слот, остальные слоты удаляются |
| `FillVertexColor` | `Mesh`, `Color` | вертекс-колор всех вершин = одно значение (линейный RGBA 0..1) |
| `BakeAOToVertexAlpha` | `Mesh`, `RayCount`, `MaxDistance` | AO от самого меша в альфу вертекс-колора (64 луча, 50 см — хорошие значения) |
| `KeepOnlyUV0` | `Mesh` | удалить UV1+, выключить генерацию лайтмап-UV, лайтмап на UV0 |
| `RemoveLods` | `Mesh` | оставить только LOD0, включая импортные LOD |
| `SetCollisionProjectDefault` | `Mesh` | Collision Complexity = Project Default, в отчёте число простых форм |

## IluUnrealToolAgent.IluMaterialToolset

Помощники для графа материала поверх штатного `MaterialTools`.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `LinkNamedRerouteUsage` | `Usage`, `DeclarationName` | связывает ноду Named Reroute Usage с объявлением по имени (обе ноды создаются через `MaterialTools.add_expression`); потом перекомпилировать материал |
| `SetCommentBounds` | `Comment`, `X`, `Y`, `SizeX`, `SizeY` | позиция и размер рамки Comment |
| `SetLinearColorCurveKeys` | `Curve`, `Colors`, `bStepped` | ключи Linear Color Curve равномерно по 0..1; цвета **линейные**; `bStepped=true` — ровные полосы (палитры для атласов), `false` — плавный градиент |

## IluUnrealToolAgent.IluBlueprintToolset

Blueprint-энумы и тип переменной — штатный `BlueprintTools.add_variable` знает только примитивы и пять структур,
а элементы `UserDefinedEnum` меняет только C++ (`FEnumEditorUtils`, в Python не открыт).

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `CreateEnum` | `AssetPath`, `Entries` | новый Blueprint-энум (`/Game/.../E_Name`) с элементами по порядку; `Entries` — отображаемые имена, внутренние (`NewEnumerator0…`) движок задаёт сам; потом сохранить |
| `SetEnumEntries` | `Enum`, `Entries` | переименовать элементы по индексу, недостающие добавить в конец, лишние удалить с конца; переименование сохраняет записанные значения, удаление сбрасывает |
| `GetEnumEntries` | `Enum` | только чтение: индекс, отображаемое и внутреннее имя элементов любого энума |
| `SetVariableType` | `Blueprint`, `VariableName`, `Type` | сменить тип переменной, как в Variable Type, без диалога подтверждения: `bool`, `byte`, `int`, `int64`, `float`, `name`, `string`, `text` или путь к энуму, структуре, классу; массив/set сохраняются, map не поддерживается; значения на экземплярах сбрасываются; потом скомпилировать и сохранить BP |

## Agent skill

`UIluUnrealToolAgentSkill` — краткая инструкция по тулсетам для агента. ToolsetRegistry находит её сам:
`AgentSkillToolset.ListSkills` → путь скилла → `AgentSkillToolset.GetSkills`.

## Вызов из терминала

`Agent/Scripts` — обёртки над MCP для Git Bash (нужны `curl` и `python`). Адрес сервера — переменная `ILU_MCP_URL`.

```bash
# один инструмент: аргументы — JSON-файл
echo '{"mesh": {"refPath": "/Game/Props/SM_Box.SM_Box"}}' > args.json
Agent/Scripts/mcp.sh IluUnrealToolAgent.IluMeshToolset InspectVertexColors args.json

# Python-скрипт в редакторе: объявляет run(), инструменты зовёт через execute_tool('<toolset>.<tool>', json)
Agent/Scripts/run.sh Agent/Examples/color_curve_palette.py
```

Скрипт для `run.sh` выполняется через `ProgrammaticToolset.execute_tool_script` одним вызовом — так многошаговые
задачи (обход десятков ассетов, сборка графа материала) идут быстрее и атомарнее, чем серия отдельных вызовов.
Пример — [Examples/color_curve_palette.py](Examples/color_curve_palette.py).

## Как добавить инструмент

1. Новая функция в существующем тулсете: `UFUNCTION(meta = (AICallable))`, `static`, doc-комментарий с `@param` и
   `@return` — из него ToolsetRegistry строит описание для агента.
2. Новый тулсет: наследник `UToolsetDefinition` в `Source/IluUnrealToolAgent/Private` + строка в
   `UIluAgentToolsetSubsystem::GetToolsetClasses()`.
3. Логика, нужная и человеку, — в `IluUnrealToolCore` (как `FIluMeshOperations`), тулсет остаётся тонкой обёрткой.
4. Обновить таблицы выше и инструкцию в `IluAgentSkill.h`.
