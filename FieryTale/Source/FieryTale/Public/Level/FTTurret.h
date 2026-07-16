#pragma once

#include "CoreMinimal.h"
#include "Level/FTTowerBase.h"
#include "FTTurret.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class UGeometryCollectionComponent;
class AFT_ProjectileBase;
class UGameplayEffect;
class UDataTable;
class USoundBase;
class UParticleSystem;

UENUM(BlueprintType)
enum class EFTTurretTeam : uint8
{
	BlueTeam,
	RedTeam
};

UENUM(BlueprintType)
enum class EFTTurretPosition : uint8
{
	NonePosition,
	Left,
	Right
};

UCLASS()
class FIERYTALE_API AFTTurret : public AFTTowerBase
{
	GENERATED_BODY()

public:
	AFTTurret();

	EFTTurretTeam GetTurretTeam() const { return TurretTeam; }
	EFTTurretPosition GetTurretPosition() const { return TurretPosition; }

protected:
	virtual void BeginPlay() override;

	virtual FGameplayTag GetTeamTag() const override;
	virtual FGameplayTag GetStructureTag() const override;
	virtual float GetDefaultMaxHealth() const override { return 1000.f; }

	virtual void PerformDestructionEffects() override;
	virtual void NotifyGameMode() override;

	void TrackNearestEnemy();
	void FireProjectile();

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_SyncProjectileHoming(AFT_ProjectileBase* Projectile, AActor* Target);

	UFUNCTION(BlueprintImplementableEvent, Category = "Turret")
	void OnTurretDestroyed();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> CollisionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> LaserMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UGeometryCollectionComponent> DebrisMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	TObjectPtr<UDataTable> TurretDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Data")
	FName DataRowName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<AFT_ProjectileBase> ProjectileClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	TSubclassOf<UGameplayEffect> AttackDamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	EFTTurretTeam TurretTeam;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turret Property")
	EFTTurretPosition TurretPosition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<USoundBase> DestructionSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visual")
	TObjectPtr<UParticleSystem> DestructionEffect;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float DetectionRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Setup")
	float AttackCooldown;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Setup")
	float HomingAcceleration;

	UPROPERTY(BlueprintReadOnly, Category = "Setup")
	TObjectPtr<AActor> CurrentTarget;

	UPROPERTY()
	TObjectPtr<APawn> TurretDummyInstigator;

	FTimerHandle TargetTrackingTimerHandle;
	FTimerHandle AttackTimerHandle;
	bool bCanAttack;
};