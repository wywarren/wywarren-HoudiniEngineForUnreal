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

#include "HoudiniApi.h"
#include "HoudiniEngine.h"
#include "HoudiniEngineString.h"
#include "HoudiniEngineTimers.h"
#include "HoudiniEngineUtils.h"

#include "Async/AsyncWork.h"
#include "Templates/EnableIf.h"

#include <type_traits>

#define NO_CONVERSION_FOUND_LOG_MESSAGE \
	TEXT("Attribute %s has incorrect type and no suitable conversion was found! Expected type %s but recieved type %s.")
#define CONVERSION_SUCCESS_LOG_MESSAGE \
	TEXT("Attribute %s has incorrect type! A conversion was performed from type %s to type %s.")
#define CONVERSION_FAIL_LOG_MESSAGE \
	TEXT("Attribute %s has incorrect type! A conversion was attempted from type %s to type %s but failed.")

namespace
{
const TCHAR* GetStorageTypeString(const HAPI_StorageType StorageType)
{
	switch (StorageType)
	{
		case HAPI_STORAGETYPE_INT:              return TEXT("Int");
		case HAPI_STORAGETYPE_INT64:            return TEXT("Int64");
		case HAPI_STORAGETYPE_FLOAT:            return TEXT("Float");
		case HAPI_STORAGETYPE_FLOAT64:          return TEXT("Float64");
		case HAPI_STORAGETYPE_STRING:           return TEXT("String");
		case HAPI_STORAGETYPE_UINT8:            return TEXT("UInt8");
		case HAPI_STORAGETYPE_INT8:             return TEXT("Int8");
		case HAPI_STORAGETYPE_INT16:            return TEXT("Int16");
		case HAPI_STORAGETYPE_DICTIONARY:       return TEXT("Dictionary");
		case HAPI_STORAGETYPE_INT_ARRAY:        return TEXT("Int Array");
		case HAPI_STORAGETYPE_INT64_ARRAY:      return TEXT("Int64 Array");
		case HAPI_STORAGETYPE_FLOAT_ARRAY:      return TEXT("Float Array");
		case HAPI_STORAGETYPE_FLOAT64_ARRAY:    return TEXT("Float64 Array");
		case HAPI_STORAGETYPE_STRING_ARRAY:     return TEXT("String Array");
		case HAPI_STORAGETYPE_UINT8_ARRAY:      return TEXT("UInt8 Array");
		case HAPI_STORAGETYPE_INT8_ARRAY:       return TEXT("Int8 Array");
		case HAPI_STORAGETYPE_INT16_ARRAY:      return TEXT("Int16 Array");
		case HAPI_STORAGETYPE_DICTIONARY_ARRAY: return TEXT("Dictionary Array");
		default:                                return TEXT("???");
	}
}

// Traits which allow us to classify different HAPI storage types

/** Trait which determines if a storage type holds an integer. */
template<HAPI_StorageType StorageType> static constexpr bool TIsIntStorageType_V = false;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT_ARRAY> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_UINT8> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_UINT8_ARRAY> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT8> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT8_ARRAY> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT16> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT16_ARRAY> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT64> = true;
template<> constexpr bool TIsIntStorageType_V<HAPI_STORAGETYPE_INT64_ARRAY> = true;

/** Trait which determines if a storage type holds a float. */
template<HAPI_StorageType StorageType> static constexpr bool TIsFloatStorageType_V = false;
template<> constexpr bool TIsFloatStorageType_V<HAPI_STORAGETYPE_FLOAT> = true;
template<> constexpr bool TIsFloatStorageType_V<HAPI_STORAGETYPE_FLOAT_ARRAY> = true;
template<> constexpr bool TIsFloatStorageType_V<HAPI_STORAGETYPE_FLOAT64> = true;
template<> constexpr bool TIsFloatStorageType_V<HAPI_STORAGETYPE_FLOAT64_ARRAY> = true;

/** Trait which determines if a storage type is numeric (holds integer or floating point type). */
template<HAPI_StorageType StorageType> static constexpr bool TIsNumericStorageType_V =
	TIsIntStorageType_V<StorageType> || TIsFloatStorageType_V<StorageType>;

/** Trait which determines if a storage type holds strings. */
template<HAPI_StorageType StorageType> static constexpr bool TIsStringStorageType_V = false;
template<> constexpr bool TIsStringStorageType_V<HAPI_STORAGETYPE_STRING> = true;
template<> constexpr bool TIsStringStorageType_V<HAPI_STORAGETYPE_STRING_ARRAY> = true;

/** Trait which determines if a storage type holds dictionaries. */
template<HAPI_StorageType StorageType> static constexpr bool TIsDictionaryStorageType_V = false;
template<> constexpr bool TIsDictionaryStorageType_V<HAPI_STORAGETYPE_DICTIONARY> = true;
template<> constexpr bool TIsDictionaryStorageType_V<HAPI_STORAGETYPE_DICTIONARY_ARRAY> = true;

/** Trait which determines if a storage type holds arrays. */
template<HAPI_StorageType StorageType> static constexpr bool TIsArrayStorageType_V = false;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_INT_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_UINT8_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_INT8_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_INT16_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_INT64_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_FLOAT_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_FLOAT64_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_STRING_ARRAY> = true;
template<> constexpr bool TIsArrayStorageType_V<HAPI_STORAGETYPE_DICTIONARY_ARRAY> = true;

struct FIntStorageTypeInfo
{
	using UnrealDataType = int32;
	using SetDataType = int;
	using GetDataType = int;
};

struct FUInt8StorageTypeInfo
{
	using UnrealDataType = uint8;
	using SetDataType = HAPI_UInt8;
	using GetDataType = HAPI_UInt8;
};

struct FInt8StorageTypeInfo
{
	using UnrealDataType = int8;
	using SetDataType = HAPI_Int8;
	using GetDataType = HAPI_Int8;
};

struct FInt16StorageTypeInfo
{
	using UnrealDataType = int16;
	using SetDataType = HAPI_Int16;
	using GetDataType = HAPI_Int16;
};

struct FInt64StorageTypeInfo
{
	using UnrealDataType = int64;
	using SetDataType = HAPI_Int64;
	using GetDataType = HAPI_Int64;
};

struct FFloatStorageTypeInfo
{
	using UnrealDataType = float;
	using SetDataType = float;
	using GetDataType = float;
};

struct FFloat64StorageTypeInfo
{
	using UnrealDataType = double;
	using SetDataType = double;
	using GetDataType = double;
};

struct FStringStorageTypeInfo
{
	using UnrealDataType = FString;
	using SetDataType = const char*;
	using GetDataType = HAPI_StringHandle;
};

/**
 * Provides helpful type information about each storage type, such as the types we can expect from
 * HAPI Get/Set calls as well as the equivalent data type we expect in Unreal Engine.
 */
template<HAPI_StorageType StorageType> struct TStorageTypeInfo;
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT>              : FIntStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT_ARRAY>        : FIntStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_UINT8>            : FUInt8StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_UINT8_ARRAY>      : FUInt8StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT8>             : FInt8StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT8_ARRAY>       : FInt8StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT16>            : FInt16StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT16_ARRAY>      : FInt16StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT64>            : FInt64StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_INT64_ARRAY>      : FInt64StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_FLOAT>            : FFloatStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_FLOAT_ARRAY>      : FFloatStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_FLOAT64>          : FFloat64StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_FLOAT64_ARRAY>    : FFloat64StorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_STRING>           : FStringStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_STRING_ARRAY>     : FStringStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_DICTIONARY>       : FStringStorageTypeInfo {};
template<> struct TStorageTypeInfo<HAPI_STORAGETYPE_DICTIONARY_ARRAY> : FStringStorageTypeInfo {};

struct FAttributeArgs
{
	const HAPI_Session* Session;
	HAPI_NodeId NodeId;
	HAPI_PartId PartId;
	const char* AttributeName;
	const HAPI_AttributeInfo* AttributeInfo;
	EHoudiniEngineAttributeFlags Flags;
};

template<HAPI_StorageType StorageType>
struct TSetAttributeArgs : FAttributeArgs
{
	TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data;
	int32 Start;
	int32 Length;
};

template<HAPI_StorageType StorageType>
struct TSetArrayAttributeArgs : FAttributeArgs
{
	TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data;
	TArrayView<const int32> Sizes;
	int32 Start;
	int32 Length;
};

template<HAPI_StorageType StorageType>
struct TGetAttributeArgs : FAttributeArgs
{
	int32 Stride;
	TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data;
	int32 Start;
	int32 Length;
};

template<HAPI_StorageType StorageType>
struct TGetArrayAttributeArgs : FAttributeArgs
{
	TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data;
	TArrayView<int32> Sizes;
	int32 Start;
	int32 Length;
};

/**
 * Perfoms conversion from UnrealDataType to SetDataType if needed, and actually performs the HAPI
 * call using @p HapiAttributeSetter with correct arguments based on storage type.
 */
template<HAPI_StorageType StorageType, typename ArgsType, typename FnType>
HAPI_Result
SetAttribute(const ArgsType Args, FnType HapiAttributeSetter)
{
	using SetDataType = typename TStorageTypeInfo<StorageType>::SetDataType;
	using UnrealDataType = typename TStorageTypeInfo<StorageType>::UnrealDataType;

	const SetDataType* DataArray;

	// Only used if we convert
	FHoudiniEngineRawStrings RawStrings;
	TArray<SetDataType> ConvertedData;

	// We only need to convert if the types are different
	if constexpr (TIsStringStorageType_V<StorageType> || TIsDictionaryStorageType_V<StorageType>)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(TEXT("Converting FStrings to raw strings"))
		RawStrings.CreateRawStrings(Args.Data);
		DataArray = RawStrings.RawStrings.GetData();
	}
	else if constexpr (!std::is_same_v<SetDataType, UnrealDataType>)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(TEXT("Converting UnrealDataType to SetDataType"))
		for (const UnrealDataType& UnconvertedValue : Args.Data)
		{
			ConvertedData.Add(static_cast<SetDataType>(UnconvertedValue));
		}
		DataArray = ConvertedData.GetData();
	}
	else
	{
		DataArray = Args.Data.GetData();
	}

	// We will have to cast away constness because for some reason, HAPI takes a const char**
	// instead of a const char* const* when passing an array of strings.

	// Set Attribute
	if constexpr (std::is_same_v<ArgsType, TSetAttributeArgs<StorageType>>)
	{
		return HapiAttributeSetter(
			Args.Session,
			Args.NodeId,
			Args.PartId,
			Args.AttributeName,
			Args.AttributeInfo,
			const_cast<SetDataType*>(DataArray),
			Args.Start,
			Args.Length);
	}
	// Set Array Attribute
	else if constexpr (std::is_same_v<ArgsType, TSetArrayAttributeArgs<StorageType>>)
	{
		return HapiAttributeSetter(
			Args.Session,
			Args.NodeId,
			Args.PartId,
			Args.AttributeName,
			Args.AttributeInfo,
			const_cast<SetDataType*>(DataArray),
			Args.Data.Num(),
			Args.Sizes.GetData(),
			Args.Start,
			Args.Length);
	}
}

