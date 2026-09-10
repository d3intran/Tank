#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "BattleGameMode.generated.h"

/**
 * 坦克 FFA 对战 GameMode（M0 骨架）：每个进场玩家自动生成一辆 TankPawn。
 * 计分（PlayerState.Kills）/ 先到 K 杀胜利 / 重生保护在 M3 扩展。
 */
UCLASS()
class TANK_API ABattleGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	ABattleGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
};
