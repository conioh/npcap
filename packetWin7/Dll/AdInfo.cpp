/***********************IMPORTANT NPCAP LICENSE TERMS***********************
 *                                                                         *
 * Npcap is a Windows packet sniffing driver and library and is copyright  *
 * (c) 2013-2021 by Insecure.Com LLC ("The Nmap Project").  All rights     *
 * reserved.                                                               *
 *                                                                         *
 * Even though Npcap source code is publicly available for review, it is   *
 * not open source software and the free version may not be redistributed  *
 * or incorporated into other software without special permission from     *
 * the Nmap Project. It also has certain usage limitations described in    *
 * the LICENSE file included with Npcap and also available at              *
 * https://github.com/nmap/npcap/blob/master/LICENSE. This header          *
 * summarizes a few important aspects of the Npcap license, but is not a   *
 * substitute for that full Npcap license agreement.                       *
 *                                                                         *
 * We fund the Npcap project by selling two commercial licenses:           *
 *                                                                         *
 * The Npcap OEM Redistribution License allows companies distribute Npcap  *
 * OEM within their products. Licensees generally use the Npcap OEM        *
 * silent installer, ensuring a seamless experience for end                *
 * users. Licensees may choose between a perpetual unlimited license or    *
 * an annual term license, along with options for commercial support and   *
 * updates. Prices and details: https://nmap.org/npcap/oem/redist.html     *
 *                                                                         *
 * The Npcap OEM Internal-Use License is for organizations that wish to    *
 * use Npcap OEM internally, without redistribution outside their          *
 * organization. This allows them to bypass the 5-system usage cap of the  *
 * Npcap free edition. It includes commercial support and update options,  *
 * and provides the extra Npcap OEM features such as the silent installer  *
 * for automated deployment. Prices and details:                           *
 * https://nmap.org/npcap/oem/internal.html                                *
 *                                                                         *
 * Free and open source software producers are also welcome to contact us  *
 * for redistribution requests, but we normally recommend that such        *
 * authors instead ask their users to download and install Npcap           *
 * themselves.                                                             *
 *                                                                         *
 * Since the Npcap source code is available for download and review,       *
 * users sometimes contribute code patches to fix bugs or add new          *
 * features.  You are encouraged to submit such patches as Github pull     *
 * requests or by email to fyodor@nmap.org.  If you wish to specify        *
 * special license conditions or restrictions on your contributions, just  *
 * say so when you send them. Otherwise, it is understood that you are     *
 * offering the Nmap Project the unlimited, non-exclusive right to reuse,  *
 * modify, and relicence your code contributions so that we may (but are   *
 * not obligated to) incorporate them into Npcap.                          *
 *                                                                         *
 * This software is distributed in the hope that it will be useful, but    *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. Warranty rights    *
 * and commercial support are available for the OEM Edition described      *
 * above.                                                                  *
 *                                                                         *
 * Other copyright notices and attribution may appear below this license   *
 * header. We have kept those for attribution purposes, but any license    *
 * terms granted by those notices apply only to their original work, and   *
 * not to any changes made by the Nmap Project or to this entire file.     *
 *                                                                         *
 ***************************************************************************/
/*
 * Copyright (c) 1999 - 2005 NetGroup, Politecnico di Torino (Italy)
 * Copyright (c) 2005 - 2010 CACE Technologies, Davis (California)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Politecnico di Torino, CACE Technologies 
 * nor the names of its contributors may be used to endorse or promote 
 * products derived from this software without specific prior written 
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 This file contains the support functions used by packet.dll to retrieve information about installed 
 adapters, like

	- the adapter list
	- the device associated to any adapter and the description of the adapter
	- physical parameters like the linkspeed or the link layer type
	- the IP and link layer addresses  */

#define UNICODE 1

#include <Packet32.h>
#include "Packet32-Int.h"
#include "debug.h"

#include <iphlpapi.h>
#include <strsafe.h>
#include <WpcapNames.h>


extern BOOLEAN g_bLoopbackSupport;

ADINFO_LIST g_AdaptersInfoList = {0, 0, 0, NULL}; /// Head of the adapter information list.
HANDLE g_AdaptersInfoMutex = NULL; /// Mutex that protects the adapter information list.

