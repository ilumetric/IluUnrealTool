#pragma once

#include "CoreMinimal.h"

class FMenuBuilder;

/**
 * Mesh operations on static meshes of the actors selected in the level ("..." toolbar panel).
 * The operations themselves live in Core (FIluMeshOperations) and are shared with the MCP toolset.
 * Операции над статик-мешами выделенных на уровне акторов (подпанель «...» на тулбаре).
 * Сами операции живут в Core (FIluMeshOperations) и общие с MCP-тулсетом.
 */
struct FIluMeshOpsMenu
{
	static constexpr int32 AORaysMin = 16;
	static constexpr int32 AORaysMax = 512;
	static constexpr float AODistanceMax = 1000.f;

	/** Adds the "Mesh Ops" section to the options menu / Добавляет секцию «Mesh Ops» в меню настроек. */
	static void AddSection(FMenuBuilder& MenuBuilder);

	static int32 GetAORays();
	static void SetAORays(int32 Rays);
	static float GetAODistance();
	static void SetAODistance(float Cm);
};
