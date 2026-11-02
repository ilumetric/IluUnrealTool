#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "IluAssetToolset.generated.h"

/**
 * Asset import for MCP in any format the editor can import (USD, FBX, glTF, textures, ...): the stock
 * StaticMeshTools.import_file only takes FBX and OBJ. Goes through the editor's own import (UAssetImportTask), so the
 * right factory or Interchange translator is picked by the file extension and nothing asks for a dialog.
 * Импорт ассетов через MCP в любом формате, который умеет редактор (USD, FBX, glTF, текстуры, ...): штатный
 * StaticMeshTools.import_file берёт только FBX и OBJ. Идёт через штатный импорт редактора (UAssetImportTask): фабрика
 * или транслятор Interchange выбирается по расширению файла, диалогов нет.
 */
UCLASS()
class UIluAssetToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Imports a file into the Content Browser with the default import settings and no dialog, like dragging it into the
	 * Content Browser. A USD stage is imported through Interchange (static or skeletal meshes, Nanite assemblies,
	 * materials, ...); one file may create several assets. The assets are not saved — save them afterwards
	 * (AssetTools.save_assets).
	 * Импортирует файл в Content Browser с настройками по умолчанию и без диалога — как перетаскивание файла в Content
	 * Browser. USD-сцена импортируется через Interchange (статические и скелетные меши, Nanite Assembly, материалы, ...);
	 * из одного файла может получиться несколько ассетов. Ассеты не сохраняются — после сохранить (AssetTools.save_assets).
	 * @param SourceFile Absolute path to the file on disk (forward or back slashes).
	 * @param DestinationPath Content Browser folder, e.g. /Game/Art/Trees.
	 * @param DestinationName Asset name for single-asset imports; empty keeps the name from the file.
	 * @param bReplaceExisting Overwrite assets with the same name (reimport over them) instead of failing.
	 * @return "OK: ..." with the class and path of every imported asset, or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluAssetToolset")
	static FString ImportFile(const FString& SourceFile, const FString& DestinationPath, const FString& DestinationName, bool bReplaceExisting);
};
