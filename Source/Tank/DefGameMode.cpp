#include "DefGameMode.h"
#include "Tank.h"
#include "TankPawn.h"
#include "DefHUD.h"
#include "DefWall.h"
#include "Zombie.h"
#include "ZombieSpawner.h"
#include "WallHealth.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ADefGameMode::ADefGameMode()
{
	PrimaryActorTick.bCanEverTick = false;

	// 灰盒默认玩家用现有坦克；关卡 WorldSettings 的 GameModeOverride 指到本类即生效
	DefaultPawnClass = ATankPawn::StaticClass();
	HUDClass = ADefHUD::StaticClass();
}

void ADefGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 场景注册：通用发现任意宿主上的 WallHealth（灰盒方块墙或真实城门皆可）
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (UWallHealthComponent* Wall = It->FindComponentByClass<UWallHealthComponent>())
		{
			CachedWallHealth = Wall;
			break;
		}
	}
	for (TActorIterator<AZombieSpawner> It(GetWorld()); It; ++It)
	{
		CachedSpawner = *It;
		break;
	}

	if (!CachedWallHealth.IsValid())
	{
		UE_LOG(LogTank, Warning, TEXT("[DefGameMode] 场上无 ADefWall，胜负判定不生效"));
	}
	if (!CachedSpawner.IsValid())
	{
		UE_LOG(LogTank, Warning, TEXT("[DefGameMode] 场上无 AZombieSpawner，无法判胜"));
	}

	GetWorldTimerManager().SetTimer(MatchCheckHandle, this,
		&ADefGameMode::CheckMatch, 0.5f, true, 2.0f);
}

void ADefGameMode::CheckMatch()
{
	if (MatchState != EDefMatchState::InProgress)
	{
		RestartCountdown -= 0.5f;
		if (RestartCountdown <= 0.0f)
		{
			UGameplayStatics::OpenLevel(this, FName(*GetWorld()->GetName()));
		}
		return;
	}

	LastAliveZombieCount = CountAliveZombies();

	// 输：城墙血量归零
	if (CachedWallHealth.IsValid() && CachedWallHealth->IsDepleted())
	{
		EndMatch(EDefMatchState::Lost);
		return;
	}

	// 赢：配额刷完 + 场上无活丧尸
	if (CachedSpawner.IsValid() && CachedSpawner->IsFinished() && LastAliveZombieCount == 0)
	{
		EndMatch(EDefMatchState::Won);
	}
}

void ADefGameMode::EndMatch(EDefMatchState Outcome)
{
	MatchState = Outcome;
	RestartCountdown = RestartDelay;
	UE_LOG(LogTank, Warning, TEXT("[DefGameMode] 比赛结束：%s，%.0f 秒后重开"),
		Outcome == EDefMatchState::Won ? TEXT("守住") : TEXT("陷落"), RestartDelay);
}

int32 ADefGameMode::CountAliveZombies() const
{
	int32 Count = 0;
	for (TActorIterator<AZombie> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			++Count;
		}
	}
	return Count;
}
