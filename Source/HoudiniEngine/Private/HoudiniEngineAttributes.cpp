/*
* Copyright (c) <2024> Side Effects Software Inc.
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


#include "HoudiniEngineAttributes.h"
#include <type_traits>

#include "HoudiniEngine.h"
#include "HoudiniEngineTimers.h"
#include "HoudiniEngineUtils.h"

struct FHoudiniRawAttributeData
{
	// This structure is used to store data before it is converted to a different type.
	//
	// Only one of the following fields is valid at a time. Can't use a union due to ambiguous
	// destructors.

	TArray<uint8> RawDataUint8;
	TArray<int8> RawDataInt8;
	TArray<int16> RawDataInt16;
	TArray<int64> RawDataInt64;
	TArray<int> RawDataInt;
	TArray<float> RawDataFloat;
	TArray<double> RawDataDouble;
	TArray<FString> RawDataStrings;

};

FHoudiniHapiAccessor::FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const char* Name)
{
	Init(NodeId, PartId, Name);
}

void FHoudiniHapiAccessor::Init(HAPI_NodeId InNodeId, HAPI_NodeId InPartId, const char* InName)
{
	NodeId = InNodeId;
	PartId = InPartId;
	AttributeName = InName;
}


bool
FHoudiniHapiAccessor::GetInfo(HAPI_AttributeInfo& OutAttributeInfo, const HAPI_AttributeOwner InOwner)
{
	H_SCOPED_FUNCTION_TIMER()

	FHoudiniApi::AttributeInfo_Init(&OutAttributeInfo);

	const auto GetInfoLambda =
		[&](const HAPI_AttributeOwner Owner) -> bool
	{
		const HAPI_Result Result = FHoudiniApi::GetAttributeInfo(
			FHoudiniEngine::Get().GetSession(),
			NodeId,
			PartId,
			AttributeName,
			Owner,
			&OutAttributeInfo);

		if (Result != HAPI_RESULT_SUCCESS)
			OutAttributeInfo.exists = false;

		return Result == HAPI_RESULT_SUCCESS && OutAttributeInfo.exists;
	};

	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; ++OwnerIdx)
		{
			if (GetInfoLambda(static_cast<HAPI_AttributeOwner>(OwnerIdx)))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return GetInfoLambda(InOwner);
	}
}

FString FHoudiniHapiAccessor::ToString(int32 Number)
{
	return FString::Printf(TEXT("%d"), Number);
}

FString FHoudiniHapiAccessor::ToString(float Number)
{
	return FString::Printf(TEXT("%f"), Number);
}

FString FHoudiniHapiAccessor::ToString(double Number)
{
	return FString::Printf(TEXT("%f"), Number);
}

double FHoudiniHapiAccessor::ToDouble(const FString& Str)
{
	return FCString::Atof(*Str);
}

int FHoudiniHapiAccessor::ToInt(const FString& Str)
{
	return FCString::Atoi(*Str);
}

template<typename SrcType, typename DestType>
void FHoudiniHapiAccessor::Convert(const SrcType* SourceData, DestType* DestData, int Count)
{
	for (int Index = 0; Index < Count; Index++)
	{
		if constexpr (std::is_arithmetic_v<DestType>)
		{
			if constexpr (std::is_same_v<SrcType, FString>)
			{
				if constexpr  (std::is_same_v<DestType, float> || std::is_same_v<DestType, double>)
					DestData[Index] = static_cast<DestType>(ToDouble(SourceData[Index]));
				else
					DestData[Index] = static_cast<DestType>(ToInt(SourceData[Index]));
			}
			else
			{
				DestData[Index] = static_cast<DestType>(SourceData[Index]);
			}
		}
		else if constexpr (std::is_same_v<DestType,FString>)
		{
			if constexpr (std::is_same_v<SrcType, FString>)
			{
				DestData[Index] = SourceData[Index];
			}
			else
			{
				DestData[Index] = ToString(SourceData[Index]);
			}
		}
	}
}

template<typename DataType>
void FHoudiniHapiAccessor::ConvertFromRawData(const FHoudiniRawAttributeData & RawData, DataType* Data, size_t Count)
{
	if (RawData.RawDataUint8.Num() > 0)
	{
		Convert(RawData.RawDataUint8.GetData(), Data, Count);
	}
	else if (RawData.RawDataInt8.Num() > 0)
	{
		Convert(RawData.RawDataInt8.GetData(), Data, Count);
	}
	else if(RawData.RawDataInt16.Num() > 0)
	{
		Convert(RawData.RawDataInt16.GetData(), Data, Count);
	}
	else if (RawData.RawDataInt.Num() > 0)
	{
		Convert(RawData.RawDataInt.GetData(), Data, Count);
	}
	else if (RawData.RawDataFloat.Num() > 0)
	{
		Convert(RawData.RawDataFloat.GetData(), Data, Count);
	}
	else if (RawData.RawDataDouble.Num() > 0)
	{
		Convert(RawData.RawDataDouble.GetData(), Data, Count);
	}
	else if (RawData.RawDataStrings.Num() > 0)
	{
		Convert(RawData.RawDataStrings.GetData(), Data, Count);
	}
	else
	{
		// no data.
	}
}

struct FHoudiniAttributeTask
{
	int RawIndex;
	int Count;
	const FHoudiniHapiAccessor* Accessor;
	const HAPI_AttributeInfo * StorageInfo;
	const HAPI_Session* Session;
	bool bSuccess;

	static bool CanAbandon() { return false; }
	static void Abandon() {  }
	TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FHoudiniAttributeTask, STATGROUP_ThreadPoolAsyncTasks); }

};

template<typename DataType>
struct FHoudiniAttributeGetTask : public  FHoudiniAttributeTask
{
	DataType* Results;

	void DoWork()
	{
		bSuccess = Accessor->GetAttributeData(Session, *StorageInfo, Results, RawIndex, Count);
	}
};

int FHoudiniHapiAccessor::CalculateNumberOfSessions() const
{
	// Calculate number of sessions to use. TODO: Do not use multiple sessions if the data size is small, perhaps
	// less than a few mbs.
	int NumSessions = FHoudiniEngine::Get().GetNumSessions();
	if (NumSessions == 0)
		return 0;

	if (bAllowMultiThreading)
		return NumSessions;
	else
		return 1;

}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int Start, int Count) const
{
	if (Count == 0)
		return true;

	HAPI_StorageType StorageType = GetHapiType<DataType>();
	if (StorageType == AttributeInfo.storage)
	{
		// No conversion is necessary so fetch the data directly.
		FetchHapiData(Session, AttributeInfo, Results, Start, Count);
	}
	else
	{
		// Fetch data in Hapi format then convert it.
		FHoudiniRawAttributeData RawData;
		if (!GetRawAttributeData(Session, AttributeInfo, RawData, Start, Count))
			return false;

		ConvertFromRawData(RawData, Results, Count * AttributeInfo.tupleSize);
	}
	return true;
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& Results, int First, int Count)
{
	if (Count == -1)
		Count = AttributeInfo.count;

	int TotalCount;
	if (IsHapiArrayType(AttributeInfo.storage))
	{
		if (!bCanBeArray)
			return false;

		TotalCount = AttributeInfo.totalArrayElements;
		if (Count != AttributeInfo.count)
		{
			HOUDINI_LOG_ERROR(TEXT("Cannot get a partial array from HAPI"));
			return false;
		}
	}
	else
	{
		TotalCount = Count * AttributeInfo.tupleSize;
	}

	Results.SetNum(TotalCount);

	return GetAttributeData(AttributeInfo, Results.GetData(), First, Count);
}

template<typename DataType>
bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int First, int Count)
{
	return GetAttributeDataMain(AttributeInfo, Results, First, Count);
}


template<typename DataType>
bool FHoudiniHapiAccessor::GetAttributeDataMain(const HAPI_AttributeInfo& AttributeInfo, DataType * Results, int First, int Count)
{
	// This is the actual main function for getting data.

	H_SCOPED_FUNCTION_DYNAMIC_LABEL(FString::Printf(TEXT("FHoudiniAttributeAccessor::Get (%s)"), ANSI_TO_TCHAR(AttributeName)));

	int NumSessions = CalculateNumberOfSessions();
	if (IsHapiArrayType(AttributeInfo.storage))
	{
		if (!bCanBeArray)
			return false;

		NumSessions = 1;
	}

	// Task array.
	TArray<FAsyncTask<FHoudiniAttributeGetTask<DataType>>> Tasks;
	Tasks.SetNum(NumSessions);

	for(int Session = 0; Session < NumSessions; Session++)
	{
		// Fill a task, one per session.
		FHoudiniAttributeGetTask<DataType> & Task = Tasks[Session].GetTask();

		int StartOffset = Count * Session / NumSessions;
		int EndOffset = Count * (Session + 1) / NumSessions;
		Task.Accessor = this;
		Task.StorageInfo = &AttributeInfo;
		Task.RawIndex = StartOffset + First;
		Task.Results = Results + StartOffset * AttributeInfo.tupleSize;
		Task.Count = EndOffset - StartOffset;
		Task.Session = FHoudiniEngine::Get().GetSession(Session);
		Tasks[Session].StartBackgroundTask();
	}

	bool bSuccess = true;
	for(int Task = 0; Task < Tasks.Num(); Task++)
	{
		Tasks[Task].EnsureCompletion();
		bSuccess &= Tasks[Task].GetTask().bSuccess;
	}

	return bSuccess;
}

#if 0
template<typename DataType>
struct FHoudiniAttributeSetTask : public  FHoudiniAttributeTask
{
	const DataType* Input;

	void DoWork()
	{
		bSuccess = FHoudiniEngineAttribute::HapiSetAttributeData(Session, *Accessor, *StorageInfo, Input, RawIndex, Count);
	}

};
#endif

#if 0
template<typename DataType> bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int First, int Count)
{
	// Task array.
	int NumSessions = Accessor.CalculateNumberOfSessions(); 
	TArray<FAsyncTask<FHoudiniAttributeSetTask<DataType>>> Tasks;
	Tasks.SetNum(NumSessions);

	for (int Session = 0; Session < NumSessions; Session++)
	{
		// Fill a task, one per session.
		FHoudiniAttributeSetTask<DataType>& Task = Tasks[Session].GetTask();

		int StartOffset = Count * Session / NumSessions;
		int EndOffset = Count * (Session + 1) / NumSessions;
		Task.Accessor = this;
		Task.StorageInfo = &AttributeInfo;
		Task.RawIndex = StartOffset + First;
		Task.Input = Data + StartOffset * AttributeInfo.tupleSize;
		Task.Count = EndOffset - StartOffset;
		Task.Session = FHoudiniEngine::Get().GetSession(Session);
		Tasks[Session].StartBackgroundTask();
	}

	bool bSuccess = true;
	for (int Task = 0; Task < Tasks.Num(); Task++)
	{
		Tasks[Task].EnsureCompletion();
		bSuccess &= Tasks[Task].GetTask().bSuccess;
	}
	return bSuccess;
}
#endif

#if 0
template<typename DataType>
bool FHoudiniHapiAccessor::SetAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int Count)
{

	auto NodeId = Accessor.NodeId;
	auto PartId = Accessor.PartId;
	auto AttributeName = Accessor.AttributeName;

	HAPI_Result Result = HAPI_RESULT_FAILURE;
	if constexpr (std::is_same_v<DataType, float>)
	{
		Result = FHoudiniApi::SetAttributeFloatData(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		Result = FHoudiniApi::SetAttributeFloat64Data(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		Result = FHoudiniApi::SetAttributeIntData(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		Result = FHoudiniApi::SetAttributeInt8Data(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		Result = FHoudiniApi::SetAttributeInt16Data(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		Result = FHoudiniApi::SetAttributeUInt8Data(Session, NodeId, PartId, AttributeName, &AttributeInfo, Data, StartIndex, Count);
	}
	else
	{
		static_assert(false, "Missing type");
	}

	return Result == HAPI_RESULT_SUCCESS;
}
#endif

#if 0
template<typename DataType>
bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& Data)
{
	return SetAttributeData(AttributeInfo, Data.GetData(), 0, Data.Num());
}
#endif

bool FHoudiniHapiAccessor::GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data)
{
	return GetRawAttributeData(Session, AttributeInfo, Data, 0, AttributeInfo.count);

}


HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, uint8* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeUInt8Data(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int8* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt8Data(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int16* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt16Data(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}
HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeIntData(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int64* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt64Data(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeFloatData(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, double* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeFloat64Data(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, -1, Data, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}
HAPI_Result FHoudiniHapiAccessor::FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FString* Data, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	FString* UnrealStrings = static_cast<FString*>(Data);
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(IndexCount);

	HAPI_Result Result = FHoudiniApi::GetAttributeStringData(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, StringHandles.GetData(), IndexStart, IndexCount);

	if (Result == HAPI_RESULT_SUCCESS)
		FHoudiniEngineString::SHArrayToFStringArray(StringHandles, UnrealStrings);
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, uint8* Data, int * Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeUInt8ArrayData(Session, 
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int8* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt8ArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int16* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt16ArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeIntArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}


HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int64* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeInt64ArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeFloatArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, double* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	HAPI_Result Result = FHoudiniApi::GetAttributeFloat64ArrayData(Session,
		NodeId, PartId, AttributeName, &TempAttributeInfo, Data, TempAttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;
	return Result;
}

HAPI_Result FHoudiniHapiAccessor::FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FString* Data, int* Sizes, int IndexStart, int IndexCount) const
{
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;
	TArray<HAPI_StringHandle> StringHandles;
	StringHandles.SetNum(TempAttributeInfo.totalArrayElements);

	HAPI_Result Result = FHoudiniApi::GetAttributeStringArrayData(Session, NodeId, PartId, AttributeName, &TempAttributeInfo, StringHandles.GetData(), 
		AttributeInfo.totalArrayElements, Sizes, IndexStart, IndexCount);

	if (Result == HAPI_RESULT_SUCCESS)
		FHoudiniEngineString::SHArrayToFStringArray(StringHandles, Data);

	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;

	return Result;
}


bool FHoudiniHapiAccessor::GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int IndexStart, int IndexCount) const
{
	HAPI_Result Result = HAPI_RESULT_FAILURE;

	int TotalCount = AttributeInfo.count * AttributeInfo.tupleSize;

	// Make a copy of the Attribute Info because it can be modified during fetching of data.
	HAPI_AttributeInfo TempAttributeInfo = AttributeInfo;

	int NumElement = IndexCount * AttributeInfo.tupleSize;
	int NumArrayElements = AttributeInfo.totalArrayElements;

	TArray<int> Sizes;

	switch(AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_UINT8:
		Data.RawDataUint8.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataUint8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT8:		
		Data.RawDataInt8.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt8.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT16:
		Data.RawDataInt16.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt16.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT64:
		Data.RawDataInt64.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt64.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT:
		Data.RawDataInt.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataInt.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT:
		Data.RawDataFloat.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataFloat.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT64:
		Data.RawDataDouble.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataDouble.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_STRING:
		Data.RawDataStrings.SetNum(NumElement);
		Result = FetchHapiData(Session, AttributeInfo, Data.RawDataStrings.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_UINT8_ARRAY:
		Data.RawDataUint8.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataUint8.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT8_ARRAY:
		Data.RawDataInt8.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt8.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT16_ARRAY:
		Data.RawDataInt16.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt16.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT64_ARRAY:
		Data.RawDataInt64.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt64.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_INT_ARRAY:
		Data.RawDataInt.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataInt.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT_ARRAY:
		Data.RawDataFloat.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataFloat.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_FLOAT64_ARRAY:
		Data.RawDataDouble.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataDouble.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;
	case HAPI_STORAGETYPE_STRING_ARRAY:
		Data.RawDataStrings.SetNum(NumArrayElements);
		Sizes.SetNum(NumElement);
		Result = FetchHapiDataArray(Session, AttributeInfo, Data.RawDataStrings.GetData(), Sizes.GetData(), IndexStart, IndexCount);
		break;

	default:
		return false;
	}
	
	if (!TempAttributeInfo.exists)
		Result = HAPI_RESULT_FAILURE;

	return Result == HAPI_RESULT_SUCCESS;
}

template<typename DataType>
HAPI_StorageType FHoudiniHapiAccessor::GetHapiType()
{
	if constexpr (std::is_same_v<DataType, float>)
	{
		return HAPI_STORAGETYPE_FLOAT;
	}
	else if constexpr (std::is_same_v<DataType, double>)
	{
		return HAPI_STORAGETYPE_FLOAT64;
	}
	else if constexpr (std::is_same_v<DataType, int>)
	{
		return HAPI_STORAGETYPE_INT;
	}
	else if constexpr (std::is_same_v<DataType, int8>)
	{
		return HAPI_STORAGETYPE_INT8;
	}
	else if constexpr (std::is_same_v<DataType, int16>)
	{
		return HAPI_STORAGETYPE_INT16;
	}
	else if constexpr (std::is_same_v<DataType, uint8>)
	{
		return HAPI_STORAGETYPE_UINT8;
	}
	else if constexpr (std::is_same_v<DataType, int64>)
	{
		return HAPI_STORAGETYPE_INT64;
	}
	else if constexpr (std::is_same_v<DataType, FString>)
	{
		return HAPI_STORAGETYPE_STRING;
	}
	else
	{
		return HAPI_STORAGETYPE_INVALID;
	}
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, TArray<DataType>& Results, int First, int Count)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	return GetAttributeData(AttrInfo, Results, First, Count);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, DataType* Results, int First, int Count)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;
	return GetAttributeData(AttrInfo, Results, First, Count);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeFirstValue(HAPI_AttributeOwner Owner, DataType& Result)
{
	return GetAttributeData(Owner, &Result, 0, 1);
}

template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, TArray<DataType>& Results, int First, int Count)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	AttrInfo.tupleSize = MaxTuples;

	return GetAttributeData(AttrInfo, Results, First, Count);

}
template<typename DataType> bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, DataType* Results, int First, int Count)
{
	HAPI_AttributeInfo AttrInfo;
	if (!GetInfo(AttrInfo, Owner))
		return false;

	AttrInfo.tupleSize = MaxTuples;

	return GetAttributeData(AttrInfo, Results, First, Count);
}

bool FHoudiniHapiAccessor::IsHapiArrayType(HAPI_StorageType StorageType)
{
	switch(StorageType)
	{
		case HAPI_STORAGETYPE_UINT8_ARRAY:
		case HAPI_STORAGETYPE_INT8_ARRAY:
		case HAPI_STORAGETYPE_INT16_ARRAY:
		case HAPI_STORAGETYPE_INT64_ARRAY:
		case HAPI_STORAGETYPE_INT_ARRAY:
		case HAPI_STORAGETYPE_FLOAT_ARRAY:
		case HAPI_STORAGETYPE_FLOAT64_ARRAY:
		case HAPI_STORAGETYPE_STRING_ARRAY:
			return true;
	default:
			return false;
	}
}


#define IMPLEMENT_HOUDINI_ACCESSOR(DATA_TYPE)\
	template bool FHoudiniHapiAccessor::GetAttributeFirstValue(HAPI_AttributeOwner Owner, DATA_TYPE& Result);\
	template bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, TArray<DATA_TYPE>& Results, int First, int Count);\
	template bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, DATA_TYPE * Results, int First, int Count);\
	template bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int TupleSize, TArray<DATA_TYPE>& Results, int First, int Count);\
	template bool FHoudiniHapiAccessor::GetAttributeData(HAPI_AttributeOwner Owner, int TupleSize, DATA_TYPE * Results, int First, int Count);\
	template bool FHoudiniHapiAccessor::GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DATA_TYPE>& Results, int First , int Count);


IMPLEMENT_HOUDINI_ACCESSOR(uint8);
IMPLEMENT_HOUDINI_ACCESSOR(int8);
IMPLEMENT_HOUDINI_ACCESSOR(int16);
IMPLEMENT_HOUDINI_ACCESSOR(int);
IMPLEMENT_HOUDINI_ACCESSOR(float);
IMPLEMENT_HOUDINI_ACCESSOR(double);
IMPLEMENT_HOUDINI_ACCESSOR(FString);