/**
 * Perfoms conversion from GetDataType to UnrealDataType if needed, and actually performs the HAPI
 * call using @p HapiAttributeSetter with correct arguments based on storage type.
 */
template<HAPI_StorageType StorageType, typename ArgsType, typename FnType>
HAPI_Result
GetAttribute(const ArgsType Args, FnType HapiAttributeGetter)
{
	using GetDataType = typename TStorageTypeInfo<StorageType>::GetDataType;
	using UnrealDataType = typename TStorageTypeInfo<StorageType>::UnrealDataType;

	GetDataType* DataArray;

	// Only used if we convert
	TArray<GetDataType> UnconvertedData;

	// We only need to convert if the types are different
	if constexpr (std::is_same_v<GetDataType, UnrealDataType>)
	{
		DataArray = Args.Data.GetData();
	}
	else
	{
		UnconvertedData.SetNumZeroed(Args.Data.Num());
		DataArray = UnconvertedData.GetData();
	}

	// We will have to cast away constness because for some reason, HAPI takes a HAPI_AttributeInfo*
	// instead of a const HAPI_AttributeInfo* when getting attributes.
	HAPI_Result Result = HAPI_RESULT_FAILURE;
	// Get Attribute
	if constexpr (std::is_same_v<ArgsType, TGetAttributeArgs<StorageType>>)
	{
		// Only numeric storage types use a stride
		if constexpr (TIsNumericStorageType_V<StorageType>)
		{
			Result = HapiAttributeGetter(
				Args.Session,
				Args.NodeId,
				Args.PartId,
				Args.AttributeName,
				const_cast<HAPI_AttributeInfo*>(Args.AttributeInfo),
				Args.Stride,
				DataArray,
				Args.Start,
				Args.Length);
		}
		else
		{
			Result = HapiAttributeGetter(
				Args.Session,
				Args.NodeId,
				Args.PartId,
				Args.AttributeName,
				const_cast<HAPI_AttributeInfo*>(Args.AttributeInfo),
				DataArray,
				Args.Start,
				Args.Length);
		}
	}
	// Get Array Attribute
	else if constexpr (std::is_same_v<ArgsType, TGetArrayAttributeArgs<StorageType>>)
	{
		Result = HapiAttributeGetter(
			Args.Session,
			Args.NodeId,
			Args.PartId,
			Args.AttributeName,
			const_cast<HAPI_AttributeInfo*>(Args.AttributeInfo),
			DataArray,
			Args.Data.Num(),
			Args.Sizes.GetData(),
			Args.Start,
			Args.Length);
	}

	// Convert data, if needed
	if constexpr (TIsStringStorageType_V<StorageType> || TIsDictionaryStorageType_V<StorageType>)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(TEXT("Converting HAPI_StringHandles to FStrings"))
		TArray<FString> StringArray;
		if (!FHoudiniEngineString::SHArrayToFStringArray(UnconvertedData, StringArray, Args.Session))
		{
			return HAPI_RESULT_FAILURE;
		}
		for (int32 i = 0; i < Args.Data.Num(); ++i)
		{
			Args.Data[i] = MoveTemp(StringArray[i]);
		}
	}
	else if constexpr (!std::is_same_v<GetDataType, UnrealDataType>)
	{
		H_SCOPED_FUNCTION_STATIC_LABEL(TEXT("Converting GetDataType to UnrealDataType"))
		for (int32 i = 0; i < Args.Data.Num(); ++i)
		{
			Args.Data[i] = static_cast<UnrealDataType>(DataArray[i]);
		}
	}

	return Result;
}