#ifdef HAVE_AIRPCAP_API
extern AirpcapGetDeviceListHandler g_PAirpcapGetDeviceList;
extern AirpcapFreeDeviceListHandler g_PAirpcapFreeDeviceList;
#endif /* HAVE_AIRPCAP_API */

/*!
  \brief Adds an entry to the adapter description list.
  \param AdName Name of the adapter to add
  \return If the function succeeds, the return value is nonzero.

  Used by PacketGetAdaptersNPF(). Queries the driver to fill the PADAPTER_INFO describing the new adapter.
*/
_Success_(return != 0)
static BOOLEAN PacketAddAdapterNPF(_In_ PIP_ADAPTER_ADDRESSES pAdapterAddr,
		_Outptr_result_nullonfailure_ PADAPTER_INFO *ppAdInfo
		)
{
	LONG		Status;
	HANDLE hAdapter = INVALID_HANDLE_VALUE;
	PADAPTER_INFO	TmpAdInfo;
	CHAR AdName[ADAPTER_NAME_LENGTH];
	PCHAR NameEnd = NULL;
	HRESULT hrStatus = S_OK;
	
	TRACE_ENTER();
	assert(pAdapterAddr != NULL);
	assert(ppAdInfo != NULL);
 	TRACE_PRINT1("Trying to add adapter %hs", pAdapterAddr->AdapterName);
	*ppAdInfo = NULL;
	
	// Create the NPF device name from the original device name
	hrStatus = StringCchPrintfA(AdName,
		sizeof(AdName),
		"%s%s",
		NPF_DRIVER_COMPLETE_DEVICE_PREFIX,
		pAdapterAddr->AdapterName);
	if (FAILED(hrStatus)) {
		TRACE_PRINT("PacketAddAdapterNPF: adapter name is too long to be stored into ADAPTER_INFO::Name, simply skip it");
		TRACE_EXIT();
		return FALSE;
	}

	TRACE_PRINT("Trying to open the NPF adapter and see if it's available...");

	// Try to Open the adapter
	hAdapter = PacketGetAdapterHandle(AdName);

	if(hAdapter == INVALID_HANDLE_VALUE)
	{
		TRACE_PRINT("NPF Adapter not available, do not add it to the global list");
		// We are not able to open this adapter. Skip to the next one.
		TRACE_EXIT();
		return FALSE;
	}

	CloseHandle(hAdapter);
	
	//
	// PacketOpenAdapter was succesful. Consider this a valid adapter and allocate an entry for it
	// In the adapter list
	//
	
	TmpAdInfo = (PADAPTER_INFO) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ADAPTER_INFO));
	if (TmpAdInfo == NULL) 
	{
		TRACE_PRINT("AddAdapter: HeapAlloc Failed allocating the buffer for the AdInfo to be added to the global list. Returning.");
		TRACE_EXIT();
		return FALSE;
	}
	
	// Copy the device name
	hrStatus = StringCchCopyExA(TmpAdInfo->Name, sizeof(TmpAdInfo->Name), AdName, &NameEnd, NULL, 0);
	if (FAILED(hrStatus)) {
		HeapFree(GetProcessHeap(), 0, TmpAdInfo);
		TRACE_EXIT();
		return FALSE;
	}
	TmpAdInfo->NameLen = (ULONG)(NameEnd - TmpAdInfo->Name);

	//we do not need to terminate the string TmpAdInfo->Name, since we have left a char at the end, and
	//the memory for TmpAdInfo was zeroed upon allocation

	// Copy the description
	// -1 for cchWideChar means returned length will _include_ null terminator
	Status = WideCharToMultiByte(CP_ACP, 0, pAdapterAddr->Description, -1, TmpAdInfo->Description, ADAPTER_DESC_LENGTH, NULL, NULL);
	// Conversion error? ensure it's terminated and ignore.
	if (Status <= 0) {
		TmpAdInfo->Description[ADAPTER_DESC_LENGTH] = '\0';
		Status = 0; // Length at this point includes the null terminator
	}
	TmpAdInfo->DescLen = Status - 1; // ADAPTER_INFO lengths do *not* include null terminator.

	// Update the AdaptersInfo list
	*ppAdInfo = TmpAdInfo;
	
	TRACE_PRINT("PacketAddAdapterNPF: Adapter successfully added to the list");
	TRACE_EXIT();
	return TRUE;
}

