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

#pragma once

#include "HAPI/HAPI_Common.h"
#include "HoudiniEnginePrivatePCH.h"

struct FHoudiniRawAttributeData;

struct FHoudiniHapiAccessor
{
	// Initialization functions. use these functions to initialize the accessor.
	FHoudiniHapiAccessor(HAPI_NodeId NodeId, HAPI_NodeId PartId, const char* Name);
	FHoudiniHapiAccessor() {}
	void Init(HAPI_NodeId InNodeId, HAPI_NodeId InPartId, const char * InName);

	// Get HAPI_AttributeInfo from the accessor. 
	bool GetInfo(HAPI_AttributeInfo& OutAttributeInfo, HAPI_AttributeOwner InOwner = HAPI_ATTROWNER_INVALID);

	// Templated functions to return data.
	//
	// If the attribute is a different from the templated data, conversion is optionally performed (bAllowTypeConversion)
	// If the attribute owner is HAPI_ATTROWNER_INVALID, all attribute owners are searched.
	// An optional tuple size can be specified.
	// Note these templates are explicitly defined in the .cpp file.

	template<typename DataType> bool GetAttributeData(HAPI_AttributeOwner Owner, TArray<DataType>& Results, int First=0, int Count=-1);
	template<typename DataType> bool GetAttributeData(HAPI_AttributeOwner Owner, DataType* Results, int First = 0, int Count = -1);
	template<typename DataType> bool GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, TArray<DataType>& Results, int First = 0, int Count = -1);
	template<typename DataType> bool GetAttributeData(HAPI_AttributeOwner Owner, int MaxTuples, DataType* Results, int First = 0, int Count = -1);
	template<typename DataType> bool GetAttributeFirstValue(HAPI_AttributeOwner Owner, DataType & Result);

	template<typename DataType> bool GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, TArray<DataType>& Results, int First=0, int Count=-1);
	template<typename DataType> bool GetAttributeData(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int First, int Count);
	template<typename DataType> bool GetAttributeDataMain(const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int First, int Count);

	// Template functions to set data.
	//

	template<typename DataType> bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const TArray<DataType>& Data);
	template<typename DataType> bool SetAttributeData(const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int First, int Count);
	template<typename DataType> bool SetAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, const DataType* Data, int StartIndex, int Count);

	int CalculateNumberOfSessions() const;


	bool GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data);
	bool GetRawAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FHoudiniRawAttributeData& Data, int Start, int Count) const;

	HAPI_NodeId NodeId = -1;
	HAPI_PartId PartId = -1;
	const char* AttributeName = nullptr;
	bool bAllowTypeConversion = true;
	bool bAllowMultiThreading = true;
	bool bCanBeArray = false;

	template<typename DataType> bool GetAttributeData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, DataType* Results, int Start, int Count) const;

	// Internal functions for actually getting data from HAPI.
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, uint8* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int8* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int16* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int64* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, double* Data, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiData(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FString* Data, int IndexStart, int IndexCount) const;

	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, uint8* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int8* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int16* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, int64* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, float* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, double* Data, int* Sizes, int IndexStart, int IndexCount) const;
	HAPI_Result FetchHapiDataArray(const HAPI_Session* Session, const HAPI_AttributeInfo& AttributeInfo, FString* Data, int* Sizes, int IndexStart, int IndexCount) const;

	// Data conversion functions.
	template<typename DataType> static void ConvertFromRawData(const FHoudiniRawAttributeData& RawData, DataType* Data, size_t Count);
		template<typename SrcType, typename DestType> static void Convert(const SrcType* SourceData, DestType* DestData, int Count);
	static FString ToString(int32 Number);
	static FString ToString(float Number);
	static FString ToString(double Number);
	static double ToDouble(const FString& Str);
	static int ToInt(const FString& Str);

	template<typename DataType>
	static HAPI_StorageType GetHapiType();

	static bool IsHapiArrayType(HAPI_StorageType);

};