#define MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE, HAPI_SETTER) \
	HAPI_Result \
	SetAttribute(const TSetAttributeArgs<HAPI_STORAGETYPE> Args) \
	{ \
		return SetAttribute<HAPI_STORAGETYPE>(Args, HAPI_SETTER); \
	}

#define MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE, HAPI_SETTER) \
	HAPI_Result \
	SetAttribute(const TSetArrayAttributeArgs<HAPI_STORAGETYPE> Args) \
	{ \
		return SetAttribute<HAPI_STORAGETYPE>(Args, HAPI_SETTER); \
	}

#define MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE, HAPI_GETTER) \
	HAPI_Result \
	GetAttribute(const TGetAttributeArgs<HAPI_STORAGETYPE> Args) \
	{ \
		return GetAttribute<HAPI_STORAGETYPE>(Args, HAPI_GETTER); \
	}

#define MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE, HAPI_GETTER) \
	HAPI_Result \
	GetAttribute(const TGetArrayAttributeArgs<HAPI_STORAGETYPE> Args) \
	{ \
		return GetAttribute<HAPI_STORAGETYPE>(Args, HAPI_GETTER); \
	}

// Each HAPI storage type has a corresponding getter/setter in the API.
// All of the following mappings are compile-time template instatiations.

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT, FHoudiniApi::SetAttributeIntData)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT, FHoudiniApi::GetAttributeIntData)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT_ARRAY, FHoudiniApi::SetAttributeIntArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT_ARRAY, FHoudiniApi::GetAttributeIntArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_UINT8, FHoudiniApi::SetAttributeUInt8Data)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_UINT8, FHoudiniApi::GetAttributeUInt8Data)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_UINT8_ARRAY, FHoudiniApi::SetAttributeUInt8ArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_UINT8_ARRAY, FHoudiniApi::GetAttributeUInt8ArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT8, FHoudiniApi::SetAttributeInt8Data)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT8, FHoudiniApi::GetAttributeInt8Data)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT8_ARRAY, FHoudiniApi::SetAttributeInt8ArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT8_ARRAY, FHoudiniApi::GetAttributeInt8ArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT16, FHoudiniApi::SetAttributeInt16Data)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT16, FHoudiniApi::GetAttributeInt16Data)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT16_ARRAY, FHoudiniApi::SetAttributeInt16ArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT16_ARRAY, FHoudiniApi::GetAttributeInt16ArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT64, FHoudiniApi::SetAttributeInt64Data)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT64, FHoudiniApi::GetAttributeInt64Data)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_INT64_ARRAY, FHoudiniApi::SetAttributeInt64ArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_INT64_ARRAY, FHoudiniApi::GetAttributeInt64ArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_FLOAT, FHoudiniApi::SetAttributeFloatData)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_FLOAT, FHoudiniApi::GetAttributeFloatData)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_FLOAT_ARRAY, FHoudiniApi::SetAttributeFloatArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_FLOAT_ARRAY, FHoudiniApi::GetAttributeFloatArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_FLOAT64, FHoudiniApi::SetAttributeFloat64Data)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_FLOAT64, FHoudiniApi::GetAttributeFloat64Data)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_FLOAT64_ARRAY, FHoudiniApi::SetAttributeFloat64ArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_FLOAT64_ARRAY, FHoudiniApi::GetAttributeFloat64ArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_STRING, FHoudiniApi::SetAttributeStringData)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_STRING, FHoudiniApi::GetAttributeStringData)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_STRING_ARRAY, FHoudiniApi::SetAttributeStringArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_STRING_ARRAY, FHoudiniApi::GetAttributeStringArrayData)

MAP_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_DICTIONARY, FHoudiniApi::SetAttributeDictionaryData)
MAP_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_DICTIONARY, FHoudiniApi::GetAttributeDictionaryData)
MAP_ARRAY_STORAGETYPE_TO_SETTER(HAPI_STORAGETYPE_DICTIONARY_ARRAY, FHoudiniApi::SetAttributeDictionaryArrayData)
MAP_ARRAY_STORAGETYPE_TO_GETTER(HAPI_STORAGETYPE_DICTIONARY_ARRAY, FHoudiniApi::GetAttributeDictionaryArrayData)

/** 
 * This is an Unreal executable task which will call the passed in function with the passed in
 * argument struct. The passed in function is expected to return a HAPI_Result which can then be
 * retrieved using GetResult. We use this to make asynchronous HAPI calls using Unreal's task 
 * system.
 */
template<typename ArgsType>
class THapiTask : public FNonAbandonableTask
{
	friend class FAsyncTask<THapiTask>;

public:

	using FnType = TFunction<HAPI_Result(const ArgsType)>;

	THapiTask(FnType Fn, ArgsType Args)
		: Fn(MoveTemp(Fn))
		, Args(Args)
	{
	}

	/**
	 * Gets the HAPI_Result of the HAPI operation.
	 * It is undefined behaviour to call this before ensuring the task is completed.
	 */
	HAPI_Result
	GetResult() const
	{
		return Result;
	}

protected:

