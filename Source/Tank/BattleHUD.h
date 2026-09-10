#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BattleHUD.generated.h"

class ATankPawn;
class UTankHealth;

/**
 * 坦克 FFA 灰盒 HUD（Canvas 直绘，零资产）：
 * - 左下角自己的血条：>50% 绿 / 25~50% 黄 / <25% 红 + 呼吸频率闪烁
 * - 其他玩家坦克头顶血条：恒红
 * 血量数据源 = TankHealth 的复制值，全端一致
 */
UCLASS()
class TANK_API ABattleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void DrawHealthBar(float HealthRatio, float X, float Y, float W, float H, FLinearColor FillColor);

	// 自己血条尺寸（像素）
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FVector2D OwnBarSize = FVector2D(340.0f, 24.0f);

	// 头顶血条尺寸（像素）
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FVector2D OverheadBarSize = FVector2D(110.0f, 10.0f);
};
