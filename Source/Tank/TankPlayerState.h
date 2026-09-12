#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "TankPlayerState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTankScoreChanged);

/**
 * FFA 计分用 PlayerState。
 *
 * 为什么必须自己写：UE5 的 APlayerState 只剩 `Score`（float，复制），
 * 计划书里写的「PlayerState.Kills 引擎自带复制」是 UE4 之前的记忆——5.8 里
 * 引擎框架类（PlayerState / GameStateBase）**都没有** Kills/Deaths 字段。
 * 所以击杀数要自己加字段 + 自己写复制。
 *
 * 职责边界（面试常问）：PlayerState 挂「跨重生持续的玩家数据」，随 PlayerController
 * 存在而存在，因此坦克被打爆重生后击杀数不会丢——这正是计分必须放这里而不是放 Pawn 的原因。
 */
UCLASS()
class TANK_API ATankPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ATankPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 加一次击杀。仅服务器调用（击杀判定在服务端）。 */
	void AddKill();

	/** 加一次死亡。仅服务器调用。 */
	void AddDeath();

	/** 回合重置：清零战绩。仅服务器调用。 */
	void ResetScore();

	FORCEINLINE int32 GetKills() const { return Kills; }
	FORCEINLINE int32 GetDeaths() const { return Deaths; }

	/** 计分变化通知（本地 HUD 用；复制到达时也会触发） */
	UPROPERTY(BlueprintAssignable, Category = "Tank|Score")
	FOnTankScoreChanged OnScoreChanged;

protected:
	UFUNCTION()
	void OnRep_Kills();

	UFUNCTION()
	void OnRep_Deaths();

	// 击杀数：服务器权威，复制到所有端（HUD 计分板读这个）
	UPROPERTY(ReplicatedUsing = OnRep_Kills, BlueprintReadOnly, Category = "Tank|Score")
	int32 Kills = 0;

	// 死亡数：与击杀同源，便于后续做 K/D 显示与「死亡后掉分」等规则
	UPROPERTY(ReplicatedUsing = OnRep_Deaths, BlueprintReadOnly, Category = "Tank|Score")
	int32 Deaths = 0;
};