_Success_(return != 0)
static BOOLEAN PacketAddLoopbackAdapter(
		_Outptr_result_nullonfailure_ PADAPTER_INFO *ppAdInfo
		)
{
	PADAPTER_INFO TmpAdInfo = NULL;
	HRESULT hrStatus = S_OK;
	PCHAR NameEnd = NULL;

	TRACE_ENTER();
	assert(ppAdInfo != NULL);
	*ppAdInfo = NULL;

	TmpAdInfo = (PADAPTER_INFO)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ADAPTER_INFO));
	if (TmpAdInfo == NULL)
	{
		TRACE_PRINT("AddAdapter: HeapAlloc Failed");
		return FALSE;
	}

	// Copy the device name
	hrStatus = StringCchCopyExA(TmpAdInfo->Name, sizeof(TmpAdInfo->Name), FAKE_LOOPBACK_ADAPTER_NAME, &NameEnd, NULL, 0);
	if (FAILED(hrStatus)) {
		HeapFree(GetProcessHeap(), 0, TmpAdInfo);
		TRACE_EXIT();
		return FALSE;
	}
	TmpAdInfo->NameLen = (ULONG)(NameEnd - TmpAdInfo->Name);
	hrStatus = StringCchCopyExA(TmpAdInfo->Description, sizeof(TmpAdInfo->Description), FAKE_LOOPBACK_ADAPTER_DESCRIPTION, &NameEnd, NULL, 0);
	if (FAILED(hrStatus)) {
		HeapFree(GetProcessHeap(), 0, TmpAdInfo);
		TRACE_EXIT();
		return FALSE;
	}
	TmpAdInfo->DescLen = (ULONG)(NameEnd - TmpAdInfo->Description);

	*ppAdInfo = TmpAdInfo;
	return TRUE;
}

/*!
  \brief Updates the list of the adapters querying the registry.
  \return If the function succeeds, the return value is nonzero.

  This function populates the list of adapter descriptions, retrieving the information from the registry. 
*/
static BOOLEAN PacketGetAdaptersNPF()
{
	static ULONG MaxGAABufLen = ADAPTERS_ADDRESSES_INITIAL_BUFFER_SIZE;
	ULONG Iterations;
	ULONG BufLen;
	ULONG RetVal = ERROR_SUCCESS;
	PIP_ADAPTER_ADDRESSES AdBuffer, TmpAddr;
	PADAPTER_INFO TmpAdInfo = NULL;

	TRACE_ENTER();


	BufLen = MaxGAABufLen;
	AdBuffer = (PIP_ADAPTER_ADDRESSES)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, BufLen);
	if (AdBuffer == NULL)
	{
		TRACE_PRINT("PacketGetAdaptersNPF: HeapAlloc Failed");
		TRACE_EXIT();
		return FALSE;
	}
	for (Iterations = 0; Iterations < ADAPTERS_ADDRESSES_MAX_TRIES; Iterations++)
	{

		RetVal = GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_ALL_INTERFACES | // Get everything
			GAA_FLAG_SKIP_DNS_INFO | // Undocumented, reported to help avoid errors on Win10 1809
			// We don't use any of these features:
			GAA_FLAG_SKIP_DNS_SERVER |
			GAA_FLAG_SKIP_UNICAST | // We don't need any address info, just names
			GAA_FLAG_SKIP_ANYCAST |
			GAA_FLAG_SKIP_MULTICAST |
			GAA_FLAG_SKIP_FRIENDLY_NAME, NULL, AdBuffer, &BufLen);
		if (RetVal == ERROR_BUFFER_OVERFLOW)
		{
			TRACE_PRINT1("PacketGetAdaptersNPF: GetAdaptersAddresses Too small buffer (need %u)", BufLen);
			TmpAddr = (PIP_ADAPTER_ADDRESSES)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, AdBuffer, BufLen);
			if (TmpAddr == NULL)
			{
				TRACE_PRINT("PacketGetAdaptersNPF: HeapReAlloc Failed");
				HeapFree(GetProcessHeap(), 0, AdBuffer);
				TRACE_EXIT();
				return FALSE;
			}
			AdBuffer = TmpAddr;
		}
		else
		{
			break;
		}
	}

	if (RetVal != ERROR_SUCCESS)
	{
		TRACE_PRINT("PacketGetAdaptersNPF: GetAdaptersAddresses Failed while retrieving the addresses");
		if (AdBuffer)
		{
			HeapFree(GetProcessHeap(), 0, AdBuffer);
		}
		TRACE_EXIT();
		return FALSE;
	}

	// Stash the value that worked here
	InterlockedMax(&MaxGAABufLen, BufLen);

	for (TmpAddr=AdBuffer; TmpAddr != NULL; TmpAddr = TmpAddr->Next)
	{
		// If the adapter is valid, add it to the list.
		if (PacketAddAdapterNPF(TmpAddr, &TmpAdInfo)) {
			TmpAdInfo->Next = g_AdaptersInfoList.Adapters;
			g_AdaptersInfoList.Adapters = TmpAdInfo;
			// The info list lengths *include* the null terminators.
			g_AdaptersInfoList.NamesLen += TmpAdInfo->NameLen + 1;
			g_AdaptersInfoList.DescsLen += TmpAdInfo->DescLen + 1;
		}
	}
	
	if (g_bLoopbackSupport && PacketAddLoopbackAdapter(&TmpAdInfo)) {
		TmpAdInfo->Next = g_AdaptersInfoList.Adapters;
		g_AdaptersInfoList.Adapters = TmpAdInfo;
		// The info list lengths *include* the null terminators.
		g_AdaptersInfoList.NamesLen += TmpAdInfo->NameLen + 1;
		g_AdaptersInfoList.DescsLen += TmpAdInfo->DescLen + 1;
	}

	if (AdBuffer)
	{
		HeapFree(GetProcessHeap(), 0, AdBuffer);
	}
	TRACE_EXIT();
	return TRUE;
}

