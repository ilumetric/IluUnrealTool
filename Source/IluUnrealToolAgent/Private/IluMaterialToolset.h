#pragma once

#include "CoreMinimal.h"
#include "ToolsetRegistry/ToolsetDefinition.h"

#include "IluMaterialToolset.generated.h"

class UCurveLinearColor;
class UMaterialExpression;

/**
 * Material graph helpers for MCP that the stock MaterialTools lack.
 * Помощники для графа материала через MCP — то, чего нет в штатном MaterialTools.
 */
UCLASS()
class UIluMaterialToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Links a Named Reroute Usage node to the Named Reroute Declaration with the given name in the same Material
	 * or MaterialFunction (MaterialTools can create both nodes but cannot link them). Recompile the material afterwards.
	 * Связывает ноду Named Reroute Usage с объявлением Named Reroute с этим именем в том же материале или функции
	 * (MaterialTools создаёт обе ноды, но связать не может). После — перекомпилировать материал.
	 * @param Usage The Named Reroute Usage expression created with MaterialTools.add_expression.
	 * @param DeclarationName The Name property of the Named Reroute Declaration to link to.
	 * @return "OK" or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMaterialToolset")
	static FString LinkNamedRerouteUsage(UMaterialExpression* Usage, FName DeclarationName);

	/**
	 * Sets the position and size of a Comment box in a material graph (MaterialTools cannot set the size).
	 * Задаёт позицию и размер рамки Comment в графе материала (MaterialTools не умеет задавать размер).
	 * @param Comment The Comment expression created with MaterialTools.add_expression.
	 * @param X Left edge in graph units.
	 * @param Y Top edge in graph units.
	 * @param SizeX Width in graph units.
	 * @param SizeY Height in graph units.
	 * @return "OK" or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMaterialToolset")
	static FString SetCommentBounds(UMaterialExpression* Comment, int32 X, int32 Y, int32 SizeX, int32 SizeY);

	/**
	 * Replaces all keys of a Linear Color Curve with evenly spaced colors over time 0..1 (atlases using it update).
	 * Заменяет все ключи цветовой кривой (Linear Color Curve) цветами, равномерно по времени 0..1 (атласы обновятся).
	 * @param Curve The CurveLinearColor asset.
	 * @param Colors Colors in linear space, from time 0 to time 1 (at least 1).
	 * @param bStepped True — flat color bands (constant keys at i/N); false — smooth linear gradient (keys at i/(N-1)).
	 * @return "OK" or an error description.
	 */
	UFUNCTION(meta = (AICallable), Category = "IluMaterialToolset")
	static FString SetLinearColorCurveKeys(UCurveLinearColor* Curve, const TArray<FLinearColor>& Colors, bool bStepped);
};
