#include "IluMaterialToolset.h"
#include "IluUnrealToolCore.h"

#include "Curves/CurveLinearColor.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialFunction.h"

namespace IluMaterialReroute
{
	template <typename ExpressionsViewType>
	static UMaterialExpressionNamedRerouteDeclaration* FindDeclaration(const ExpressionsViewType& Expressions, FName Name)
	{
		for (UMaterialExpression* Expression : Expressions)
		{
			UMaterialExpressionNamedRerouteDeclaration* Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(Expression);
			if (Declaration && Declaration->Name == Name)
			{
				return Declaration;
			}
		}
		return nullptr;
	}
}

FString UIluMaterialToolset::LinkNamedRerouteUsage(UMaterialExpression* Usage, FName DeclarationName)
{
	UMaterialExpressionNamedRerouteUsage* RerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(Usage);
	if (!RerouteUsage)
	{
		return TEXT("Error: Usage is not a Named Reroute Usage expression.");
	}

	// Нода живёт внутри материала или функции материала — ищем объявление там же.
	UMaterialExpressionNamedRerouteDeclaration* Declaration = nullptr;
	UObject* Owner = RerouteUsage->GetOuter();
	if (UMaterial* Material = Cast<UMaterial>(Owner))
	{
		Declaration = IluMaterialReroute::FindDeclaration(Material->GetExpressions(), DeclarationName);
	}
	else if (UMaterialFunction* Function = Cast<UMaterialFunction>(Owner))
	{
		Declaration = IluMaterialReroute::FindDeclaration(Function->GetExpressions(), DeclarationName);
	}
	else
	{
		return TEXT("Error: Usage is not inside a Material or MaterialFunction.");
	}

	if (!Declaration)
	{
		UE_LOG(LogIluUnrealTool, Warning, TEXT("%s: нет объявления Named Reroute «%s»."), *GetNameSafe(Owner), *DeclarationName.ToString());
		return FString::Printf(TEXT("Error: no Named Reroute Declaration named '%s' in %s."), *DeclarationName.ToString(), *GetNameSafe(Owner));
	}

	RerouteUsage->Modify();
	RerouteUsage->Declaration = Declaration;
	RerouteUsage->DeclarationGuid = Declaration->VariableGuid;
	Owner->MarkPackageDirty();
	return TEXT("OK");
}

FString UIluMaterialToolset::SetCommentBounds(UMaterialExpression* Comment, int32 X, int32 Y, int32 SizeX, int32 SizeY)
{
	UMaterialExpressionComment* CommentBox = Cast<UMaterialExpressionComment>(Comment);
	if (!CommentBox)
	{
		return TEXT("Error: expression is not a Comment.");
	}

	CommentBox->Modify();
	CommentBox->MaterialExpressionEditorX = X;
	CommentBox->MaterialExpressionEditorY = Y;
	CommentBox->SizeX = FMath::Max(SizeX, 64);
	CommentBox->SizeY = FMath::Max(SizeY, 64);
	CommentBox->MarkPackageDirty();
	return TEXT("OK");
}

FString UIluMaterialToolset::SetLinearColorCurveKeys(UCurveLinearColor* Curve, const TArray<FLinearColor>& Colors, bool bStepped)
{
	if (!Curve)
	{
		return TEXT("Error: Curve is null.");
	}
	if (Colors.Num() == 0)
	{
		return TEXT("Error: Colors is empty.");
	}

	Curve->Modify();
	const int32 Num = Colors.Num();
	const ERichCurveInterpMode InterpMode = bStepped ? RCIM_Constant : RCIM_Linear;
	for (int32 Channel = 0; Channel < 4; ++Channel)
	{
		FRichCurve& Channels = Curve->FloatCurves[Channel];
		Channels.Reset();
		for (int32 Index = 0; Index < Num; ++Index)
		{
			// Ступенчато: каждая краска занимает равную полосу [i/N, (i+1)/N). Плавно: ключи на концах 0 и 1.
			const float Time = bStepped ? float(Index) / float(Num) : (Num > 1 ? float(Index) / float(Num - 1) : 0.f);
			const FLinearColor& Color = Colors[Index];
			const float Value = Channel == 0 ? Color.R : Channel == 1 ? Color.G : Channel == 2 ? Color.B : Color.A;
			const FKeyHandle Key = Channels.AddKey(Time, Value);
			Channels.SetKeyInterpMode(Key, InterpMode);
		}
	}

	// PostEditChange рассылает OnUpdateCurve — атласы с этой кривой перестраивают текстуру.
	Curve->PostEditChange();
	Curve->MarkPackageDirty();
	return TEXT("OK");
}
