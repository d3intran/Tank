#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "BattleHUD.generated.h"

class ATankPawn;
class ATankPlayerState;
class ATankGameState;
class UTankHealth;
class UFont;

/**
 * 坦克 FFA 灰盒 HUD（Canvas 直绘，零资产）。
 *
 * 数据源全部是**复制值**，所以每个客户端各自算出来的画面是一致的：
 * - 本机血条 / 装填进度 ← TankPawn + TankHealth
 * - 计分板 ← GameState->PlayerArray 里的 TankPlayerState（Kills/Deaths）
 * - 胜利面板 ← TankGameState（bMatchOver / WinnerName / KillsToWin）
 *
 * 布局：
 *   左下角  本机血条 + 装填进度 + 重生保护提示
 *   右上角  常驻计分板（按击杀降序，本机高亮）
 *   屏幕中央 本机阵亡/重生提示、回合结束的胜利面板
 *   世界中  其他玩家坦克头顶「玩家 ID + 血条」（被建筑遮挡时不画）
 */
UCLASS()
class TANK_API ABattleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** 底槽 + 填充的通用条 */
	void DrawBar(float Ratio, float X, float Y, float W, float H, FLinearColor FillColor);

	/** 左下角本机状态：血条（分段变色+低血呼吸）、装填进度、重生保护提示 */
	void DrawLocalStatus(ATankPawn* MyTank);

	/** 其他玩家坦克头顶「玩家 ID + 血条」（含 80m 距离剔除 + Visibility 通道遮挡剔除，避免穿墙看到） */
	void DrawOverheadBars(ATankPawn* MyTank);

	/** 头顶名牌：居中画在血条上方，带半透明底衬与描边（Canvas 直绘没有描边，亮背景会糊） */
	void DrawOverheadName(const FString& PlayerName, float CenterX, float BarTopY);

	/** 取坦克所属玩家的显示名（Controller→PlayerState，都是复制值，各端一致）；拿不到返回空串 */
	static FString ResolvePlayerName(const ATankPawn* Tank);

	/** 右上角常驻计分板 */
	void DrawScoreboard();

	/** 屏幕中央：本机无 Pawn 时的阵亡/重生提示 */
	void DrawDeathNotice();

	/** 屏幕中央：回合结束的胜利面板 */
	void DrawVictoryPanel();

	/** 居中绘制一行文字 */
	void DrawCenteredText(const FString& Text, float CenterY, const FLinearColor& Color, UFont* Font, float Scale);

	// 自己血条尺寸（像素）
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FVector2D OwnBarSize = FVector2D(340.0f, 24.0f);

	// 头顶血条尺寸（像素）
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FVector2D OverheadBarSize = FVector2D(110.0f, 10.0f);

	// 装填条尺寸（像素）
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	FVector2D ReloadBarSize = FVector2D(340.0f, 10.0f);

	// 计分板
	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ScoreboardWidth = 360.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ScoreboardRowHeight = 20.0f;

	UPROPERTY(EditDefaultsOnly, Category = "HUD")
	float ScoreboardMargin = 30.0f;
};
