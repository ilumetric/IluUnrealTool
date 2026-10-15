#include "IluEditorNotify.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

void FIluEditorNotify::Show(const FText& Text, bool bSuccess, float ExpireSeconds)
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = ExpireSeconds > 0.f ? ExpireSeconds : (bSuccess ? 6.f : 5.f);
	Info.bUseSuccessFailIcons = true;
	const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(bSuccess ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
	}
}
