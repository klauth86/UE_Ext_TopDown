// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "Actions/PawnAction.h"
#include "TopDownProjPlayerController.generated.h"

/** Forward declaration to improve compiling times */
class UNiagaraSystem;
class UCameraComponent;

UCLASS()
class UPawnAction_Falling : public UPawnAction
{
	GENERATED_BODY()

public:

	static UPawnAction_Falling* CreateAction(UWorld& World) { return UPawnAction::CreateActionInstance<UPawnAction_Falling>(World); }

protected:

	virtual bool Start() override;

	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;

	UFUNCTION()
		void OnLanded(const FHitResult& Hit);
};



UCLASS()
class UPawnAction_PrimaryMove : public UPawnAction
{
	GENERATED_UCLASS_BODY()

public:

	static UPawnAction_PrimaryMove* CreateAction(UWorld& World) { return UPawnAction::CreateActionInstance<UPawnAction_PrimaryMove>(World); }

protected:

	virtual bool Start() override;

	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;

	virtual void Tick(float DeltaTime) override;

protected:

	UPROPERTY()
		UCameraComponent* CameraComp;
};



UCLASS()
class UPawnAction_TargetMove : public UPawnAction
{
	GENERATED_UCLASS_BODY()

public:

	static UPawnAction_TargetMove* CreateAction(UWorld& World) { return UPawnAction::CreateActionInstance<UPawnAction_TargetMove>(World); }

protected:

	virtual bool Start() override;

	virtual EPawnActionAbortState::Type PerformAbort(EAIForceParam::Type ShouldForce) override;

	virtual void Tick(float DeltaTime) override;

protected:

	UPROPERTY()
		APlayerController* PlayerController;
};



UCLASS()
class ATopDownProjPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ATopDownProjPlayerController();

	UPawnActionsComponent* GetActionsComp() const { return ActionsComp; }

	/** Time Threshold to know if it was a short press */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
		float ShortPressThreshold;

	/** FX Class that we will spawn when clicking */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Input)
		UNiagaraSystem* FXCursor;

	static struct
	{
		float MoveX;

		float MoveY;

		uint8 bIsTouch : 1; // Is it a touch device

		uint8 bInputPressed : 1;; // Input is bring pressed

		uint8 bHasActivePrimaryMove : 1;

		uint8 bHasActiveTargetMove : 1;

	} SharedData;

protected:

	// Begin PlayerController interface
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	// End PlayerController interface

	void OnSetDestinationPressed() { SharedData.bInputPressed = 1; }
	void OnSetDestinationReleased() { SharedData.bInputPressed = 0; }
	
	void OnTouchPressed(const ETouchIndex::Type FingerIndex, const FVector Location)
	{
		SharedData.bIsTouch = true;
		OnSetDestinationPressed();
	}
	void OnTouchReleased(const ETouchIndex::Type FingerIndex, const FVector Location)
	{
		OnSetDestinationReleased();
		SharedData.bIsTouch = false;
	}

	void OnJump();

	void OnMoveX(float value) { SharedData.MoveX = value; }
	void OnMoveY(float value) { SharedData.MoveY = value; }

private:

	UPROPERTY(BlueprintReadOnly, Category = AI, meta = (AllowPrivateAccess = "true"))
		TObjectPtr<UPawnActionsComponent> ActionsComp;
};