#ifdef HAVE_AIRPCAP_API
/*!
  \brief Add an airpcap adapter to the adapters info list, gathering information from the airpcap dll
  \param name Name of the adapter.
  \param description description of the adapter.
  \return If the function succeeds, the return value is nonzero.
*/
_Success_(return != 0)
static BOOLEAN PacketAddAdapterAirpcap(_In_ PCCH name, _In_ PCCH description,
		_Outptr_result_nullonfailure_ PADAPTER_INFO *ppAdInfo)
{
	//this function should acquire the g_AdaptersInfoMutex, since it's NOT called with an ADAPTER_INFO as parameter
	PADAPTER_INFO TmpAdInfo = NULL;
	BOOLEAN Result = FALSE;
	HRESULT hrStatus = S_OK;
	PCHAR NameEnd = NULL;

	TRACE_ENTER();
	assert(ppAdInfo != NULL);
	*ppAdInfo = NULL;
	
	do
	{
		//
		// Allocate a descriptor for this adapter
		//			
		TmpAdInfo = (PADAPTER_INFO) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(ADAPTER_INFO));
		if (TmpAdInfo == NULL) 
		{
			TRACE_PRINT("PacketAddAdapterAirpcap: HeapAlloc Failed");
			break;
		}
		
		// Copy the device name and description
		hrStatus = StringCchCopyExA(TmpAdInfo->Name, sizeof(TmpAdInfo->Name), name, &NameEnd, NULL, 0);
		if (FAILED(hrStatus)) {
			break;
		}
		TmpAdInfo->NameLen = (ULONG)(NameEnd - TmpAdInfo->Name);
		
		hrStatus = StringCchCopyExA(TmpAdInfo->Description, sizeof(TmpAdInfo->Description), description, &NameEnd, NULL, 0);
		if (FAILED(hrStatus)) {
			break;
		}
		TmpAdInfo->DescLen = (ULONG)(NameEnd - TmpAdInfo->Description);
		
		Result = TRUE;
		*ppAdInfo = TmpAdInfo;
	}
	while(FALSE);

	if (!Result && TmpAdInfo) {
		HeapFree(GetProcessHeap(), 0, TmpAdInfo);
	}

	TRACE_EXIT();
	return Result;
}

