#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DefHUD.generated.h"

/**
 * P1 灰盒 HUD：Canvas 直绘（零资产）。左上角战况（城墙HP/活丧尸数），
 * 结局时居中大字 + 重开倒计时。P3 换 UMG。
 */
UCLASS()
class TANK_API ADefHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
