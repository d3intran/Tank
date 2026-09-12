#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "BattleGameMode.generated.h"

class ATankPlayerState;

/**
 * 坦克 FFA 对战 GameMode：每个进场玩家自动生成一辆 TankPawn。
 * M0 出生骨架 → M2 战斗闭环 → M3 计分 / 先到 K 杀胜利 / 回合重置。
 *
 * 职责边界：GameMode 只在服务器存在，是「规则裁判」——击杀计分、胜负判定、回合重开都在这。
 * 需要让客户端看到的数据（计分板、胜负面板）分别放 PlayerState / GameState 复制过去。
 */
UCLASS(config = Game)
class TANK_API ABattleGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ABattleGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	/** 每个玩家用哪种坦克：Config 里的 bUseChaosVehicle 决定。
	 *  **必须用这个钩子而不是在 BeginPlay 里改 DefaultPawnClass** —— 实测单人 PIE 下
	 *  PostLogin（首台车生成）发生在 BeginPlay **之前**，那时改已经晚了（拿到的是旧 Pawn）。 */
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

	// 必须覆盖为 false，否则 FFA 选点在重生时根本不会执行（见 .cpp 注释）
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

	/**
	 * 击杀结算入口。由 ATankPawn::HandleDeath 在服务器调用。
	 * @param Killer 击杀者 Controller，可能为 null（无归属）或与 Victim 相同（自杀）
	 * @param Victim 被击杀者 Controller
	 */
	void NotifyKill(AController* Killer, AController* Victim);

	FORCEINLINE bool IsMatchOver() const { return bMatchOver; }

protected:
	virtual void BeginPlay() override;
	virtual APawn* SpawnDefaultPawnFor_Implementation(AController* NewPlayer, AActor* StartSpot) override;
	void EnsureAllPlayersHavePawns();

	/** 有人达到 KillsToWin → 宣告回合结束并安排重开。
	 *  注意：不能叫 EndMatch——AGameMode 已有 `virtual void EndMatch()`（引擎的 MatchState 状态机：
	 *  InProgress → WaitingPostMatch）。同名会隐藏基类虚函数（C4263/C4264），既是维护陷阱，
	 *  也让「回合结束」的语义和引擎那套混在一起。
	 *  本项目灰盒阶段刻意不走引擎 MatchState/RestartGame：RestartGame 会触发地图重载，
	 *  对「原地重开一局」太重；M4 若要做完整回合流程（含回大厅）可切回引擎那套。 */
	void DeclareWinner(ATankPlayerState* Winner);

	/** 回合重置：清零战绩 + 全体回炉重生，复用已验证的占有巡检链路 */
	void ResetRound();

	/** 先到多少杀获胜（默认 10，可在 Config/DefaultGame.ini 的
	 *  [/Script/Tank.BattleGameMode] 段里改，不用重编） */
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Rules", meta = (ClampMin = "1"))
	int32 KillsToWin = 10;

	/** 回合结束后多久自动重开（秒），同样可在 ini 里调 */
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Rules", meta = (ClampMin = "0.0", Units = "s"))
	float RoundRestartDelay = 5.0f;

	/** 用 Chaos 载具坦克（ATankVehicle）还是手写地形跟随的 ATankPawn。
	 *  在 Config/DefaultGame.ini 的 [/Script/Tank.BattleGameMode] 段改：
	 *      bUseChaosVehicle=True
	 *  Chaos 载具的联机移动同步与炮塔同步已全面就绪并通过实机三开对战验证。 */
	UPROPERTY(Config, EditDefaultsOnly, BlueprintReadOnly, Category = "Tank|Vehicle")
	bool bUseChaosVehicle = true;

	/** 本局是否已分出胜负（防止同帧多人达标时重复结算） */
	bool bMatchOver = false;

	FTimerHandle HealTimerHandle;
	FTimerHandle RoundResetTimerHandle;
};
