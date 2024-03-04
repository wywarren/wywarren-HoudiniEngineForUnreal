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

#include "HoudiniEditorTestLandscapes.h"

#include "HoudiniParameterFloat.h"
#include "HoudiniParameterInt.h"
#include "Landscape.h"
#include "AssetRegistry/AssetRegistryModule.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "HoudiniEditorTestUtils.h"

#include "Misc/AutomationTest.h"
#include "GenericPlatform/GenericPlatformProcess.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "HoudiniEditorUnitTestUtils.h"
#include "LandscapeEdit.h"

TArray<FString> FHoudiniEditorTestLandscapes::CheckHeightFieldValues(TArray<float> & Results, TArray<float> & Expected, const FIntPoint & Size, int MaxErrors)
{
	TArray<FString> Errors;
	
	for(int Y = 0; Y < Size.Y; Y++)
	{
		for (int X = 0; X < Size.X; X++)
		{
			int ResultIndex = X + Y * Size.X;
			int ExpectedIndex = Y + X * Size.Y;
			float ExpectedValue = Expected[ExpectedIndex];

			float AbsDiff = FMath::Abs(ExpectedValue - Results[ResultIndex]);
			if (AbsDiff > 0.01f)
			{
				FString Error = FString::Printf(TEXT("(%d, %d) Expected %.2f but got %.2f"), X, Y, ExpectedValue, Results[ResultIndex]);
				Errors.Add(Error);
				if (Results.Num() == MaxErrors)
				{
					FString Terminator = FString(TEXT("... skipping additional Height Field Checks..."));
					Errors.Add(Terminator);
					return Errors;
				}
			}
		}
	}
	return Errors;
}

TArray<float> FHoudiniEditorTestLandscapes::GetLandscapeValues(ALandscape * LandscapeActor)
{
	FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
	FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);


	TArray<float> HoudiniValues;

	TArray<uint16> Values;
	{
		auto EditLayer = LandscapeActor->GetLayer(0);

		int NumPoints = LandscapeVertSize.X * LandscapeVertSize.Y;
		Values.SetNum(NumPoints);

		FScopedSetLandscapeEditingLayer Scope(LandscapeActor, EditLayer->Guid, [&] {});

		FLandscapeEditDataInterface LandscapeEdit(LandscapeActor->GetLandscapeInfo());
		LandscapeEdit.SetShouldDirtyPackage(false);
		LandscapeEdit.GetHeightDataFast(0, 0, LandscapeVertSize.X - 1, LandscapeVertSize.Y - 1, Values.GetData(), 0);
	}

	FTransform LandscapeTransform = LandscapeActor->GetActorTransform();

	HoudiniValues.SetNum(Values.Num());

	float ZScale = LandscapeTransform.GetScale3D().Z / 100.0f;
	for (int Index = 0; Index < Values.Num(); Index++)
	{
		// https://docs.unrealengine.com/4.27/en-US/BuildingWorlds/Landscape/TechnicalGuide/
		float HoudiniValue = ZScale * (static_cast<float>(Values[Index]) - 32768.0f) / 128.0f;
		HoudiniValues[Index] = HoudiniValue;
	}

	return HoudiniValues;
}

TArray<float> FHoudiniEditorTestLandscapes::CreateExpectedLandscapeValues(const FIntPoint & ExpectedSize, float HeightScale)
{
	TArray<float> ExpectedResults;
	ExpectedResults.SetNum(ExpectedSize.X * ExpectedSize.Y);
	for (int Y = 0; Y < ExpectedSize.Y; Y++)
	{
		for (int X = 0; X < ExpectedSize.X; X++)
		{
			int Index = X + Y * ExpectedSize.X;

			// This line should mimic what the height field wrangle node is doing.
			ExpectedResults[Index] = HeightScale * (Y * 2 + X);
		}
	}
	return ExpectedResults;
}

float FHoudiniEditorTestLandscapes::GetMin(const TArray<float>& Values)
{
	float MinValue = TNumericLimits<float>::Max();
	for( float Value : Values)
		MinValue = FMath::Min(MinValue, Value);
	return MinValue;
}

float FHoudiniEditorTestLandscapes::GetMax(const TArray<float>& Values)
{
	float MaxValue = TNumericLimits<float>::Min();
	for (float Value : Values)
		MaxValue = FMath::Max(MaxValue, Value);

	return MaxValue;
}


IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_Simple, "Houdini.UnitTests.Landscapes.Simple", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_Simple::RunTest(const FString & Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	
	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	constexpr int LandscapeSize = 64;
	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
		{
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 1);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 1, 0);
			SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
			Context->StartCookingHDA();
			return true;
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
		{
			// Cooking has finished, however, Unreal updates the landscape during Tick(), so wait a couple of frames
			Context->WaitForTicks(10);
			return true;
		}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
		{
			TArray<UHoudiniOutput*> Outputs;
			Context->HAC->GetOutputs(Outputs);

			// We should have one output.
			HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

			// Fetch the output as a landscape..
			TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
			HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
			ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

			const FIntPoint ExpectedGridSize = FIntPoint(LandscapeSize, LandscapeSize);

			// Check the size of the landscape is correct.
			FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
			FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
			HOUDINI_TEST_EQUAL(LandscapeVertSize, ExpectedGridSize);

			TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedLandscapeValues(ExpectedGridSize, 1.0f);
			TArray<float> HoudiniValues = FHoudiniEditorTestLandscapes::GetLandscapeValues(LandscapeActor);
			TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckHeightFieldValues(HoudiniValues, ExpectedResults, ExpectedGridSize, true);
			HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, for(auto & Error : Errors) this->AddError(Error); );

			FBox Bounds = LandscapeActor->GetLoadedBounds();

			float MinValue = FHoudiniEditorTestLandscapes::GetMin(ExpectedResults);
			float MaxValue = FHoudiniEditorTestLandscapes::GetMax(ExpectedResults);
			
			FVector3d ExpectedSize((LandscapeSize - 1) * 100.0f, (LandscapeSize - 1) * 100.0f, (MaxValue - MinValue) * 100.0f);
			FVector3d ActualSize = Bounds.GetSize();
			HOUDINI_TEST_EQUAL(ExpectedSize.X, ActualSize.X);
			HOUDINI_TEST_EQUAL(ExpectedSize.Y, ActualSize.Y);
			HOUDINI_TEST_EQUAL(ExpectedSize.Z, ActualSize.Z);

			return true;
		}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}