	void
	DoWork()
	{
		Result = Fn(Args);
	}

	TStatId
	GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(THapiTask, STATGROUP_ThreadPoolAsyncTasks);
	}

private:

	HAPI_Result Result;
	const FnType Fn;
	const ArgsType Args;
};

template<HAPI_StorageType StorageType>
struct TAsyncAttributeSetter
{
	static bool
	Set(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
	{
		using ArgsType = TSetAttributeArgs<StorageType>;
		using TaskType = THapiTask<ArgsType>;

		const int32 NumSessions = FHoudiniEngine::Get().GetNumSessions();
		TArray<TUniquePtr<FAsyncTask<TaskType>>> Tasks;

		const int32 NumValues = AttributeInfo.count;

		// Best if we evenly distribute the amount of work across all threads. Note we round up on
		// the division to ensure NumValuesPerSession * NumSessions >= NumValues.
		const int32 NumValuesPerSession = NumValues / NumSessions + (NumValues % NumSessions != 0);

		for (int32 SessionIndex = 0; SessionIndex < NumSessions; ++SessionIndex)
		{
			// Range for the async task to work on
			const int32 Start = NumValuesPerSession * SessionIndex;
			const int32 Length = FMath::Min(NumValuesPerSession, NumValues - Start);

			// Parameters for the async call
			ArgsType TaskArgs;
			TaskArgs.Session = FHoudiniEngine::Get().GetSession(SessionIndex);
			TaskArgs.NodeId = Args.NodeId;
			TaskArgs.PartId = Args.PartId;
			TaskArgs.AttributeName = Args.AttributeName;
			TaskArgs.AttributeInfo = &AttributeInfo;
			TaskArgs.Data = Data.Slice(
				Start * AttributeInfo.tupleSize, Length * AttributeInfo.tupleSize);
			TaskArgs.Start = Start;
			TaskArgs.Length = Length;
			TaskArgs.Flags = Args.Flags;

			// Start the async task
			Tasks.Add(MakeUnique<FAsyncTask<TaskType>>(
				[](const ArgsType Args) { return SetAttribute(Args); }, TaskArgs));
			Tasks.Last()->StartBackgroundTask();
		}

		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		for (auto&& Task : Tasks)
		{
			Task->EnsureCompletion();

			const HAPI_Result TaskResult = Task->GetTask().GetResult();
			if (TaskResult != HAPI_RESULT_SUCCESS)
			{
				Result = TaskResult;
			}
		}

		return Result == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TAttributeSetter
{
	static bool
	Set(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
	{
		TSetAttributeArgs<StorageType> SetAttributeArgs;
		SetAttributeArgs.Session = FHoudiniEngine::Get().GetSession();
		SetAttributeArgs.NodeId = Args.NodeId;
		SetAttributeArgs.PartId = Args.PartId;
		SetAttributeArgs.AttributeName = Args.AttributeName;
		SetAttributeArgs.AttributeInfo = &AttributeInfo;
		SetAttributeArgs.Data = Data;
		SetAttributeArgs.Start = 0;
		SetAttributeArgs.Length = AttributeInfo.count;
		SetAttributeArgs.Flags = Args.Flags;

		return SetAttribute(SetAttributeArgs) == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TAsyncArrayAttributeSetter
{
	static bool
	Set(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
		const TArrayView<const int32> Sizes)
	{
		using ArgsType = TSetArrayAttributeArgs<StorageType>;
		using TaskType = THapiTask<ArgsType>;

		const int32 NumSessions = FHoudiniEngine::Get().GetNumSessions();
		TArray<TUniquePtr<FAsyncTask<TaskType>>> Tasks;

		const int32 NumValues = AttributeInfo.totalArrayElements;

		// Best if we evenly distribute the amount of work across all threads. Note we round up on
		// the division to ensure NumValuesPerSession * NumSessions >= NumValues.
		const int32 NumValuesPerSession = NumValues / NumSessions + (NumValues % NumSessions != 0);

		int32 DataStart = 0;
		int32 SizesStart = 0;
		for (int32 SessionIndex = 0; SessionIndex < NumSessions; ++SessionIndex)
		{
			// We need to find the range for the async task to work on.
			// Keep adding more work for this task, until it has the minimum required work.
			int32 DataLength = 0;
			int32 SizesLength = 0;
			while (DataLength < NumValuesPerSession)
			{
				// We can exit early if there is no more work.
				if (SizesStart + SizesLength == Sizes.Num())
				{
					break;
				}

				DataLength += Sizes[SizesStart + SizesLength];
				++SizesLength;
			}

			// Parameters for the async call
			ArgsType TaskArgs;
			TaskArgs.Session = FHoudiniEngine::Get().GetSession(SessionIndex);
			TaskArgs.NodeId = Args.NodeId;
			TaskArgs.PartId = Args.PartId;
			TaskArgs.AttributeName = Args.AttributeName;
			TaskArgs.AttributeInfo = &AttributeInfo;
			TaskArgs.Data = Data.Slice(
				DataStart * AttributeInfo.tupleSize,
				DataLength * AttributeInfo.tupleSize);
			TaskArgs.Sizes = Sizes.Slice(SizesStart, SizesLength);
			TaskArgs.Start = SizesStart;
			TaskArgs.Length = SizesLength;
			TaskArgs.Flags = Args.Flags;

			// Start the async task
			Tasks.Add(MakeUnique<FAsyncTask<TaskType>>(
				[](const ArgsType Args) { return SetAttribute(Args); }, TaskArgs));
			Tasks.Last()->StartBackgroundTask();

			// Set starting position for next task
			DataStart += DataLength;
			SizesStart += SizesLength;
		}

		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		for (auto&& Task : Tasks)
		{
			Task->EnsureCompletion();

			const HAPI_Result TaskResult = Task->GetTask().GetResult();
			if (TaskResult != HAPI_RESULT_SUCCESS)
			{
				Result = TaskResult;
			}
		}

		return Result == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TArrayAttributeSetter
{
	static bool
	Set(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
		const TArrayView<const int32> Sizes)
	{
		TSetArrayAttributeArgs<StorageType> SetArrayAttributeArgs;
		SetArrayAttributeArgs.Session = FHoudiniEngine::Get().GetSession();
		SetArrayAttributeArgs.NodeId = Args.NodeId;
		SetArrayAttributeArgs.PartId = Args.PartId;
		SetArrayAttributeArgs.AttributeName = Args.AttributeName;
		SetArrayAttributeArgs.AttributeInfo = &AttributeInfo;
		SetArrayAttributeArgs.Data = Data;
		SetArrayAttributeArgs.Sizes = Sizes;
		SetArrayAttributeArgs.Start = 0;
		SetArrayAttributeArgs.Length = AttributeInfo.count;
		SetArrayAttributeArgs.Flags = Args.Flags;

		return SetAttribute(SetArrayAttributeArgs) == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TAsyncAttributeGetter
{
	static bool
	Get(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
	{
		using ArgsType = TGetAttributeArgs<StorageType>;
		using TaskType = THapiTask<ArgsType>;

		const int32 NumSessions = FHoudiniEngine::Get().GetNumSessions();
		TArray<TUniquePtr<FAsyncTask<TaskType>>> Tasks;

		const int32 NumValues = AttributeInfo.count;

		// Best if we evenly distribute the amount of work across all threads. Note we round up on the
		// division to ensure NumValuesPerSession * NumSessions >= NumValues.
		const int32 NumValuesPerSession = NumValues / NumSessions + (NumValues % NumSessions != 0);

		for (int32 SessionIndex = 0; SessionIndex < NumSessions; ++SessionIndex)
		{
			// Range for the async task to work on
			const int32 Start = NumValuesPerSession * SessionIndex;
			const int32 Length = FMath::Min(NumValuesPerSession, NumValues - Start);

			// Parameters for the async call
			ArgsType TaskArgs;
			TaskArgs.Session = FHoudiniEngine::Get().GetSession(SessionIndex);
			TaskArgs.NodeId = Args.NodeId;
			TaskArgs.PartId = Args.PartId;
			TaskArgs.AttributeName = Args.AttributeName;
			TaskArgs.AttributeInfo = &AttributeInfo;
			TaskArgs.Stride = -1;
			TaskArgs.Data = Data.Slice(
				Start * AttributeInfo.tupleSize, Length * AttributeInfo.tupleSize);
			TaskArgs.Start = Start;
			TaskArgs.Length = Length;
			TaskArgs.Flags = Args.Flags;

			// Start the async task
			Tasks.Add(MakeUnique<FAsyncTask<TaskType>>(
				[](const ArgsType Args) { return GetAttribute(Args); }, TaskArgs));
			Tasks.Last()->StartBackgroundTask();
		}

		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		for (auto&& Task : Tasks)
		{
			Task->EnsureCompletion();

			const HAPI_Result TaskResult = Task->GetTask().GetResult();
			if (TaskResult != HAPI_RESULT_SUCCESS)
			{
				Result = TaskResult;
			}
		}

		return Result == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TAttributeGetter
{
	static bool
	Get(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
	{
		TGetAttributeArgs<StorageType> GetAttributeArgs;
		GetAttributeArgs.Session = FHoudiniEngine::Get().GetSession();
		GetAttributeArgs.NodeId = Args.NodeId;
		GetAttributeArgs.PartId = Args.PartId;
		GetAttributeArgs.AttributeName = Args.AttributeName;
		GetAttributeArgs.AttributeInfo = &AttributeInfo;
		GetAttributeArgs.Stride = -1;
		GetAttributeArgs.Data = Data;
		GetAttributeArgs.Start = 0;
		GetAttributeArgs.Length = AttributeInfo.count;
		GetAttributeArgs.Flags = Args.Flags;

		return GetAttribute(GetAttributeArgs) == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TAsyncArrayAttributeGetter
{
	static bool
	Get(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
		const TArrayView<int32> Sizes)
	{
		using ArgsType = TGetArrayAttributeArgs<StorageType>;
		using TaskType = THapiTask<ArgsType>;

		// Since we do not know sizes ahead of time, we can only try evenly distribute the load
		// by attribute count. This will work well if generally all arrays have a similar size, but
		// if array size is extremely variable across attributes, some threads may get more work
		// than others.

		const int32 NumSessions = FHoudiniEngine::Get().GetNumSessions();
		TArray<TUniquePtr<FAsyncTask<TaskType>>> Tasks;

		// We will have to stitch these arrays back together at the end once tasks complete.
		TArray<TArray<typename TStorageTypeInfo<StorageType>::UnrealDataType>> TaskDataArrays;
		TArray<TArray<int32>> TaskSizesArrays;

		const int32 NumValues = AttributeInfo.count;

		// We round up on the division to ensure NumValuesPerSession * NumSessions >= NumValues.
		const int32 NumValuesPerSession = NumValues / NumSessions + (NumValues % NumSessions != 0);

		for (int32 SessionIndex = 0; SessionIndex < NumSessions; ++SessionIndex)
		{
			// Range for the async task to work on
			const int32 Start = NumValuesPerSession * SessionIndex;
			const int32 Length = FMath::Min(NumValuesPerSession, NumValues - Start);

			TaskDataArrays.Emplace();
			TaskDataArrays.Last().SetNumZeroed(
				AttributeInfo.totalArrayElements * AttributeInfo.tupleSize);
			TaskSizesArrays.Emplace();
			TaskSizesArrays.Last().SetNumUninitialized(Length);

			// Parameters for the async call
			ArgsType TaskArgs;
			TaskArgs.Session = FHoudiniEngine::Get().GetSession(SessionIndex);
			TaskArgs.NodeId = Args.NodeId;
			TaskArgs.PartId = Args.PartId;
			TaskArgs.AttributeName = Args.AttributeName;
			TaskArgs.AttributeInfo = &AttributeInfo;
			TaskArgs.Data = TaskDataArrays.Last();
			TaskArgs.Sizes = TaskSizesArrays.Last();
			TaskArgs.Start = Start;
			TaskArgs.Length = Length;
			TaskArgs.Flags = Args.Flags;

			// Start the async task
			Tasks.Add(MakeUnique<FAsyncTask<TaskType>>(
				[](const ArgsType Args) { GetAttribute(Args); }, TaskArgs));
			Tasks.Last()->StartBackgroundTask();
		}

		HAPI_Result Result = HAPI_RESULT_SUCCESS;

		int32 DataIndex = 0;
		int32 SizesIndex = 0;
		for (int32 TaskIndex = 0; TaskIndex < Tasks.Num(); ++TaskIndex)
		{
			Tasks[TaskIndex]->EnsureCompletion();

			const HAPI_Result TaskResult = Tasks[TaskIndex] ->GetTask().GetResult();
			if (TaskResult != HAPI_RESULT_SUCCESS)
			{
				Result = TaskResult;
			}

			int32 NumTaskValues = 0;
			// Move the receieved sizes from the task to the result array
			for (auto&& Size : TaskSizesArrays[TaskIndex])
			{
				NumTaskValues += Size;
				Sizes[SizesIndex++] = Size;
			}

			// Move the received data from the task to the result array
			for (int32 TaskDataIndex = 0; TaskDataIndex < NumTaskValues; ++TaskDataIndex)
			{
				Data[DataIndex++] = MoveTemp(TaskDataArrays[TaskIndex][TaskDataIndex]);
			}
		}

		return Result == HAPI_RESULT_SUCCESS;
	}
};

template<HAPI_StorageType StorageType>
struct TArrayAttributeGetter
{
	static bool
	Get(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
		const TArrayView<int32> Sizes)
	{
		TGetArrayAttributeArgs<StorageType> GetArrayAttributeArgs;
		GetArrayAttributeArgs.Session = FHoudiniEngine::Get().GetSession();
		GetArrayAttributeArgs.NodeId = Args.NodeId;
		GetArrayAttributeArgs.PartId = Args.PartId;
		GetArrayAttributeArgs.AttributeName = Args.AttributeName;
		GetArrayAttributeArgs.AttributeInfo = &AttributeInfo;
		GetArrayAttributeArgs.Data = Data;
		GetArrayAttributeArgs.Sizes = Sizes;
		GetArrayAttributeArgs.Start = 0;
		GetArrayAttributeArgs.Length = AttributeInfo.count;
		GetArrayAttributeArgs.Flags = Args.Flags;

		return GetAttribute(GetArrayAttributeArgs) == HAPI_RESULT_SUCCESS;
	}
};

template<
	HAPI_StorageType SrcStorageType,
	HAPI_StorageType DstStorageType>
bool
TryConvertDataArray(
	const char* AttributeName,
	const TArrayView<typename TStorageTypeInfo<SrcStorageType>::UnrealDataType> SrcData,
	const TArrayView<typename TStorageTypeInfo<DstStorageType>::UnrealDataType> DstData)
{
	if constexpr (TIsNumericStorageType_V<SrcStorageType> && TIsNumericStorageType_V<DstStorageType>)
	{
		for (int32 i = 0; i < SrcData.Num(); ++i)
		{
			// Perform conversion of different numeric types
			DstData[i] = static_cast<typename TStorageTypeInfo<DstStorageType>::UnrealDataType>(SrcData[i]);
		}

		HOUDINI_LOG_WARNING(
			CONVERSION_SUCCESS_LOG_MESSAGE,
			ANSI_TO_TCHAR(AttributeName),
			GetStorageTypeString(SrcStorageType),
			GetStorageTypeString(DstStorageType));

		return true;
	}
	else if constexpr (TIsStringStorageType_V<SrcStorageType> && TIsNumericStorageType_V<DstStorageType>)
	{
		for (int32 i = 0; i < SrcData.Num(); ++i)
		{
			// Try to convert the string into numeric type
			if (!LexTryParseString(DstData[i], *SrcData[i]))
			{
				HOUDINI_LOG_ERROR(
					CONVERSION_FAIL_LOG_MESSAGE,
					ANSI_TO_TCHAR(AttributeName),
					GetStorageTypeString(SrcStorageType),
					GetStorageTypeString(DstStorageType));
				return false;
			}
		}

		HOUDINI_LOG_WARNING(
			CONVERSION_SUCCESS_LOG_MESSAGE,
			ANSI_TO_TCHAR(AttributeName),
			GetStorageTypeString(SrcStorageType),
			GetStorageTypeString(DstStorageType));

		return true;
	}
	else if constexpr (TIsNumericStorageType_V<SrcStorageType> && TIsStringStorageType_V<DstStorageType>)
	{
		for (int32 i = 0; i < SrcData.Num(); ++i)
		{
			if constexpr (TIsIntStorageType_V<SrcStorageType>)
			{
				DstData[i] = FString::FromInt(SrcData[i]);
			}
			else if constexpr (TIsFloatStorageType_V<SrcStorageType>)
			{
				DstData[i] = FString::SanitizeFloat(SrcData[i]);
			}
		}

		HOUDINI_LOG_WARNING(
			CONVERSION_SUCCESS_LOG_MESSAGE,
			ANSI_TO_TCHAR(AttributeName),
			GetStorageTypeString(SrcStorageType),
			GetStorageTypeString(DstStorageType));

		return true;
	}
	else
	{
		// By default, prevent data type conversion.
		HOUDINI_LOG_ERROR(
			NO_CONVERSION_FOUND_LOG_MESSAGE,
			ANSI_TO_TCHAR(AttributeName),
			GetStorageTypeString(DstStorageType),
			GetStorageTypeString(SrcStorageType));

		return false;
	}
}

template<
	template<HAPI_StorageType> typename AttributeGetter,
	HAPI_StorageType SrcStorageType,
	HAPI_StorageType DstStorageType>
bool
GetAttributeWithTypeConversion(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<DstStorageType>::UnrealDataType> DstData)
{
	if constexpr (SrcStorageType == DstStorageType)
	{
		// No conversion needed, the type is correct
		return AttributeGetter<DstStorageType>::Get(Args, AttributeInfo, DstData);
	}
	else
	{
		TArray<typename TStorageTypeInfo<SrcStorageType>::UnrealDataType> SrcData;
		SrcData.SetNumZeroed(DstData.Num());

		if (!AttributeGetter<SrcStorageType>::Get(Args, AttributeInfo, SrcData))
		{
			return false;
		}

		return TryConvertDataArray<SrcStorageType, DstStorageType>(Args.AttributeName, SrcData, DstData);
	}
}

template<
	template<HAPI_StorageType> typename ArrayAttributeGetter,
	HAPI_StorageType SrcStorageType,
	HAPI_StorageType DstStorageType>
bool
GetArrayAttributeWithTypeConversion(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<DstStorageType>::UnrealDataType> DstData,
	const TArrayView<int32> Sizes)
{
	if constexpr (SrcStorageType == DstStorageType)
	{
		// No conversion needed, the type is correct
		return ArrayAttributeGetter<DstStorageType>::Get(Args, AttributeInfo, DstData, Sizes);
	}
	else
	{
		TArray<typename TStorageTypeInfo<SrcStorageType>::UnrealDataType> SrcData;
		SrcData.SetNumZeroed(DstData.Num());

		if (!ArrayAttributeGetter<SrcStorageType>::Get(Args, AttributeInfo, SrcData, Sizes))
		{
			return false;
		}

		return TryConvertDataArray<SrcStorageType, DstStorageType>(Args.AttributeName, SrcData, DstData);
	}
}

template<template<HAPI_StorageType> typename AttributeGetter, HAPI_StorageType StorageType>
bool
GetAttributeWithTypeConversion(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
{
	switch (AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_INT:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_INT, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_UINT8:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_UINT8, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_INT8:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_INT8, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_INT16:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_INT16, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_INT64:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_INT64, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_FLOAT:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_FLOAT, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_FLOAT64:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_FLOAT64, StorageType>(
			Args, AttributeInfo, Data);
	case HAPI_STORAGETYPE_STRING:
		return GetAttributeWithTypeConversion<AttributeGetter, HAPI_STORAGETYPE_STRING, StorageType>(
			Args, AttributeInfo, Data);
	default:
		HOUDINI_LOG_ERROR(
			NO_CONVERSION_FOUND_LOG_MESSAGE,
			ANSI_TO_TCHAR(Args.AttributeName),
			GetStorageTypeString(StorageType),
			GetStorageTypeString(AttributeInfo.storage));
		return false;
	}
}

template<template<HAPI_StorageType> typename ArrayAttributeGetter, HAPI_StorageType StorageType>
bool
GetArrayAttributeWithTypeConversion(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
	const TArrayView<int32> Sizes)
{
	switch (AttributeInfo.storage)
	{
	case HAPI_STORAGETYPE_INT_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_INT_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_UINT8_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_UINT8_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_INT8_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_INT8_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_INT16_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_INT16_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_INT64_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_INT64_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_FLOAT_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_FLOAT_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_FLOAT64_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_FLOAT64_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	case HAPI_STORAGETYPE_STRING_ARRAY:
		return GetArrayAttributeWithTypeConversion<ArrayAttributeGetter, HAPI_STORAGETYPE_STRING_ARRAY, StorageType>(
			Args, AttributeInfo, Data, Sizes);
	default:
		HOUDINI_LOG_ERROR(
			NO_CONVERSION_FOUND_LOG_MESSAGE,
			ANSI_TO_TCHAR(Args.AttributeName),
			GetStorageTypeString(StorageType),
			GetStorageTypeString(AttributeInfo.storage));
		return false;
	}
}

template<HAPI_StorageType StorageType>
bool
SetAttribute(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(
		FString::Printf(TEXT("SetAttribute (%s)"), ANSI_TO_TCHAR(Args.AttributeName)));

	using Flags = EHoudiniEngineAttributeFlags;

	if ((Args.Flags & Flags::AllowMultithreading) != Flags::None)
	{
		return TAsyncAttributeSetter<StorageType>::Set(Args, AttributeInfo, Data);
	}
	else
	{
		return TAttributeSetter<StorageType>::Set(Args, AttributeInfo, Data);
	}
}

template<HAPI_StorageType StorageType>
bool
SetArrayAttribute(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<const typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
	const TArrayView<const int32> Sizes)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(
		FString::Printf(TEXT("SetArrayAttribute (%s)"), ANSI_TO_TCHAR(Args.AttributeName)));

	using Flags = EHoudiniEngineAttributeFlags;

	if ((Args.Flags & Flags::AllowMultithreading) != Flags::None)
	{
		return TAsyncArrayAttributeSetter<StorageType>::Set(Args, AttributeInfo, Data, Sizes);
	}
	else
	{
		return TArrayAttributeSetter<StorageType>::Set(Args, AttributeInfo, Data, Sizes);
	}
}

template<HAPI_StorageType StorageType>
bool
GetAttributeNoResize(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(
		FString::Printf(TEXT("GetAttribute (%s)"), ANSI_TO_TCHAR(Args.AttributeName)));

	using Flags = EHoudiniEngineAttributeFlags;

	// TODO: We do not support multi threading string-like attributes. We need some pretty major
	// changes to how we get attributes in HAPI to be able to read string attributes in a thread
	// safe way. Ideally in the future the step of getting an attribute and reading a large buffer
	// is split.
	if ((Args.Flags & Flags::AllowMultithreading) != Flags::None 
		&& !TIsStringStorageType_V<StorageType> 
		&& !TIsDictionaryStorageType_V<StorageType>)
	{
		if ((Args.Flags & Flags::AllowTypeConversion) != Flags::None)
		{
			return GetAttributeWithTypeConversion<TAsyncAttributeGetter, StorageType>(
				Args, AttributeInfo, Data);
		}
		else
		{
			check(AttributeInfo.storage == StorageType);
			return TAsyncAttributeGetter<StorageType>::Get(Args, AttributeInfo, Data);
		}
	}
	else
	{
		if ((Args.Flags & Flags::AllowTypeConversion) != Flags::None)
		{
			return GetAttributeWithTypeConversion<TAttributeGetter, StorageType>(
				Args, AttributeInfo, Data);
		}
		else
		{
			check(AttributeInfo.storage == StorageType);
			return TAttributeGetter<StorageType>::Get(Args, AttributeInfo, Data);
		}
	}
}

template<HAPI_StorageType StorageType>
bool
GetAttribute(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& InAttributeInfo,
	TArray<typename TStorageTypeInfo<StorageType>::UnrealDataType>& OutData)
{
	OutData.SetNumZeroed(InAttributeInfo.count * InAttributeInfo.tupleSize);
	return GetAttributeNoResize<StorageType>(Args, InAttributeInfo, OutData);
}

template<HAPI_StorageType StorageType>
bool
GetArrayAttributeNoResize(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& AttributeInfo,
	const TArrayView<typename TStorageTypeInfo<StorageType>::UnrealDataType> Data,
	const TArrayView<int32> Sizes)
{
	H_SCOPED_FUNCTION_DYNAMIC_LABEL(
		FString::Printf(TEXT("GetArrayAttribute (%s)"), ANSI_TO_TCHAR(Args.AttributeName)));

	using Flags = EHoudiniEngineAttributeFlags;

	// TODO: Currently HAPI array attribute getter is not thread safe and does not work properly
	// on ranges of attributes that do not start at index 0. We also need a way to get array sizes
	// before the data to better partition the work. Enable this once things are fixed.
#define ENABLE_ASYNC_ARRAY_ATTRIBUTE_GET 0
#if ENABLE_ASYNC_ARRAY_ATTRIBUTE_GET
	if ((Args.Flags & Flags::AllowMultithreading) != Flags::None)
	{
		if ((Args.Flags & Flags::AllowTypeConversion) != Flags::None)
		{
			return GetArrayAttributeWithTypeConversion<TAsyncArrayAttributeGetter, StorageType>(
				Args, AttributeInfo, Data, Sizes);
		}
		else
		{
			check(AttributeInfo.storage == StorageType);
			return TAsyncArrayAttributeGetter<StorageType>::Get(Args, AttributeInfo, Data, Sizes);
		}
	}
	else
#endif // ENABLE_ASYNC_ARRAY_ATTRIBUTE_GET
	{
		if ((Args.Flags & Flags::AllowTypeConversion) != Flags::None)
		{
			return GetArrayAttributeWithTypeConversion<TArrayAttributeGetter, StorageType>(
				Args, AttributeInfo, Data, Sizes);
		}
		else
		{
			check(AttributeInfo.storage == StorageType);
			return TArrayAttributeGetter<StorageType>::Get(Args, AttributeInfo, Data, Sizes);
		}
	}
}

template<HAPI_StorageType StorageType>
bool
GetArrayAttribute(
	const FHoudiniEngineAttributeArgs& Args,
	const HAPI_AttributeInfo& InAttributeInfo,
	TArray<typename TStorageTypeInfo<StorageType>::UnrealDataType>& OutData,
	TArray<int32>& OutSizes)
{
	OutData.SetNumZeroed(InAttributeInfo.totalArrayElements * InAttributeInfo.tupleSize);

	OutSizes.SetNumUninitialized(InAttributeInfo.count);
	return GetArrayAttributeNoResize<StorageType>(Args, InAttributeInfo, OutData, OutSizes);
}

} // namespace

bool
FHoudiniEngineAttributes::GetInfo(
	const FHoudiniEngineAttributeArgs& Args,
	HAPI_AttributeInfo& OutAttributeInfo,
	const HAPI_AttributeOwner InOwner)
{
	H_SCOPED_FUNCTION_TIMER()

	FHoudiniApi::AttributeInfo_Init(&OutAttributeInfo);

	const auto GetInfo =
		[&](const HAPI_AttributeOwner Owner) -> bool
		{
			const HAPI_Result Result =  FHoudiniApi::GetAttributeInfo(
				FHoudiniEngine::Get().GetSession(),
				Args.NodeId,
				Args.PartId,
				Args.AttributeName,
				Owner,
				&OutAttributeInfo);

			return Result == HAPI_RESULT_SUCCESS && OutAttributeInfo.exists;
		};

	if (InOwner == HAPI_ATTROWNER_INVALID)
	{
		for (int32 OwnerIdx = 0; OwnerIdx < HAPI_ATTROWNER_MAX; ++OwnerIdx)
		{
			if (GetInfo(static_cast<HAPI_AttributeOwner>(OwnerIdx)))
			{
				return true;
			}
		}
		return false;
	}
	else
	{
		return GetInfo(InOwner);
	}
}

#define IMPLEMENT_ATTRIBUTE_TYPE(TYPE_NAME, HAPI_STORAGETYPE) \
	bool \
	FHoudiniEngineAttributes::Set##TYPE_NAME( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& AttributeInfo, \
		const TArrayView<const typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType> Data) \
	{ \
		return SetAttribute<HAPI_STORAGETYPE>(Args, AttributeInfo, Data); \
	} \
	bool \
	FHoudiniEngineAttributes::Get##TYPE_NAME##NoResize( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& AttributeInfo, \
		const TArrayView<typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType> Data) \
	{ \
		return GetAttributeNoResize<HAPI_STORAGETYPE>(Args, AttributeInfo, Data); \
	} \
	bool \
	FHoudiniEngineAttributes::Get##TYPE_NAME( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& InAttributeInfo, \
		TArray<typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType>& OutData) \
	{ \
		return GetAttribute<HAPI_STORAGETYPE>(Args, InAttributeInfo, OutData); \
	}

#define IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(TYPE_NAME, HAPI_STORAGETYPE) \
	bool \
	FHoudiniEngineAttributes::Set##TYPE_NAME( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& AttributeInfo, \
		const TArrayView<const typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType> Data, \
		const TArrayView<const int32> Sizes) \
	{ \
		return SetArrayAttribute<HAPI_STORAGETYPE>(Args, AttributeInfo, Data, Sizes); \
	} \
	bool \
	FHoudiniEngineAttributes::Get##TYPE_NAME##NoResize( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& AttributeInfo, \
		const TArrayView<typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType> Data, \
		const TArrayView<int32> Sizes) \
	{ \
		return GetArrayAttributeNoResize<HAPI_STORAGETYPE>(Args, AttributeInfo, Data, Sizes); \
	} \
	bool \
	FHoudiniEngineAttributes::Get##TYPE_NAME( \
		const FHoudiniEngineAttributeArgs& Args, \
		const HAPI_AttributeInfo& InAttributeInfo, \
		TArray<typename TStorageTypeInfo<HAPI_STORAGETYPE>::UnrealDataType>& OutData, \
		TArray<int32>& OutSizes) \
	{ \
		return GetArrayAttribute<HAPI_STORAGETYPE>(Args, InAttributeInfo, OutData, OutSizes); \
	}

IMPLEMENT_ATTRIBUTE_TYPE(UInt8, HAPI_STORAGETYPE_UINT8)
IMPLEMENT_ATTRIBUTE_TYPE(Int8, HAPI_STORAGETYPE_INT8)
IMPLEMENT_ATTRIBUTE_TYPE(Int16, HAPI_STORAGETYPE_INT16)
IMPLEMENT_ATTRIBUTE_TYPE(Int32, HAPI_STORAGETYPE_INT)
IMPLEMENT_ATTRIBUTE_TYPE(Int64, HAPI_STORAGETYPE_INT64)
IMPLEMENT_ATTRIBUTE_TYPE(Float, HAPI_STORAGETYPE_FLOAT)
IMPLEMENT_ATTRIBUTE_TYPE(Double, HAPI_STORAGETYPE_FLOAT64)
IMPLEMENT_ATTRIBUTE_TYPE(String, HAPI_STORAGETYPE_STRING)
IMPLEMENT_ATTRIBUTE_TYPE(Dictionary, HAPI_STORAGETYPE_DICTIONARY)

IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(UInt8Array, HAPI_STORAGETYPE_UINT8_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(Int8Array, HAPI_STORAGETYPE_INT8_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(Int16Array, HAPI_STORAGETYPE_INT16_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(Int32Array, HAPI_STORAGETYPE_INT_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(Int64Array, HAPI_STORAGETYPE_INT64_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(FloatArray, HAPI_STORAGETYPE_FLOAT_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(DoubleArray, HAPI_STORAGETYPE_FLOAT64_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(StringArray, HAPI_STORAGETYPE_STRING_ARRAY)
IMPLEMENT_ARRAY_ATTRIBUTE_TYPE(DictionaryArray, HAPI_STORAGETYPE_DICTIONARY_ARRAY)
