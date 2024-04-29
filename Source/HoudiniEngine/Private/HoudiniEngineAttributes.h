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

#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"

enum class EHoudiniEngineAttributeFlags : uint8
{
	None = 0,

	/**
	 * Allow large attribute data operations to be distributed across multiple threads using
	 * seperate sessions.
	 */
	AllowMultithreading = 1 << 0,

	/**
	 * Allow attribute data to be converted into a different type if there is a storage type
	 * mismatch. Successful conversions are logged as warnings and failed conversions are logged as
	 * errors.
	 *
	 * @note Currently only supported when getting attributes.
	 */
	AllowTypeConversion = 1 << 1,

	// TODO: Add run length encoding option here. We can re-use run length encoding algorithm in
	// HoudiniEngineUtils.cpp - consider moving it instead to HoudiniAlgorithm.h
};
ENUM_CLASS_FLAGS(EHoudiniEngineAttributeFlags)

/** Arguments for HAPI attribute operations. */
struct FHoudiniEngineAttributeArgs
{
	HAPI_NodeId NodeId = -1;
	HAPI_PartId PartId = -1;
	const char* AttributeName = nullptr;

	/** Can be optionally set by caller. In most cases these default flags are good. */
	EHoudiniEngineAttributeFlags Flags =
		EHoudiniEngineAttributeFlags::AllowMultithreading;
};

/**
 * Statics for working with Houdini attribute values. Using these wrappers is preferred over calling
 * HAPI directly as these can have Unreal Engine specific optimizations. They are also designed to
 * work with Unreal Engine's native types, and will convert between the types used by Unreal Engine
 * and HAPI when necessary.
 */
class FHoudiniEngineAttributes
{
public:

	static bool GetInfo(
		const FHoudiniEngineAttributeArgs& Args,
		HAPI_AttributeInfo& OutAttributeInfo,
		const HAPI_AttributeOwner InOwner = HAPI_ATTROWNER_INVALID);

	// TODO(alexanderk)
	// These functions are implemented via IMPLEMENT_ATTRIBUTE_TYPE and IMPLEMENT_ARRAY_ATTRIBUTE_TYPE macros in HoudiniEngineAttributes.cpp

	static bool SetUInt8(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const uint8> Data);

	static bool GetUInt8NoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<uint8> Data);

	static bool GetUInt8(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<uint8>& OutData);

	static bool SetUInt8Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const uint8> Data,
		const TArrayView<const int32> Sizes);

	static bool GetUInt8ArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<uint8> Data,
		const TArrayView<int32> Sizes);

	static bool GetUInt8Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<uint8>& OutData,
		TArray<int32>& OutSizes);

	static bool SetInt8(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int8> Data);

	static bool GetInt8NoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int8> Data);

	static bool GetInt8(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int8>& OutData);

	static bool SetInt8Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int8> Data,
		const TArrayView<const int32> Sizes);

	static bool GetInt8ArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int8> Data,
		const TArrayView<int32> Sizes);

	static bool GetInt8Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int8>& OutData,
		TArray<int32>& OutSizes);

	static bool SetInt16(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int16> Data);

	static bool GetInt16NoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int16> Data);

	static bool GetInt16(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int16>& OutData);

	static bool SetInt16Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int16> Data,
		const TArrayView<const int32> Sizes);

	static bool GetInt16ArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int16> Data,
		const TArrayView<int32> Sizes);

	static bool GetInt16Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int16>& OutData,
		TArray<int32>& OutSizes);

	static bool SetInt32(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int32> Data);

	static bool GetInt32NoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int32> Data);

	static bool GetInt32(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int32>& OutData);

	static bool SetInt32Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int32> Data,
		const TArrayView<const int32> Sizes);

	static bool GetInt32ArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int32> Data,
		const TArrayView<int32> Sizes);

	static bool GetInt32Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int32>& OutData,
		TArray<int32>& OutSizes);

	static bool SetInt64(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int64> Data);

	static bool GetInt64NoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int64> Data);

	static bool GetInt64(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int64>& OutData);

	static bool SetInt64Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const int64> Data,
		const TArrayView<const int32> Sizes);

	static bool GetInt64ArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<int64> Data,
		const TArrayView<int32> Sizes);

	static bool GetInt64Array(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<int64>& OutData,
		TArray<int32>& OutSizes);

	static bool SetFloat(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const float> Data);

	static bool GetFloatNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<float> Data);

	static bool GetFloat(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<float>& OutData);

	static bool SetFloatArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const float> Data,
		const TArrayView<const int32> Sizes);

	static bool GetFloatArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<float> Data,
		const TArrayView<int32> Sizes);

	static bool GetFloatArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<float>& OutData,
		TArray<int32>& OutSizes);

	static bool SetDouble(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const double> Data);

	static bool GetDoubleNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<double> Data);

	static bool GetDouble(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<double>& OutData);

	static bool SetDoubleArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const double> Data,
		const TArrayView<const int32> Sizes);

	static bool GetDoubleArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<double> Data,
		const TArrayView<int32> Sizes);

	static bool GetDoubleArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<double>& OutData,
		TArray<int32>& OutSizes);

	static bool SetString(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const FString> Data);

	static bool GetStringNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<FString> Data);

	static bool GetString(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<FString>& OutData);

	static bool SetStringArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const FString> Data,
		const TArrayView<const int32> Sizes);

	static bool GetStringArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<FString> Data,
		const TArrayView<int32> Sizes);

	static bool GetStringArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<FString>& OutData,
		TArray<int32>& OutSizes);

	static bool SetDictionary(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const FString> Data);

	static bool GetDictionaryNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<FString> Data);

	static bool GetDictionary(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<FString>& OutData);

	static bool SetDictionaryArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<const FString> Data,
		const TArrayView<const int32> Sizes);

	static bool GetDictionaryArrayNoResize(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& AttributeInfo,
		const TArrayView<FString> Data,
		const TArrayView<int32> Sizes);

	static bool GetDictionaryArray(
		const FHoudiniEngineAttributeArgs& Args,
		const HAPI_AttributeInfo& InAttributeInfo,
		TArray<FString>& OutData,
		TArray<int32>& OutSizes);
};
