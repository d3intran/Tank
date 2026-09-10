#include "ZombieSpawner.h"
#include "Tank.h"
#include "Zombie.h"
#include "Components/SceneComponent.h"

AZombieSpawner::AZombieSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// 默认本类，蓝图子类可换
	ZombieClass = AZombie::StaticClass();
}

void AZombieSpawner::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetWall)
	{
		UE_LOG(LogTank, Error, TEXT("[Spawner] %s 未设置 TargetWall，无法刷怪"), *GetName());
		return;
	}

	RemainingToSpawn = TotalToSpawn;
	if (bAutoStart)
	{
		GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AZombieSpawner::StartWave));
	}
}

void AZombieSpawner::StartWave()
{
	UE_LOG(LogTank, Log, TEXT("[Spawner] 尸潮开始：共 %d 只，间隔 %.1fs"), RemainingToSpawn, SpawnInterval);
	GetWorldTimerManager().SetTimer(SpawnTimerHandle, this,
		&AZombieSpawner::SpawnOne, SpawnInterval, true, 0.0f);
}

void AZombieSpawner::SpawnOne()
{
	if (RemainingToSpawn <= 0 || !TargetWall)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 出生位姿：出生点 XY 抖动 + 朝向目标墙
	FVector SpawnLoc = GetActorLocation() + FVector(
		FMath::FRandRange(-SpawnJitterExtent, SpawnJitterExtent),
		FMath::FRandRange(-SpawnJitterExtent, SpawnJitterExtent),
		0.0f);
	FVector FaceDir = TargetWall->GetActorLocation() - SpawnLoc;
	FaceDir.Z = 0.0f;
	const FTransform SpawnTransform(FaceDir.IsNearlyZero()
		? FRotator::ZeroRotator
		: FRotator(0.0f, FaceDir.Rotation().Yaw, 0.0f), SpawnLoc);

	// Deferred 生成：先注入目标墙再 BeginPlay，保证丧尸缓存到墙血量组件
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	UClass* ClassToSpawn = ZombieClass.Get();
	if (!ClassToSpawn)
	{
		ClassToSpawn = AZombie::StaticClass();
	}
	AZombie* Zombie = World->SpawnActorDeferred<AZombie>(
		ClassToSpawn,
		SpawnTransform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (Zombie)
	{
		Zombie->SetTargetWall(TargetWall.Get());
		Zombie->FinishSpawning(SpawnTransform);
	}

	--RemainingToSpawn;
	if (RemainingToSpawn <= 0)
	{
		GetWorldTimerManager().ClearTimer(SpawnTimerHandle);
		UE_LOG(LogTank, Log, TEXT("[Spawner] 尸潮刷完"));
	}
}
