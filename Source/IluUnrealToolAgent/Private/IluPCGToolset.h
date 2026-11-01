#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "IluPCGToolset.generated.h"

/**
 * PCG helpers for MCP that the stock PCGToolset lacks: reading and writing the HLSL code of Custom HLSL nodes and PCG
 * Compute Source assets, listing and placing nodes from any module (the stock AddNode only knows /Script/PCG nodes).
 * Works through reflection, so the plugin does not depend on the PCG plugin.
 * Помощники PCG через MCP — то, чего нет в штатном PCGToolset: чтение и запись HLSL-кода нод Custom HLSL и ассетов
 * PCG Compute Source, список и установка нод из любого модуля (штатный AddNode знает только ноды /Script/PCG).
 * Работает через рефлексию, поэтому плагин не зависит от плагина PCG.
 */
UCLASS()
class UIluPCGToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Read-only: returns the HLSL code of a Custom HLSL node (kernel source and custom functions) or of a PCG Compute Source
	 * asset. With bIncludeDeclarations the generated read-only declarations are appended: the accessor functions the code
	 * may call for each pin (e.g. Landscape_GetHeight, Out_SetPosition) and the helper functions.
	 * Только чтение: HLSL-код ноды Custom HLSL (тело ядра и свои функции) или ассета PCG Compute Source. С bIncludeDeclarations
	 * добавляются сгенерированные объявления (только чтение): функции доступа к данным каждого пина (например
	 * Landscape_GetHeight, Out_SetPosition) и вспомогательные функции.
	 * @param Target The PCG node (refPath from PCGToolset.GetGraphStructure, e.g. /Game/Folder/Graph.Graph:CustomHLSL_0),
	 *        its settings object, or a PCG Compute Source asset.
	 * @param bIncludeDeclarations Also return the declarations (Custom HLSL nodes only).
	 * @return "OK: ..." with sections === Source ===, === Functions ===, === Declarations === or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluPCGToolset")
	static FString GetHLSLSource(UObject* Target, bool bIncludeDeclarations);

	/**
	 * Replaces the kernel source of a Custom HLSL node (the body run per element) or the source of a PCG Compute Source
	 * asset, as the HLSL Source Editor does: undoable, the graph is refreshed and recompiled. Shader errors show up when
	 * the graph runs (PCGToolset.ExecuteGraphInstance). Save the graph or asset afterwards (AssetTools.save_assets).
	 * Заменяет тело ядра ноды Custom HLSL (код на каждый элемент) или код ассета PCG Compute Source — как HLSL Source
	 * Editor: с отменой, граф обновляется и перекомпилируется. Ошибки шейдера видны при запуске графа
	 * (PCGToolset.ExecuteGraphInstance). После — сохранить граф или ассет.
	 * @param Target The PCG node, its settings object, or a PCG Compute Source asset.
	 * @param Source The new HLSL code.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluPCGToolset")
	static FString SetHLSLSource(UObject* Target, const FString& Source);

	/**
	 * Replaces the custom functions block of a Custom HLSL node (functions the kernel source may call), as the HLSL Source
	 * Editor does: undoable, the graph is refreshed and recompiled. Save the graph afterwards (AssetTools.save_assets).
	 * Заменяет блок своих функций ноды Custom HLSL (функции, которые зовёт тело ядра) — как HLSL Source Editor: с отменой,
	 * граф обновляется и перекомпилируется. После — сохранить граф.
	 * @param Target The Custom HLSL node or its settings object.
	 * @param Functions The new HLSL functions code.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluPCGToolset")
	static FString SetHLSLFunctions(UObject* Target, const FString& Functions);

	/**
	 * Read-only: lists the PCG node types of all loaded modules, including plugin nodes that the stock
	 * PCGToolset.ListNativeNodes / AddNode do not see (e.g. Mesh Partition, Geometry Script interop). One line per type:
	 * class display name, class path, [stock] for /Script/PCG nodes.
	 * Только чтение: типы PCG-нод всех загруженных модулей, включая ноды плагинов, которых не видят штатные
	 * PCGToolset.ListNativeNodes / AddNode (например Mesh Partition, Geometry Script). Строка на тип: отображаемое имя
	 * класса, путь класса, [stock] у нод /Script/PCG.
	 * @param Filter Case-insensitive substring of the display name or class path; empty lists everything.
	 * @return "OK: N node types" followed by the list, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluPCGToolset")
	static FString ListNodeTypes(const FString& Filter);

	/**
	 * Adds a node of any PCG settings class to a graph, undoable. Then set its parameters with PCGToolset.UpdateNode and
	 * wire it with PCGToolset.ConnectNodePins as usual, and save the graph (AssetTools.save_assets).
	 * Добавляет в граф ноду любого класса настроек PCG, с отменой. Дальше как обычно: параметры — PCGToolset.UpdateNode,
	 * связи — PCGToolset.ConnectNodePins, потом сохранить граф.
	 * @param Graph The PCG graph asset.
	 * @param NodeType Settings class path from ListNodeTypes (e.g. /Script/PCGMeshPartitionInterop.PCGQuerySettings),
	 *        a class name, or a class display name.
	 * @param NodeName Unique node name in the graph (used in the node path); empty keeps the generated name.
	 * @param NodeTitle Display title; empty keeps the default title.
	 * @param XPosition X position in the graph editor (a node is about 200 wide).
	 * @param YPosition Y position in the graph editor (a node is about 100 high).
	 * @return "OK: ..." with the node path and its pin labels, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluPCGToolset")
	static FString AddNodeByClass(UObject* Graph, const FString& NodeType, const FString& NodeName, const FString& NodeTitle,
		int32 XPosition, int32 YPosition);
};
