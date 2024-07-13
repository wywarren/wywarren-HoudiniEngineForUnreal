/*
* Copyright (c) <2021> Side Effects Software Inc.
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
*    this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
*    promote products derived from this software without specific prior
*    written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE "AS IS" AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniSkeletalMeshUtils.h"

#include "HoudiniSkeletalMeshTranslator.h"

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineUtils.h"
#include "HoudiniGeoPartObject.h"
#include "HoudiniMeshTranslator.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Factories/FbxSkeletalMeshImportData.h"
#include "IMeshBuilderModule.h"
#include "ImportUtils/SkeletalMeshImportUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Math/UnrealMathUtility.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "Rendering/SkeletalMeshModel.h"
#include "ReferenceSkeleton.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 1
#include "Engine/SkinnedAssetCommon.h"
#endif
#include <functional>

#include "Chaos/ChaosPerfTest.h"
#include "Internationalization/Regex.h"

FMatrix
FHoudiniSkeletalMeshUtils::ConstructHoudiniMatrix(const float RotationData[], const float PositionData[])
{
	FMatrix M44Pose;  //this is unconverted houdini space, stored in an Unreal FMatrix.
	M44Pose.M[0][0] = RotationData[0];
	M44Pose.M[0][1] = RotationData[1];
	M44Pose.M[0][2] = RotationData[2];
	M44Pose.M[0][3] = 0;
	M44Pose.M[1][0] = RotationData[3];
	M44Pose.M[1][1] = RotationData[4];
	M44Pose.M[1][2] = RotationData[5];
	M44Pose.M[1][3] = 0;
	M44Pose.M[2][0] = RotationData[6];
	M44Pose.M[2][1] = RotationData[7];
	M44Pose.M[2][2] = RotationData[8];
	M44Pose.M[2][3] = 0;
	M44Pose.M[3][0] = PositionData[0];
	M44Pose.M[3][1] = PositionData[1];
	M44Pose.M[3][2] = PositionData[2];
	M44Pose.M[3][3] = 1;

	return M44Pose;
}

FTransform FHoudiniSkeletalMeshUtils::HoudiniToUnrealMatrix(const FMatrix& Matrix)
{
	FTransform Transform = FTransform(Matrix);

	FQuat PoseQ = Transform.GetRotation();
	FVector PoseT = Transform.GetLocation();
	FVector ConvertedPoseT = FVector(100.0f * PoseT.X, 100.0f * PoseT.Z, 100.0f * PoseT.Y);
	const FQuat ConvertedPoseQ = PoseQ * FQuat::MakeFromEuler({ 90.f, 0.f, 0.f });
	FVector PoseS = Transform.GetScale3D();

	Transform = FTransform(ConvertedPoseQ, ConvertedPoseT, PoseS);

	return Transform;
}

FHoudiniSkeleton FHoudiniSkeletalMeshUtils::ConstructSkeleton(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	FHoudiniSkeleton Result;

	// Houdini stores the bone names in the "name" attribute on the point. However, when this data is fetched HAPI
	// doesn't return just the name, it expands the points so there is one per vertex. So for now, we have to assign
	// an extra attribute to determine which preserves the bone number.

	TArray<FString> ParentChild;
	
	HAPI_AttributeInfo AttribInfo;

	bool bSuccess = FHoudiniEngineUtils::HapiGetAttributeDataAsString(NodeId, PartId, "name", AttribInfo, ParentChild, 0, HAPI_ATTROWNER_POINT);

	if (!bSuccess || ParentChild.IsEmpty())
	{
		HOUDINI_LOG_ERROR(TEXT("No __bone_id found on skeleton"));
		return {};
	}

	TArray<int> ParentChildBoneNumbers;

	bSuccess = FHoudiniEngineUtils::HapiGetAttributeDataAsInteger(NodeId, PartId, "__bone_id", AttribInfo, ParentChildBoneNumbers, 0, HAPI_ATTROWNER_POINT);
	if (!bSuccess || ParentChildBoneNumbers.Num() != ParentChild.Num())
	{
		HOUDINI_LOG_ERROR(TEXT("No __bone_id found on skeleton"));
		return {};
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Get all bone names and uses this to seed the skeleton
	//--------------------------------------------------------------------------------------------------------------------

	TArray<FString> BoneNames;
	TMap<FString, int> BoneNamesToPointIndex;
	for(int Index = 0; Index < ParentChild.Num(); Index++)
	{
		BoneNamesToPointIndex.Add(ParentChild[Index], Index);
	}

	BoneNamesToPointIndex.GetKeys(BoneNames);

	if (BoneNames.IsEmpty())
	{
		HOUDINI_LOG_ERROR(TEXT("No Bone names found on skeleton"));
		return {};
	}

	// Fill in bone names.
	Result.Bones.SetNum(BoneNames.Num());
	for(int Index = 0; Index < BoneNames.Num(); Index++)
	{
		FHoudiniSkeletonBone & Bone = Result.Bones[Index];
		Bone.Name = BoneNames[Index];
		Bone.Id = Index;
		Result.BoneMap.Add(Bone.Name, &Bone);
	}

	for (int Index = 0; Index < ParentChild.Num(); Index++)
	{
		FHoudiniSkeletonBone* Node = Result.BoneMap[ParentChild[Index]];
		if (!Result.HoudiniBoneMap.Contains(ParentChildBoneNumbers[Index]))
		{
			Node->HoudiniBoneNumber = ParentChildBoneNumbers[Index];
			Result.HoudiniBoneMap.Add(ParentChildBoneNumbers[Index], Node->Name);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Read matrices
	//--------------------------------------------------------------------------------------------------------------------

	TArray<float> RotationData; // 9 floats per bone
	TArray<float> PositionData; // 3 floats per bone

	bSuccess = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(NodeId, PartId, "transform", AttribInfo, RotationData, 0, HAPI_ATTROWNER_POINT);
	bSuccess &= FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(NodeId, PartId, "P", AttribInfo, PositionData, 0, HAPI_ATTROWNER_POINT);

	for(int Index = 0; Index < Result.Bones.Num(); Index++)
	{
		FHoudiniSkeletonBone* BOne = &Result.Bones[Index];
		int PointIndex = BoneNamesToPointIndex[BOne->Name];

		BOne->HoudiniGlobalMatrix = ConstructHoudiniMatrix(&RotationData[PointIndex * 9], &PositionData[PointIndex * 3]);
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Fill in parent - child relationships
	//--------------------------------------------------------------------------------------------------------------------

	for(int Index = 0; Index < ParentChild.Num(); Index+= 2)
	{
		FHoudiniSkeletonBone* Parent = Result.BoneMap[ParentChild[Index]];
		FHoudiniSkeletonBone* Child = Result.BoneMap[ParentChild[Index + 1]];

		Child->Parent = Parent;
		Parent->Children.Add(Child);
	}

	// Root is first bone with no parent
	Result.Root = nullptr;
	for(FHoudiniSkeletonBone& Bone : Result.Bones)
	{
		if (!Bone.Parent)
		{
			Result.Root = &Bone;
		}
	}
	if (!Result.Root)
	{
		HOUDINI_LOG_ERROR(TEXT("No root found on skeleton"));
		return {};
	}

	//--------------------------------------------------------------------------------------------------------------------
	// Convert Houdini matrices to Unreal matrix, and calculate the local matrices(which is what Unreal wants).
	//--------------------------------------------------------------------------------------------------------------------

	std::function<void(FHoudiniSkeletonBone*, FHoudiniSkeletonBone*)> ConstructGlobalMatrices = [&](FHoudiniSkeletonBone * Node, const FHoudiniSkeletonBone* Parent) -> void
	{

		FTransform ParentUnrealMatrix = FTransform::Identity;
		if (Parent != nullptr)
		{
			ParentUnrealMatrix = Parent->UnrealGlobalMatrix;
		}
		Node->UnrealGlobalMatrix = HoudiniToUnrealMatrix(Node->HoudiniGlobalMatrix);
		Node->UnrealLocalMatrix = Node->UnrealGlobalMatrix * ParentUnrealMatrix.Inverse();
		for(auto Child : Node->Children)
		{
			ConstructGlobalMatrices(Child, Node);
		}
	};

	ConstructGlobalMatrices(Result.Root, nullptr);

	return Result;
}

FHoudiniSkinWeights FHoudiniSkeletalMeshUtils::ConstructSkinWeights(HAPI_NodeId NodeId, HAPI_PartId PartId, FHoudiniSkeleton& Skeleton)
{
	HAPI_AttributeInfo BoneCaptureInfo;
	TArray<float> BoneCaptureData;

	bool bSuccess = FHoudiniEngineUtils::HapiGetAttributeDataAsFloat(NodeId, PartId, "boneCapture", BoneCaptureInfo, BoneCaptureData, 0, HAPI_ATTROWNER_POINT);
	if (!bSuccess || BoneCaptureData.Num() == 0)
	{
		HOUDINI_LOG_ERROR(TEXT("No Capture Data found on Skeletal Mesh."));		
		return {};
	}

	int HoudiniInfluencesPerVertex = BoneCaptureInfo.tupleSize / 2; // Divide by two compensate for the 'two floats' per capture data entry.

	const int MaxInfluences = 4; // TODO: Support more than 4.

	FHoudiniSkinWeights SkinWeights;
	SkinWeights.NumInfluences = FMath::Min(MaxInfluences, HoudiniInfluencesPerVertex);
	SkinWeights.NumVertices = BoneCaptureData.Num() / (2 * HoudiniInfluencesPerVertex);
	SkinWeights.Influences.SetNum(SkinWeights.NumInfluences * SkinWeights.NumVertices);

	TArray< FHoudiniSkinInfluence> InputInfluences;
	InputInfluences.SetNum(HoudiniInfluencesPerVertex);

	for(int VertexIndex = 0; VertexIndex < SkinWeights.NumVertices; VertexIndex++)
	{
		// Read input influences
		for(int InputInfluence = 0; InputInfluence < HoudiniInfluencesPerVertex; InputInfluence++)
		{
			int Index = VertexIndex * HoudiniInfluencesPerVertex + InputInfluence;
			int BoneIndex = static_cast<int>(BoneCaptureData[Index * 2]);
			float BoneWeight = BoneCaptureData[Index * 2 + 1];

			if (BoneWeight > 0.0f && BoneIndex != -1)
			{
				if(BoneIndex >= Skeleton.Bones.Num())
				{
					HOUDINI_LOG_ERROR(TEXT("Invalid bone index in bone capture."));
					return {};
				}

				const FString& BoneName = Skeleton.HoudiniBoneMap[BoneIndex];
				InputInfluences[InputInfluence].Bone = Skeleton.BoneMap[BoneName];
				InputInfluences[InputInfluence].Weight = BoneWeight;
			}
			else
			{
				InputInfluences[InputInfluence].Bone = nullptr;
				InputInfluences[InputInfluence].Weight = 0.0f;
			}
		}

		// Sort the bone influences by weight (higher weight first) so we can get the most influencial.
		InputInfluences.Sort([](const FHoudiniSkinInfluence & Bone1, const FHoudiniSkinInfluence& Bone2) { return Bone1.Weight > Bone2.Weight; });
		float TotalWeight = 0.0f;
		for(int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			TotalWeight += InputInfluences[AddIndex].Weight;

		float WeightScale = 1.0f / TotalWeight;
		for (int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			InputInfluences[AddIndex].Weight *= WeightScale;

		// Add input influence to result
		for (int AddIndex = 0; AddIndex < SkinWeights.NumInfluences; AddIndex++)
			SkinWeights.Influences[VertexIndex * SkinWeights.NumInfluences + AddIndex] = InputInfluences[AddIndex];

		// If there are less influences in Houdini than required for Unreal, add dummy wights
		FHoudiniSkinInfluence EmptyInfluence;
		for(int Index = InputInfluences.Num(); Index < SkinWeights.NumInfluences; Index++)
		{
			SkinWeights.Influences[VertexIndex * SkinWeights.NumInfluences + Index] = EmptyInfluence;
		}
	}
	return SkinWeights;
}


FHoudiniSkeletalMeshMaterialSettings FHoudiniSkeletalMeshUtils::GetMaterialOverrides(HAPI_NodeId NodeId, HAPI_PartId PartId)
{
	// Fetch attributes and check its valid
	HAPI_AttributeInfo AttribInfo;
	TArray<FString> AttributeData;

	bool bSuccess = FHoudiniEngineUtils::HapiGetAttributeDataAsString(NodeId, PartId, HAPI_UNREAL_ATTRIB_MATERIAL, AttribInfo, AttributeData, 0, HAPI_ATTROWNER_PRIM);
	if (!bSuccess || AttributeData.IsEmpty())
		return {};

	// Find all unique material overrides and create a FHoudiniSkeletalMeshMaterial per material.

	TMap<FString, FHoudiniSkeletalMeshMaterial> UniqueMaterials;
	int NextFreeSlot = 0;
	for(FString & Attribute : AttributeData)
	{
		if (!UniqueMaterials.Contains(Attribute))
		{
			FHoudiniSkeletalMeshMaterial Material;
			Material.OverridePath = Attribute;
			Material.AssetPath = Attribute;
			Material.Slot = NextFreeSlot++;

			// see if  there is a slot override. The name will begin the [0], [1], etc.
			if (Material.OverridePath.StartsWith("["))
			{
				FRegexPattern NumberPattern(TEXT("\\[(\\d+)\\]"));
				FRegexMatcher Matcher(NumberPattern, Material.OverridePath);
				if (Matcher.FindNext())
				{
					int32 BeginIndex = Matcher.GetCaptureGroupBeginning(1);
					int32 EndIndex = Matcher.GetCaptureGroupEnding(1);
					FString MatchedNumber = Material.OverridePath.Mid(BeginIndex, EndIndex - BeginIndex);
					if (MatchedNumber.IsNumeric())
					{
						Material.Slot = FCString::Atoi(*MatchedNumber);
						Material.AssetPath = Material.OverridePath.Mid(EndIndex + 1);
						Attribute = Material.AssetPath;
					}
				}
			}

			UniqueMaterials.Add(Attribute, Material);
		}
	}

	// Store all unique materials in the results, sorting by slot.
	FHoudiniSkeletalMeshMaterialSettings Result;
	for(auto & It : UniqueMaterials)
	{
		Result.Materials.Add(It.Value);
	}
	Result.Materials.Sort([](const FHoudiniSkeletalMeshMaterial & A, const FHoudiniSkeletalMeshMaterial & B) { return A.Slot < B.Slot; } );

	// Create per-face indexes into the material array
	TMap<FString, int> AttributeToMaterialIndex;
	for(int Index = 0; Index < Result.Materials.Num(); Index++)
	{
		AttributeToMaterialIndex.Add(Result.Materials[Index].AssetPath, Index);
	}

	Result.MaterialIds.SetNum(AttributeData.Num());
	for (int Index = 0; Index < AttributeData.Num(); Index++)
	{
		Result.MaterialIds[Index] = AttributeToMaterialIndex[AttributeData[Index]];
	}
	return Result;
}

FHoudiniSkeletalMeshMaterialSettings FHoudiniSkeletalMeshUtils::GetHoudiniMaterials(HAPI_NodeId NodeId, HAPI_PartId PartId, int NumFaces)
{
	TArray<HAPI_NodeId> MaterialNodes;
	MaterialNodes.SetNum(NumFaces);

	bool bSingleMaterial = false;

	HAPI_Result HapiResult = FHoudiniApi::GetMaterialNodeIdsOnFaces(
			FHoudiniEngine::Get().GetSession(),
			NodeId, PartId, &bSingleMaterial, MaterialNodes.GetData(), 0, NumFaces);

	if (HapiResult != HAPI_RESULT_SUCCESS)
		return {};

	FHoudiniSkeletalMeshMaterialSettings Result;

	TMap<HAPI_NodeId, int > UniqueMaterials;

	if (bSingleMaterial)
	{
		FHoudiniSkeletalMeshMaterial Material;
		Material.NodeId = MaterialNodes[0];
		Material.Slot = 0;
		UniqueMaterials.Add(MaterialNodes[0], 0);
		Result.Materials.Add(Material);
	}
	else
	{
		for(int Index = 0; Index < MaterialNodes.Num(); Index++)
		{
			if (!UniqueMaterials.Contains(NodeId))
			{
				FHoudiniSkeletalMeshMaterial Material;
				Material.NodeId = MaterialNodes[Index];
				Material.Slot = Result.Materials.Num();
				UniqueMaterials.Add(NodeId, Index);
				Result.Materials.Add(Material);
			}
		}
	}

	Result.MaterialIds.SetNum(NumFaces);
	for(int FaceId = 0; FaceId < NumFaces; FaceId++)
	{
		Result.MaterialIds[FaceId] = UniqueMaterials[MaterialNodes[FaceId]];
	}

	Result.GeoNodeId = NodeId;
	return Result;

}

bool FHoudiniSkeletalMeshUtils::CreateHoudiniMaterial(
	FHoudiniSkeletalMeshMaterialSettings& SkeletalFaceMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> InputAssignmentMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> AllOutputMaterials,
	TMap<FHoudiniMaterialIdentifier, UMaterialInterface*> OutputAssignmentMaterials,
	const FHoudiniPackageParams& InPackageParams)
{
	TArray<int> UniqueHoudiniMaterialIds;

	TArray<HAPI_MaterialInfo> UniqueHoudiniMaterialInfos;
	UniqueHoudiniMaterialInfos.SetNum(SkeletalFaceMaterials.Materials.Num());
	UniqueHoudiniMaterialIds.SetNum(SkeletalFaceMaterials.Materials.Num());

	// Fetch all material infos.
	for(int Index = 0; Index < SkeletalFaceMaterials.Materials.Num(); Index++)
	{
		UniqueHoudiniMaterialIds[Index] = SkeletalFaceMaterials.Materials[Index].NodeId;
		HOUDINI_CHECK_ERROR_RETURN(FHoudiniApi::GetMaterialInfo(FHoudiniEngine::Get().GetSession(), 
			SkeletalFaceMaterials.Materials[Index].NodeId, &UniqueHoudiniMaterialInfos[Index]), false);
	}

	// Create materials.

	TArray<UPackage*> MaterialAndTexturePackages;
	FHoudiniPackageParams PackageParams = InPackageParams;
	TArray<UMaterialInterface*> OutMaterialArray;

	if (!FHoudiniMaterialTranslator::CreateHoudiniMaterials(
		SkeletalFaceMaterials.GeoNodeId,
		InPackageParams,
		UniqueHoudiniMaterialIds,
		UniqueHoudiniMaterialInfos,
		InputAssignmentMaterials,
		AllOutputMaterials,
		OutputAssignmentMaterials,
		OutMaterialArray,
		MaterialAndTexturePackages,
		false,
		true,
		false))
	{
		return false;
	}

	// Set output materials.

	if (OutMaterialArray.Num() != SkeletalFaceMaterials.Materials.Num())
		return false;

	for(int Index = 0; Index < SkeletalFaceMaterials.Materials.Num(); Index++)
	{
		SkeletalFaceMaterials.Materials[Index].AssetPath = OutMaterialArray[Index]->GetPathName();
	}
	return true;
}
