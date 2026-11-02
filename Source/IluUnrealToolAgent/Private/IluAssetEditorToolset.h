#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "IluAssetEditorToolset.generated.h"

/**
 * Driving an OPEN asset editor from MCP without clicking its UI: selecting graph nodes (in PCG / Procedural Vegetation
 * editors the selected node drives the preview), capturing the editor's preview viewport from a chosen angle, running
 * editor commands (e.g. Force Regen) and closing the editor. Works with any asset editor built on the standard
 * graph editor and editor viewport widgets, through Slate, without depending on the editors' modules.
 * Управление ОТКРЫТЫМ редактором ассета через MCP без кликов по интерфейсу: выделение нод графа (в редакторах PCG и
 * Procedural Vegetation выделенная нода определяет превью), снимок окна превью с заданного ракурса, команды редактора
 * (например Force Regen) и закрытие редактора. Подходит любому редактору на штатных виджетах графа и вьюпорта,
 * работает через Slate, без зависимости от модулей самих редакторов.
 */
UCLASS()
class UIluAssetEditorToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Selects nodes in the graph editor of the asset's open editor (replacing the current selection), as a click would:
	 * the editor reacts to the new selection, e.g. the PCG / Procedural Vegetation preview switches to the selected node.
	 * The preview is regenerated asynchronously — wait a few seconds before CapturePreview. Unknown names return the list
	 * of the graph's nodes.
	 * Выделяет ноды в графе открытого редактора ассета (вместо текущего выделения), как клик: редактор реагирует на
	 * выделение, например превью PCG / Procedural Vegetation переключается на выделенную ноду. Превью пересчитывается
	 * асинхронно — перед CapturePreview подождать несколько секунд. Если имя не найдено, в ответе список нод графа.
	 * @param Asset The asset whose editor is open (EditorAppToolset.OpenEditorForAsset), e.g. a PCG graph or a Procedural Vegetation asset.
	 * @param Nodes Node names (as in PCGToolset.GetGraphStructure, e.g. Export_0) or display titles; case and '_' vs ' ' are ignored.
	 * @param bFrame Also scroll the graph view to the first selected node.
	 * @return "OK: ..." with the selected nodes, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluAssetEditorToolset")
	static FString SelectGraphNodes(UObject* Asset, const TArray<FString>& Nodes, bool bFrame);

	/**
	 * Saves a PNG of the 3D preview viewport of the asset's open editor (e.g. the PCG / Procedural Vegetation preview) seen
	 * from Yaw/Pitch and framed to the visible preview content (editor floor and sky are skipped). The viewport itself
	 * renders the image, so lighting, exposure and show flags match what the user sees; its camera is moved for the shot
	 * and restored. The image has the viewport size. Also returns the viewport overlay text (e.g. the PVE stats:
	 * Triangles, Foliage Instances).
	 * Сохраняет PNG 3D-вьюпорта превью открытого редактора ассета (например превью PCG / Procedural Vegetation) с ракурса
	 * Yaw/Pitch, кадр — по видимому содержимому превью (пол и небо редактора не учитываются). Рисует сам вьюпорт, поэтому
	 * свет, экспозиция и флаги показа — как видит пользователь; камера переставляется на время снимка и возвращается.
	 * Размер картинки — размер вьюпорта. Также возвращает текст оверлея вьюпорта (например статистику PVE: Triangles,
	 * Foliage Instances).
	 * @param Asset The asset whose editor is open; the viewport must be visible and in Perspective.
	 * @param FilePath Absolute .png path; empty saves to <Project>/Saved/IluUnrealTool/Captures/<Asset>.png. Overwritten.
	 * @param Yaw Camera yaw in degrees: 0 looks along +X, 90 along +Y.
	 * @param Pitch Camera pitch in degrees: negative looks down (e.g. -10), 0 is level.
	 * @param FOV Horizontal field of view in degrees for the shot, 0 keeps the viewport's.
	 * @return "OK: ..." with the file path, image size, framed bounds and overlay text, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluAssetEditorToolset")
	static FString CapturePreview(UObject* Asset, const FString& FilePath, float Yaw, float Pitch, float FOV);

	/**
	 * Runs a UI command of the asset's open editor by its binding context and name, as its toolbar button or shortcut
	 * would, e.g. PCGEditor / ForceGraphRegeneration (Force Regen in PCG and Procedural Vegetation editors).
	 * Выполняет команду открытого редактора ассета по контексту и имени — как её кнопка или горячая клавиша, например
	 * PCGEditor / ForceGraphRegeneration (Force Regen в редакторах PCG и Procedural Vegetation).
	 * @param Asset The asset whose editor is open.
	 * @param CommandContext Binding context name of the command set, e.g. PCGEditor.
	 * @param CommandName Command name inside the context, e.g. ForceGraphRegeneration.
	 * @return "OK: ..." or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluAssetEditorToolset")
	static FString ExecuteEditorCommand(UObject* Asset, const FString& CommandContext, const FString& CommandName);

	/**
	 * Closes all editors open for the asset without a save prompt: unsaved changes stay in memory (the asset stays dirty),
	 * save it first if needed (AssetTools.save_assets).
	 * Закрывает все редакторы ассета без вопроса о сохранении: несохранённые правки остаются в памяти (ассет остаётся
	 * изменённым), при нужде сначала сохранить.
	 * @param Asset The asset whose editors to close.
	 * @return "OK: ..." with the number of closed editors, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluAssetEditorToolset")
	static FString CloseAssetEditor(UObject* Asset);
};
