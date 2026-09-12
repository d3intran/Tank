#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "TankGameState.generated.h"

/**
 * 对局级共享状态（回合是否结束 / 赢家 / 胜利所需击杀数）。
 *
 * 为什么不能直接放 GameMode：GameMode 只在服务器存在，客户端拿不到。
 * GameState 会自动复制到所有客户端，是「全局对局信息」的正确归属——
 * 这也正是面试里 PlayerState / PlayerController / GameState 三者职责的经典区分：
 *   - PlayerState：单个玩家的跨重生数据（击杀/死亡）
 *   - GameState：整局共享的状态（回合、赢家、人数）
 *   - GameMode：只在服务器的规则裁判
 */
UCLASS()
class TANK_API ATankGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ATankGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 由 GameMode 在服务器调用：宣告本局结束与赢家。 */
	void SetMatchOver(const FString& InWinnerName);

	/** 由 GameMode 在服务器调用：回合重置，清空胜负状态。 */
	void ClearMatchOver();

	/** 由 GameMode 在 BeginPlay 同步一次，让客户端 HUD 能显示「先到 N 杀」。 */
	void SetKillsToWin(int32 InKillsToWin);

	FORCEINLINE bool IsMatchOver() const { return bMatchOver; }
	FORCEINLINE const FString& GetWinnerName() const { return WinnerName; }
	FORCEINLINE int32 GetKillsToWin() const { return KillsToWin; }

protected:
	UFUNCTION()
	void OnRep_MatchOver();

	UPROPERTY(ReplicatedUsing = OnRep_MatchOver, BlueprintReadOnly, Category = "Tank|Match")
	bool bMatchOver = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tank|Match")
	FString WinnerName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Tank|Match")
	int32 KillsToWin = 10;
};