/*!
  \brief Updates the list of the adapters using the airpcap dll.
  \return If the function succeeds, the return value is nonzero.

  This function populates the list of adapter descriptions, looking for AirPcap cards on the system. 
*/
static BOOLEAN PacketGetAdaptersAirpcap()
{
	CHAR Ebuf[AIRPCAP_ERRBUF_SIZE];
	AirpcapDeviceDescription *Devs = NULL, *TmpDevs;
	UINT i;
	
	TRACE_ENTER();

	if(!g_PAirpcapGetDeviceList(&Devs, Ebuf))
	{
		// No airpcap cards found on this system
		TRACE_PRINT1("No AirPcap adapters found: %s", Ebuf);
		TRACE_EXIT();
		return FALSE;
	}
	for(TmpDevs = Devs, i = 0; TmpDevs != NULL; TmpDevs = TmpDevs->next)
	{
		PADAPTER_INFO TmpAdInfo = NULL;
		// If the adapter is valid, add it to the list.
		if (PacketAddAdapterAirpcap(TmpDevs->Name, TmpDevs->Description, &TmpAdInfo)) {
			TmpAdInfo->Next = g_AdaptersInfoList.Adapters;
			g_AdaptersInfoList.Adapters = TmpAdInfo;
			// The info list lengths *include* the null terminators.
			g_AdaptersInfoList.NamesLen += TmpAdInfo->NameLen + 1;
			g_AdaptersInfoList.DescsLen += TmpAdInfo->DescLen + 1;
		}
	}
	
	g_PAirpcapFreeDeviceList(Devs);
	
	TRACE_EXIT();
	return TRUE;
}
#endif // HAVE_AIRPCAP_API


/*!
  \brief Populates the list of the adapters.

  This function populates the list of adapter descriptions, invoking PacketGetAdapters*()
*/
_Use_decl_annotations_
DWORD PacketPopulateAdaptersInfoList()
{
	//this function should acquire the g_AdaptersInfoMutex, since it's NOT called with an ADAPTER_INFO as parameter
	PADAPTER_INFO TAdInfo;
	PVOID Mem2;
	DWORD dwError = ERROR_SUCCESS;
	ULONGLONG Now = GetTickCount64();

	TRACE_ENTER();

	WaitForSingleObject(g_AdaptersInfoMutex, INFINITE);

	if (Now - g_AdaptersInfoList.TicksLastUpdate < ADINFO_LIST_STALE_TICK_COUNT) {
		// Data is still valid
		ReleaseMutex(g_AdaptersInfoMutex);
		return ERROR_SUCCESS;
	}

	if(g_AdaptersInfoList.Adapters)
	{
		// Free the old list
		TAdInfo = g_AdaptersInfoList.Adapters;
		while(TAdInfo != NULL)
		{
			Mem2 = TAdInfo;
			TAdInfo = TAdInfo->Next;
			
			HeapFree(GetProcessHeap(), 0, Mem2);
		}
		
		g_AdaptersInfoList.Adapters = NULL;
	}
	// Each list is terminated with an empty string, so length of list is 1 + total length
	g_AdaptersInfoList.NamesLen = 1;
	g_AdaptersInfoList.DescsLen = 1;

	//
	// Fill the new list
	//
	if(!PacketGetAdaptersNPF())
	{
		dwError = GetLastError();
		// No info about adapters in the registry. (NDIS adapters, i.e. exported by NPF)
		TRACE_PRINT("PacketPopulateAdaptersInfoList: registry scan for adapters failed!");
	}

#ifdef HAVE_AIRPCAP_API
	if(g_PAirpcapGetDeviceList)	// Ensure that the airpcap dll is present
	{
		if(!PacketGetAdaptersAirpcap())
		{
			if (dwError == ERROR_SUCCESS) {
				dwError = GetLastError();
			}
			TRACE_PRINT("PacketPopulateAdaptersInfoList: lookup of airpcap adapters failed!");
		}
	}
#endif // HAVE_AIRPCAP_API

	if (g_AdaptersInfoList.Adapters == NULL && dwError == ERROR_SUCCESS) {
		dwError = ERROR_NO_MORE_ITEMS;
	}

	ReleaseMutex(g_AdaptersInfoMutex);
	TRACE_EXIT();
	return dwError;
}
