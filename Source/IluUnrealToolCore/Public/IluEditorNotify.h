#pragma once

#include "CoreMinimal.h"

/**
 * Editor toast notifications with the success/fail icon.
 * Всплывающие уведомления редактора с иконкой успеха/ошибки.
 */
struct ILUUNREALTOOLCORE_API FIluEditorNotify
{
	/** Show a toast; ExpireSeconds <= 0 — default duration (6 s success, 5 s fail).
	 *  Показать уведомление; ExpireSeconds <= 0 — длительность по умолчанию (6 с успех, 5 с ошибка). */
	static void Show(const FText& Text, bool bSuccess, float ExpireSeconds = 0.f);
};
