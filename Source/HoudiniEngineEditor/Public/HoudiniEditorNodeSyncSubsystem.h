// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineEditorPrivatePCH.h"
#include "HoudiniInput.h"
#include "HoudiniNodeSyncComponent.h"

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Toolkits/AssetEditorModeUILayer.h"

#include "HoudiniEditorNodeSyncSubsystem.generated.h"

class USkeletalMesh;


USTRUCT()
struct HOUDINIENGINEEDITOR_API FHoudiniNodeSyncOptions
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FString FetchNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString SendNodePath = "/obj/UnrealContent";

	UPROPERTY()
	FString UnrealAssetName = "TestAsset";

	UPROPERTY()
	FString UnrealAssetFolder = "/Game/000";

	UPROPERTY()
	bool bUseOutputNodes = true;

	UPROPERTY()
	bool bFetchToWorld = false;

	UPROPERTY()
	FString UnrealActorName = "HoudiniNodeSyncActor";

	UPROPERTY()
	FString UnrealActorFolder = "/Houdini/NodeSync";

	UPROPERTY()
	bool bReplaceExisting = false;

	UPROPERTY()
	bool bOverwriteSkeleton = false;

	UPROPERTY()
	FString SkeletonAssetPath = "";	

	UPROPERTY()
	bool bAutoBake = false;
};



UCLASS()
class HOUDINIENGINEEDITOR_API UHoudiniEditorNodeSyncSubsystem : public UAssetEditorUISubsystem
{
	GENERATED_BODY()

public:

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	
	// Register layout for tab placement
	virtual void RegisterLayoutExtensions(FLayoutExtender& Extender) override;

public:

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendToHoudini(const TArray<UObject*>& SelectedAssets);

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void FetchFromHoudini();
	
	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendWorldSelection();

	UFUNCTION(BlueprintCallable, Category = "Houdini")
	void SendContentBrowserSelection(const TArray<UObject*>& SelectedAssets);

	bool CreateSessionIfNeeded();

	// Returns the color corresponding to a given node sync status
	static FLinearColor GetStatusColor(const EHoudiniNodeSyncStatus& Status);

	bool GetNodeSyncInput(UHoudiniInput*& OutInput);

	bool GatherAllFetchedNodeIds(
		HAPI_NodeId UnrealFetchNodeId,
		const bool bUseOutputNodes,
		TArray<HAPI_NodeId>& OutOutputNodes);

	bool ValidateFetchedNodePath(
		const FString& InFetchedNodePath,
		HAPI_NodeId& OutFetchedNodeId);

	//
	// Public Members
	//
	
	// Node sync options
	FHoudiniNodeSyncOptions NodeSyncOptions;

	// SEND status
	EHoudiniNodeSyncStatus LastSendStatus = EHoudiniNodeSyncStatus::None;
	FString SendStatusMessage;
	FString SendStatusDetails;

	// FETCH status
	EHoudiniNodeSyncStatus LastFetchStatus = EHoudiniNodeSyncStatus::None;
	FString FetchStatusMessage;
	FString FetchStatusDetails;

private:

	bool InitNodeSyncInputIfNeeded();

	UPROPERTY()
	UHoudiniInput* NodeSyncInput;

	UPROPERTY()
	TArray<UObject*> WorldSelection;

	UPROPERTY()
	TArray<UObject*> ContentBrowserSelection;


};