#if 0
IMPLEMENT_SIMPLE_HOUDINI_AUTOMATION_TEST(FHoudiniEditorTestLandscapes_GridSize, "AAAHoudini.UnitTests.Landscapes.GridSize", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FHoudiniEditorTestLandscapes_GridSize::RunTest(const FString& Parameters)
{
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// This test various aspects of Landscapes.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/// Make sure we have a Houdini Session before doing anything.
	FHoudiniEditorTestUtils::CreateSessionIfInvalidWithLatentRetries(this, FHoudiniEditorTestUtils::HoudiniEngineSessionPipeName, {}, {});

	// Now create the test context.
	TSharedPtr<FHoudiniTestContext> Context(new FHoudiniTestContext(this, TEXT("/Game/TestHDAs/Landscape/Test_Landscapes"), FTransform::Identity, false));
	HOUDINI_TEST_EQUAL_ON_FAIL(Context->IsValid(), true, return false);

	Context->HAC->bOverrideGlobalProxyStaticMeshSettings = true;
	Context->HAC->bEnableProxyStaticMeshOverride = false;

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Create a small landscape and check it loads.
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	constexpr int LandscapeSize = 64;
	{
		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
			{
				SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 0);
				SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "size", LandscapeSize, 1);
				SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterInt, "grid_size", 2, 0);
				SET_HDA_PARAMETER(Context->HAC, UHoudiniParameterFloat, "height_scale", 1.0f, 0);
				Context->StartCookingHDA();
				return true;
			}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context]()
			{
				// Cooking has finished, however, Unreal updates the landscape during Tick(), so wait a couple of frames
				Context->WaitForTicks(10);
				return true;
			}));

		AddCommand(new FHoudiniLatentTestCommand(Context, [this, Context, LandscapeSize]()
			{
				TArray<UHoudiniOutput*> Outputs;
				Context->HAC->GetOutputs(Outputs);

				// We should have one output.
				HOUDINI_TEST_EQUAL_ON_FAIL(Outputs.Num(), 1, return true);

				// Fetch the output as a landscape..
				TArray<UHoudiniLandscapeTargetLayerOutput*> LandscapeOutputs = FHoudiniEditorUnitTestUtils::GetOutputsWithObject<UHoudiniLandscapeTargetLayerOutput>(Outputs);
				HOUDINI_TEST_EQUAL(LandscapeOutputs.Num(), 1);
				ALandscape* LandscapeActor = LandscapeOutputs[0]->Landscape;

				const FIntPoint ExpectedGridSize = FIntPoint(LandscapeSize / 2, LandscapeSize / 2);

				// Check the size of the landscape is correct.
				FIntPoint LandscapeQuadSize = LandscapeActor->GetBoundingRect().Size();
				FIntPoint LandscapeVertSize(LandscapeQuadSize.X + 1, LandscapeQuadSize.Y + 1);
				HOUDINI_TEST_EQUAL_ON_FAIL(LandscapeVertSize, ExpectedGridSize, return true);

				TArray<float> ExpectedResults = FHoudiniEditorTestLandscapes::CreateExpectedLandscapeValues(ExpectedGridSize, 1.0f);
				TArray<float> HoudiniValues = FHoudiniEditorTestLandscapes::GetLandscapeValues(LandscapeActor);
				TArray<FString> Errors = FHoudiniEditorTestLandscapes::CheckHeightFieldValues(HoudiniValues, ExpectedResults, ExpectedGridSize, true);
				HOUDINI_TEST_EQUAL_ON_FAIL(Errors.Num(), 0, { for (auto& Error : Errors) this->AddError(Error); return true; } );

				FBox Bounds = LandscapeActor->GetLoadedBounds();

				float MinValue = FHoudiniEditorTestLandscapes::GetMin(ExpectedResults);
				float MaxValue = FHoudiniEditorTestLandscapes::GetMax(ExpectedResults);

				FVector3d ExpectedSize((LandscapeSize - 1) * 100.0f, (LandscapeSize - 1) * 100.0f, (MaxValue - MinValue) * 100.0f);
				FVector3d ActualSize = Bounds.GetSize();
				HOUDINI_TEST_EQUAL(ExpectedSize.X, ActualSize.X);
				HOUDINI_TEST_EQUAL(ExpectedSize.Y, ActualSize.Y);
				HOUDINI_TEST_EQUAL(ExpectedSize.Z, ActualSize.Z);

				return true;
			}));
	}

	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	/// Done
	/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	///
	return true;
}
#endif
#endif

