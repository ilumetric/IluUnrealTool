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
| `InspectUVs` | `Mesh` | только чтение: индекс и генерация лайтмап-UV, диапазоны U/V каждого канала в исходнике и в собранном LOD0 — проверить, что данные в UV1+ (вертекс-колор, запечённый в UV) пережили импорт |
| `DisableLightmapUVs` | `Mesh` | выключить генерацию лайтмап-UV, лайтмап на UV0, **все UV-каналы сохраняются** (меши с данными в UV1+) |
| `RemoveLods` | `Mesh` | оставить только LOD0, включая импортные LOD |
| `SetCollisionProjectDefault` | `Mesh` | Collision Complexity = Project Default, в отчёте число простых форм |

Запекание акторов (не меняют существующие ассеты): видимые меш-компоненты акторов — Dynamic Mesh (блокауты на
Geometry Script) и Static Mesh — сливаются в один меш, слот на уникальный материал. `bPivotAtBottomCenter` — пивот
в центре низа общих габаритов (иначе в начале координат мира). Акторы — например, из `EditorAppToolset.GetSelectedActors`.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `BakeActorsToStaticMesh` | `Actors`, `AssetPath`, `bPivotAtBottomCenter` | новый ассет статик-меша (`/Game/.../SM_Имя`, существующий не перезаписывается), коллизия complex as simple; потом сохранить |
| `ExportActorsToFile` | `Actors`, `FilePath`, `bPivotAtBottomCenter` | сразу в файл на диске, `.fbx` или `.obj` по расширению (абсолютный путь, перезаписывается); в Content ничего не создаётся |

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

## IluUnrealToolAgent.IluPCGToolset

HLSL-код нод **Custom HLSL** и ассетов **PCG Compute Source** — штатный `PCGToolset` и `ObjectTools` его не видят
(поля кода в движке без флага Edit). Работает через рефлексию: плагин PCG тулсету не нужен. `Target` — нода графа
(путь из `PCGToolset.GetGraphStructure`, например `/Game/Folder/Graph.Graph:CustomHLSL_0`), объект её настроек
или ассет Compute Source.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `GetHLSLSource` | `Target`, `bIncludeDeclarations` | только чтение: тело ядра, свои функции, Kernel Source Override и Additional Sources; с `bIncludeDeclarations` — сгенерированные объявления (функции доступа к пинам: `Landscape_GetHeight`, `Out_SetPosition`, … и хелперы) |
| `SetHLSLSource` | `Target`, `Source` | заменить тело ядра ноды или код Compute Source — как HLSL Source Editor: с отменой, граф перекомпилируется; ошибки шейдера — при запуске графа (`PCGToolset.ExecuteGraphInstance`); потом сохранить |
| `SetHLSLFunctions` | `Target`, `Functions` | заменить блок своих функций ноды Custom HLSL; потом сохранить |

Нода с **Kernel Source Override** исполняет код ассета — запись в саму ноду отклоняется с подсказкой править ассет.

Ноды из **любых модулей**: штатные `PCGToolset.ListNativeNodes` / `AddNode` знают только ноды пакета `/Script/PCG`,
ноды плагинов (Mesh Partition / Mesh Terrain, Geometry Script и др.) через них не поставить. Граф правится его же
функциями `AddNodeOfType` и `ForceNotificationForEditor` через `ProcessEvent` — тоже без зависимости от плагина PCG.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `ListNodeTypes` | `Filter` | только чтение: типы нод всех загруженных модулей — отображаемое имя класса, путь класса, `[stock]` у нод `/Script/PCG`; `Filter` — подстрока имени или пути без учёта регистра |
| `AddNodeByClass` | `Graph`, `NodeType`, `NodeName`, `NodeTitle`, `XPosition`, `YPosition` | добавить ноду по пути класса (например `/Script/PCGMeshPartitionInterop.PCGQuerySettings`), имени или отображаемому имени класса, с отменой; ответ — путь ноды и подписи пинов. Дальше как обычно: `PCGToolset.UpdateNode`, `ConnectNodePins`, сохранить граф |

## IluUnrealToolAgent.IluAssetEditorToolset

Управление **открытым** редактором ассета без кликов по интерфейсу: штатные тулсеты открывают редактор
(`EditorAppToolset.OpenEditorForAsset`), но не умеют выделить ноду в его графе, снять его окно превью или нажать кнопку
тулбара. Работает через Slate с любым редактором на штатных виджетах графа и вьюпорта (PCG, Procedural Vegetation,
Blueprint, Material), без зависимости от их модулей. Редактор ассета должен быть открыт.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `SelectGraphNodes` | `Asset`, `Nodes`, `bFrame` | выделить ноды в графе (вместо текущего выделения), как кликом: в PCG / PVE превью переключается на выделенную ноду и пересчитывается асинхронно — перед снимком подождать несколько секунд. Имя — как в `PCGToolset.GetGraphStructure` (`Export_0`) или заголовок, без учёта регистра и `_`/пробела; не нашлось — в ответе список нод |
| `CapturePreview` | `Asset`, `FilePath`, `Yaw`, `Pitch`, `FOV` | PNG вьюпорта превью с ракурса Yaw/Pitch, кадр по видимому содержимому (пол и небо редактора не считаются). Рисует сам вьюпорт — свет и экспозиция как у пользователя; камера ставится на время снимка и возвращается, размер картинки — размер вьюпорта; `FOV` 0 — как во вьюпорте. Пустой путь — `<Проект>/Saved/IluUnrealTool/Captures/<Ассет>.png`. В ответе — габариты и текст оверлея вьюпорта (статистика PVE: Triangles, Foliage Instances) |
| `ExecuteEditorCommand` | `Asset`, `CommandContext`, `CommandName` | команда редактора, как её кнопка или клавиша: например `PCGEditor` / `ForceGraphRegeneration` (Force Regen) |
| `CloseAssetEditor` | `Asset` | закрыть редакторы ассета без вопроса о сохранении (правки остаются в памяти, ассет «грязный») — сначала сохранить |

## IluUnrealToolAgent.IluAssetToolset

Импорт файлов в любом формате, который умеет редактор: штатный `StaticMeshTools.import_file` берёт только FBX и OBJ.

| Инструмент | Аргументы | Что делает |
|---|---|---|
| `ImportFile` | `SourceFile`, `DestinationPath`, `DestinationName`, `bReplaceExisting` | импорт файла с диска с настройками по умолчанию, без диалога — как перетаскивание в Content Browser (`UAssetImportTask`): USD через Interchange (статические и скелетные меши, Nanite Assembly, материалы), FBX, glTF, текстуры; из одного файла может получиться несколько ассетов, в ответе — класс и путь каждого; потом сохранить |

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
