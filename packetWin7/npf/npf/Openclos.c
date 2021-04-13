/***********************IMPORTANT NPCAP LICENSE TERMS***********************
 *                                                                         *
 * Npcap is a Windows packet sniffing driver and library and is copyright  *
 * (c) 2013-2020 by Insecure.Com LLC ("The Nmap Project").  All rights     *
 * reserved.                                                               *
 *                                                                         *
 * Even though Npcap source code is publicly available for review, it is   *
 * not open source software and may not be redistributed or incorporated   *
 * into other software without special permission from the Nmap Project.   *
 * We fund the Npcap project by selling a commercial license which allows  *
 * companies to redistribute Npcap with their products and also provides   *
 * for support, warranty, and indemnification rights.  For details on      *
 * obtaining such a license, please contact:                               *
 *                                                                         *
 * sales@nmap.com                                                          *
 *                                                                         *
 * Free and open source software producers are also welcome to contact us  *
 * for redistribution requests.  However, we normally recommend that such  *
 * authors instead ask your users to download and install Npcap            *
 * themselves.                                                             *
 *                                                                         *
 * Since the Npcap source code is available for download and review,       *
 * users sometimes contribute code patches to fix bugs or add new          *
 * features.  By sending these changes to the Nmap Project (including      *
 * through direct email or our mailing lists or submitting pull requests   *
 * through our source code repository), it is understood unless you        *
 * specify otherwise that you are offering the Nmap Project the            *
 * unlimited, non-exclusive right to reuse, modify, and relicence your     *
 * code contribution so that we may (but are not obligated to)             *
 * incorporate it into Npcap.  If you wish to specify special license      *
 * conditions or restrictions on your contributions, just say so when you  *
 * send them.                                                              *
 *                                                                         *
 * This software is distributed in the hope that it will be useful, but    *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.                    *
 *                                                                         *
 * Other copyright notices and attribution may appear below this license   *
 * header. We have kept those for attribution purposes, but any license    *
 * terms granted by those notices apply only to their original work, and   *
 * not to any changes made by the Nmap Project or to this entire file.     *
 *                                                                         *
 * This header summarizes a few important aspects of the Npcap license,    *
 * but is not a substitute for the full Npcap license agreement, which is  *
 * in the LICENSE file included with Npcap and also available at           *
 * https://github.com/nmap/npcap/blob/master/LICENSE.                      *
 *                                                                         *
 ***************************************************************************/
/*
* Copyright (c) 1999 - 2005 NetGroup, Politecnico di Torino (Italy)
* Copyright (c) 2005 - 2010 CACE Technologies, Davis (California)
* Copyright (c) 2010 - 2013 Riverbed Technology, San Francisco (California), Yang Luo (China)
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

#include "stdafx.h"

#include "packet.h"
#include "Loopback.h"
#include "Lo_send.h"
#include "..\..\..\Common\WpcapNames.h"

extern NDIS_STRING g_LoopbackAdapterName;
extern NDIS_STRING g_SendToRxAdapterName;
extern NDIS_STRING g_BlockRxAdapterName;
extern NDIS_STRING devicePrefix;
extern ULONG g_Dot11SupportMode;

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	extern HANDLE g_WFPEngineHandle;
#endif

ULONG g_NumLoopbackInstances = 0;

extern SINGLE_LIST_ENTRY g_arrFiltMod; //Adapter filter module list head, each list item is a group head.
extern NDIS_SPIN_LOCK g_FilterArrayLock; //The lock for adapter filter module list.

/*!
  \brief Get the packet filter of the adapter.
  \param FilterModuleContext Pointer to the filter context structure.
  \return the packet filter.

  This function is used to get the original adapter packet filter with
  a NPF_AttachAdapter(), it is stored in the HigherPacketFilter, the combination
  of HigherPacketFilter and MyPacketFilter will be the final packet filter
  the low-level adapter sees.
*/
ULONG
NPF_GetPacketFilter(
	_In_ NDIS_HANDLE FilterModuleContext
	);

/*!
  \brief Set the packet filter of the adapter.
  \param FilterModuleContext Pointer to the filter context structure.
  \param packet filter The packet filter
  \return Status of the set/query request.

This function is used to get the original adapter packet filter with
a NPF_AttachAdapter(), it is stored in the HigherPacketFilter, the combination
of HigherPacketFilter and MyPacketFilter will be the final packet filter
the low-level adapter sees.
*/
_IRQL_requires_(PASSIVE_LEVEL)
NDIS_STATUS
NPF_SetPacketFilter(
	_In_ NDIS_HANDLE FilterModuleContext,
	_In_ ULONG PacketFilter
);

/*!
  \brief Utility routine that forms and sends an NDIS_OID_REQUEST to the miniport adapter.
  \param FilterModuleContext Pointer to the filter context structure.
  \param RequestType NdisRequest[Set|Query|method]Information.
  \param Oid The object being set/queried.
  \param InformationBuffer Data for the request.
  \param InformationBufferLength Length of the above.
  \param OutputBufferLength Valid only for method request.
  \param MethodId Valid only for method request.
  \param pBytesProcessed Place to return bytes read/written.
  \return Status of the set/query request.

  Utility routine that forms and sends an NDIS_OID_REQUEST to the miniport,
  waits for it to complete, and returns status to the caller.
  NOTE: this assumes that the calling routine ensures validity
  of the filter handle until this returns.
*/
_IRQL_requires_(PASSIVE_LEVEL)
NDIS_STATUS
NPF_DoInternalRequest(
	_In_ NDIS_HANDLE					FilterModuleContext,
	_In_ NDIS_REQUEST_TYPE				RequestType,
	_In_ NDIS_OID						Oid,
	_Inout_updates_bytes_to_(InformationBufferLength, *pBytesProcessed)
	PVOID								InformationBuffer,
	_In_ ULONG							InformationBufferLength,
	_In_opt_ ULONG						OutputBufferLength,
	_In_ ULONG							MethodId,
	_Out_ PULONG						pBytesProcessed
	);

/*!
  \brief Self-sent request handler.
  \param FilterModuleContext Pointer to the filter context structure.
  \param NdisRequest Pointer to NDIS request.
  \param Status Status of request completion.

  NDIS entry point indicating completion of a pended self-sent NDIS_OID_REQUEST,
  called by NPF_OidRequestComplete.
*/
VOID
NPF_InternalRequestComplete(
	_In_ NDIS_HANDLE                  FilterModuleContext,
	_In_ PNDIS_OID_REQUEST            NdisRequest,
	_In_ NDIS_STATUS                  Status
	);

/*!
  \brief Add the open context to the group open array of a filter module.
  \param pOpen Pointer to open context structure.
  \param pFiltMod Pointer to filter module context structure.

  This function is used by NPF_OpenAdapter to add a new open context to
  the group open array of a filter module, this array is designed to help find and clean the specific adapter context.
  A filter module context is generated by NPF_AttachAdapter(), it handles with NDIS.
  A open instance is generated by NPF_OpenAdapter(), it handles with the WinPcap
  up-level packet.dll and so on.
*/
void
NPF_AddToGroupOpenArray(
	_Inout_ POPEN_INSTANCE pOpen,
	_Inout_ PNPCAP_FILTER_MODULE pFiltMod
	);

/*!
  \brief Remove the filter module context from the global filter module array.
  \param pFiltMod Pointer to filter module context structure.

  This function is used by NPF_DetachAdapter(), NPF_Cleanup() and NPF_CleanupForUnclosed()
  to remove a filter module context from the global filter module array.
*/
void
NPF_RemoveFromFilterModuleArray(
	_Inout_ PNPCAP_FILTER_MODULE pFiltMod
	);

/*!
  \brief Remove the open context from the group open array of a filter module.
  \param pOpen Pointer to open context structure.

  This function is used by NPF_Cleanup() and NPF_CleanupForUnclosed()
  to remove an open context from the group open array of a filter module.
*/
void
NPF_RemoveFromGroupOpenArray(
	_Inout_ POPEN_INSTANCE pOpen
	);

/*!
  \brief Get a pointer to filter module from the global array.
  \param pAdapterName The adapter name of the target filter module.
  \return Pointer to the filter module, or NULL if not found.

  This function is used to create a group member adapter for the group head one.
*/
_Ret_maybenull_
PNPCAP_FILTER_MODULE
NPF_GetFilterModuleByAdapterName(
	_In_ PNDIS_STRING pAdapterName
	);

/*!
  \brief Create a new Open instance
  \return Pointer to the new open instance.

*/
_Ret_maybenull_
POPEN_INSTANCE
NPF_CreateOpenObject(
	_In_ NDIS_HANDLE NdisHandle
	);

#ifdef HAVE_DOT11_SUPPORT
NTSTATUS NPF_GetDataRateMappingTable(
	_In_ PNPCAP_FILTER_MODULE pFiltMod,
	_Out_ PDOT11_DATA_RATE_MAPPING_TABLE pDataRateMappingTable
	);

NTSTATUS NPF_GetCurrentOperationMode(
	_In_ PNPCAP_FILTER_MODULE pFiltMod,
	_Out_ PDOT11_CURRENT_OPERATION_MODE pCurrentOperationMode);

ULONG NPF_GetCurrentOperationMode_Wrapper(
	_In_ PNPCAP_FILTER_MODULE pFiltMod);

NTSTATUS NPF_GetCurrentChannel(
	_In_ PNPCAP_FILTER_MODULE pFiltMod,
	_Out_ PULONG pCurrentChannel);

ULONG NPF_GetCurrentChannel_Wrapper(
	_In_ PNPCAP_FILTER_MODULE pFiltMod);

NTSTATUS NPF_GetCurrentFrequency(
	_In_ PNPCAP_FILTER_MODULE pFiltMod,
	_Out_ PULONG pCurrentFrequency);

ULONG NPF_GetCurrentFrequency_Wrapper(
	_In_ PNPCAP_FILTER_MODULE pFiltMod);
#endif
//-------------------------------------------------------------------

_Use_decl_annotations_
BOOLEAN
NPF_IsOpenInstance(
	POPEN_INSTANCE pOpen
	)
{
	if (pOpen == NULL)
	{
		return FALSE;
	}
	if (pOpen->OpenSignature != OPEN_SIGNATURE)
	{
		return FALSE;
	}
	return TRUE;
}

_Use_decl_annotations_
BOOLEAN
NPF_StartUsingBinding(
	PNPCAP_FILTER_MODULE pFiltMod, BOOLEAN AtDispatchLevel
	)
{
	if (!pFiltMod) {
		return FALSE;
	}
	// NPF_OpenAdapter() is not called on PASSIVE_LEVEL, so the assertion will fail.
	// ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	FILTER_ACQUIRE_LOCK(&pFiltMod->AdapterHandleLock, AtDispatchLevel);

	if (pFiltMod->AdapterBindingStatus != FilterRunning)
	{
		FILTER_RELEASE_LOCK(&pFiltMod->AdapterHandleLock, AtDispatchLevel);
		return FALSE;
	}

	pFiltMod->AdapterHandleUsageCounter++;

	FILTER_RELEASE_LOCK(&pFiltMod->AdapterHandleLock, AtDispatchLevel);

	return TRUE;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_StopUsingBinding(
	PNPCAP_FILTER_MODULE pFiltMod, BOOLEAN AtDispatchLevel
	)
{
	NT_ASSERT(pFiltMod != NULL);
	//
	//  There is no risk in calling this function from abobe passive level
	//  (i.e. DISPATCH, in this driver) as we acquire a spinlock and decrement a
	//  counter.
	//
	//	ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

	FILTER_ACQUIRE_LOCK(&pFiltMod->AdapterHandleLock, AtDispatchLevel);

	NT_ASSERT(pFiltMod->AdapterHandleUsageCounter > 0);

	pFiltMod->AdapterHandleUsageCounter--;

	FILTER_RELEASE_LOCK(&pFiltMod->AdapterHandleLock, AtDispatchLevel);
}

//-------------------------------------------------------------------
_IRQL_requires_(PASSIVE_LEVEL)
VOID
NPF_CloseBinding(
	_In_ PNPCAP_FILTER_MODULE pFiltMod
	)
{
	NDIS_EVENT Event;

	NT_ASSERT(pFiltMod != NULL);

	NdisInitializeEvent(&Event);
	NdisResetEvent(&Event);

	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	pFiltMod->AdapterBindingStatus = FilterDetaching;

	while (pFiltMod->AdapterHandleUsageCounter > 0)
	{
		NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);
		NdisWaitEvent(&Event, 1);
		NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	}

	//
	// now the UsageCounter is 0
	//

	pFiltMod->AdapterBindingStatus = FilterDetached;
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_ResetBufferContents(
	POPEN_INSTANCE Open,
	BOOLEAN AcquireLock
)
{
	LOCK_STATE_EX lockState;
	PLIST_ENTRY Curr;
	PNPF_CAP_DATA pCapData;

	if (AcquireLock)
		NdisAcquireRWLockWrite(Open->BufferLock, &lockState, 0);
	Open->Accepted = 0;
	Open->Dropped = 0;
	Open->Received = 0;

	// Clear packets from the buffer
	Curr = Open->PacketQueue.Flink;
	while (Curr != &Open->PacketQueue)
	{
		NT_ASSERT(Curr != NULL);
		pCapData = CONTAINING_RECORD(Curr, NPF_CAP_DATA, PacketQueueEntry);
		Curr = Curr->Flink;

		NPF_ReturnCapData(pCapData, Open->DeviceExtension);
	}
	// Remove links
	InitializeListHead(&Open->PacketQueue);
	// Reset Free counter
	Open->Free = Open->Size;
	if (AcquireLock)
		NdisReleaseRWLock(Open->BufferLock, &lockState);
}

_Use_decl_annotations_
VOID NPF_ReturnNBCopies(PNPF_NB_COPIES pNBCopy, PDEVICE_EXTENSION pDevExt)
{
	PBUFCHAIN_ELEM pDeleteMe = NULL;
	// FirstElem is not separately allocated
	PBUFCHAIN_ELEM pElem = pNBCopy->FirstElem.Next;
	ULONG refcount = InterlockedDecrement(&pNBCopy->refcount);

	if (refcount == 0)
	{
		ExFreeToLookasideListEx(&pDevExt->NBCopiesPool, pNBCopy);
		while (pElem != NULL)
		{
			pDeleteMe = pElem;
			pElem = pElem->Next;
			ExFreeToLookasideListEx(&pDevExt->BufferPool, pDeleteMe);
		}
	}
}

_Use_decl_annotations_
VOID NPF_ReturnNBLCopy(PNPF_NBL_COPY pNBLCopy, PDEVICE_EXTENSION pDevExt)
{
	PUCHAR pDot11RadiotapHeader = pNBLCopy->Dot11RadiotapHeader;
	ULONG refcount = InterlockedDecrement(&pNBLCopy->refcount);
	if (refcount == 0)
	{
		ExFreeToLookasideListEx(&pDevExt->NBLCopyPool, pNBLCopy);
		if (pDot11RadiotapHeader != NULL)
		{
			ExFreeToLookasideListEx(&pDevExt->Dot11HeaderPool, pDot11RadiotapHeader);
		}
	}
}

_Use_decl_annotations_
VOID NPF_ReturnCapData(PNPF_CAP_DATA pCapData, PDEVICE_EXTENSION pDevExt)
{
	PNPF_NB_COPIES pNBCopy = pCapData->pNBCopy;
	PNPF_NBL_COPY pNBLCopy = (pNBCopy ? pNBCopy->pNBLCopy : NULL);
	ExFreeToLookasideListEx(&pDevExt->CapturePool, pCapData);
	if (pNBLCopy)
	{
		NPF_ReturnNBLCopy(pNBLCopy, pDevExt);
	}
	if (pNBCopy)
	{
		NPF_ReturnNBCopies(pNBCopy, pDevExt);
	}
}

//-------------------------------------------------------------------

VOID
NPF_AddToAllOpensList(_In_ POPEN_INSTANCE pOpen)
{
	LOCK_STATE_EX lockState;
	PDEVICE_EXTENSION pDevExt = pOpen->DeviceExtension;

	NdisAcquireRWLockWrite(pDevExt->AllOpensLock, &lockState, 0);
	InsertTailList(&pDevExt->AllOpens, &pOpen->AllOpensEntry);
	NdisReleaseRWLock(pDevExt->AllOpensLock, &lockState);
}

VOID
NPF_RemoveFromAllOpensList(_In_ POPEN_INSTANCE pOpen)
{
	LOCK_STATE_EX lockState;
	PDEVICE_EXTENSION pDevExt = pOpen->DeviceExtension;

	NdisAcquireRWLockWrite(pDevExt->AllOpensLock, &lockState, 0);
	RemoveEntryList(&pOpen->AllOpensEntry);
	NdisReleaseRWLock(pDevExt->AllOpensLock, &lockState);
}

_Use_decl_annotations_
NTSTATUS
NPF_OpenAdapter(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	PNPCAP_FILTER_MODULE			pFiltMod = NULL;
	POPEN_INSTANCE			Open;
	PIO_STACK_LOCATION		IrpSp;
	NDIS_STATUS				Status = STATUS_SUCCESS;
	ULONG idx;
	PUNICODE_STRING FileName;
	NDIS_HANDLE NdisFilterHandle = ((PDEVICE_EXTENSION)(DeviceObject->DeviceExtension))->FilterDriverHandle;

	TRACE_ENTER();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);

	FileName = &IrpSp->FileObject->FileName;
	// Skip leading slashes
	for (idx = 0; idx < FileName->Length && FileName->Buffer[idx] == L'\\'; idx++);
	// If the filename is empty or all slashes, this is a request for the "root" device.
	// Otherwise, look for a filter module for it.
	if (idx != FileName->Length)
	{
		// Find the head adapter of the global array.
		pFiltMod = NPF_GetFilterModuleByAdapterName(&IrpSp->FileObject->FileName);

		if (pFiltMod == NULL)
		{
			// Can't find the adapter from the global open array.
			TRACE_MESSAGE1(PACKET_DEBUG_LOUD,
				"NPF_GetFilterModuleByAdapterName error, pFiltMod=NULL, AdapterName=%ws",
				IrpSp->FileObject->FileName.Buffer);

			Irp->IoStatus.Status = STATUS_NDIS_INTERFACE_NOT_FOUND;
			Irp->IoStatus.Information = FILE_DOES_NOT_EXIST;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			TRACE_EXIT();
			return STATUS_NDIS_INTERFACE_NOT_FOUND;
		}

		if (NPF_StartUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN) == FALSE)
		{
			TRACE_MESSAGE1(PACKET_DEBUG_LOUD,
				"NPF_StartUsingBinding error, AdapterName=%ws",
				IrpSp->FileObject->FileName.Buffer);

			Irp->IoStatus.Status = STATUS_NDIS_OPEN_FAILED;
			IoCompleteRequest(Irp, IO_NO_INCREMENT);
			TRACE_EXIT();
			return STATUS_NDIS_OPEN_FAILED;
		}

		NdisFilterHandle = pFiltMod->AdapterHandle;
	}

	// Create a group child adapter object from the head adapter.
	Open = NPF_CreateOpenObject(NdisFilterHandle);
	if (Open == NULL)
	{
		if (pFiltMod)
			NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
		Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		TRACE_EXIT();
		return STATUS_INSUFFICIENT_RESOURCES;
	}
	Open->UserPID = IoGetRequestorProcessId(Irp);
	Open->pFiltMod = pFiltMod;
	Open->DeviceExtension = DeviceObject->DeviceExtension;

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	TRACE_MESSAGE3(PACKET_DEBUG_LOUD,
		"Opening the device %ws, BindingContext=%p, Loopback=%u",
		IrpSp->FileObject->FileName.Buffer,
		Open,
		pFiltMod ? pFiltMod->Loopback : 0);
#else
	TRACE_MESSAGE2(PACKET_DEBUG_LOUD,
		"Opening the device %ws, BindingContext=%p, Loopback=<Not supported>",
		IrpSp->FileObject->FileName.Buffer,
		Open);
#endif

	if (!NT_SUCCESS(Status))
	{
		// Free the open instance' resources
		NPF_ReleaseOpenInstanceResources(Open);

		// Free the open instance itself
		ExFreePool(Open);
		Open = NULL;

		NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);

		Irp->IoStatus.Status = Status;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		TRACE_EXIT();
		return Status;
	}
	else
	{
		//  Save or open here
		IrpSp->FileObject->FsContext = Open;
	}

	NPF_AddToAllOpensList(Open);

	//
	// complete the open
	//
	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Open = %p\n", Open);

	if (pFiltMod)
	{
		NPF_AddToGroupOpenArray(Open, pFiltMod);
		NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
	}
	else
	{
		Open->OpenStatus = OpenDetached;
	}

	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = FILE_OPENED;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	TRACE_EXIT();
	return Status;
}

//-------------------------------------------------------------------
_IRQL_requires_(PASSIVE_LEVEL)
NTSTATUS NPF_EnableOps(_In_ PNPCAP_FILTER_MODULE pFiltMod, _In_ PDEVICE_OBJECT pDevObj)
{
	NTSTATUS Status = STATUS_PENDING;
	NDIS_EVENT Event;

	if (pFiltMod == NULL)
	{
		return STATUS_INVALID_DEVICE_REQUEST;
	}

	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	switch(pFiltMod->OpsState)
	{
		case OpsEnabled:
			// Already good to go;
			Status = STATUS_SUCCESS;
			break;
		case OpsEnabling:
		case OpsDisabling:
			NdisInitializeEvent(&Event);
			NdisResetEvent(&Event);
			// Wait for other thread to finish enabling
			while (pFiltMod->OpsState == OpsEnabling || pFiltMod->OpsState == OpsDisabling)
			{
				NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);
				NdisWaitEvent(&Event, 1);
				NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
			}
			if (pFiltMod->OpsState == OpsEnabled)
			{
				Status = STATUS_SUCCESS;
				break;
			}
			else if (pFiltMod->OpsState != OpsDisabled)
			{
				Status = STATUS_DRIVER_INTERNAL_ERROR;
				break;
			}
			// else drop through to OpsDisabled:
		case OpsDisabled:
			// Time to get to work
			pFiltMod->OpsState = OpsEnabling;
			break;
		default:
			Status = STATUS_INVALID_DEVICE_STATE;
	}
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);

	if (Status != STATUS_PENDING)
	{
		return Status;
	}

	Status = STATUS_SUCCESS;
	do
	{

#ifdef HAVE_DOT11_SUPPORT
		// DataRateMappingTable
		if (pFiltMod->Dot11)
		{
			// Fetch the device's data rate mapping table with the OID_DOT11_DATA_RATE_MAPPING_TABLE OID.
			pFiltMod->HasDataRateMappingTable = NT_SUCCESS(
					NPF_GetDataRateMappingTable(
						pFiltMod,
						&pFiltMod->DataRateMappingTable
						));
		}
#endif

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
		if (pFiltMod->Loopback)
		{
			if (g_WFPEngineHandle == INVALID_HANDLE_VALUE)
			{
				TRACE_MESSAGE(PACKET_DEBUG_LOUD, "init injection handles and register callouts");
				// Use Windows Filtering Platform (WFP) to capture loopback packets, also help WSK take care of loopback packet sending.
				Status = NPF_InitInjectionHandles();
				if (!NT_SUCCESS(Status))
				{
					break;
				}

				Status = NPF_RegisterCallouts(pDevObj);
				if (!NT_SUCCESS(Status))
				{
					if (g_WFPEngineHandle != INVALID_HANDLE_VALUE)
					{
						NPF_UnregisterCallouts();
					}
					break;
				}
			}
			else

			{
				TRACE_MESSAGE(PACKET_DEBUG_LOUD, "g_WFPEngineHandle already initialized");
			}
		}
#endif
	} while (0);

	pFiltMod->OpsState = NT_SUCCESS(Status) ? OpsEnabled : OpsDisabled;
	return Status;
}

_Use_decl_annotations_
BOOLEAN
NPF_StartUsingOpenInstance(
	POPEN_INSTANCE pOpen, OPEN_STATE MaxState, BOOLEAN AtDispatchLevel)
{
	BOOLEAN returnStatus;

	if (MaxState <= OpenAttached && !NPF_StartUsingBinding(pOpen->pFiltMod, AtDispatchLevel))
	{
		// Not attached, but need to be.
		return FALSE;
	}

	FILTER_ACQUIRE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);
	if (MaxState == OpenRunning && pOpen->OpenStatus == OpenAttached)
	{
		// NPF_EnableOps must be called at PASSIVE_LEVEL. Release the lock first.
		NT_ASSERT(!AtDispatchLevel);
		if (AtDispatchLevel) {
			// This is really bad! We should never be able to get here.
			returnStatus = FALSE;
		}
		else {
			FILTER_RELEASE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);
			returnStatus = NT_SUCCESS(NPF_EnableOps(pOpen->pFiltMod, pOpen->DeviceExtension->pDevObj));
#ifdef HAVE_DOT11_SUPPORT
			if (returnStatus && pOpen->pFiltMod->Dot11)
			{
				/* Update packet filter for raw wifi (must be PASSIVE_LEVEL) */
				NPF_SetPacketFilter(pOpen, 0);
			}
#endif
			FILTER_ACQUIRE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);

			if (returnStatus)
			{
				// Get the absolute value of the system boot time.
				// This is used for timestamp conversion.
				TIME_SYNCHRONIZE(&pOpen->start);

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
				if (pOpen->pFiltMod->Loopback)
				{
					// Keep track of how many active loopback captures there are
					NpfInterlockedIncrement(&g_NumLoopbackInstances);
				}
#endif

				pOpen->OpenStatus = OpenRunning;
				pOpen->PendingIrps[OpenRunning]++;
			}
		}
	}
	else if (pOpen->OpenStatus > MaxState)
	{
		returnStatus = FALSE;
	}
	else
	{
		returnStatus = TRUE;
		pOpen->PendingIrps[MaxState]++;
	}
	FILTER_RELEASE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);

	return returnStatus;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_StopUsingOpenInstance(
	POPEN_INSTANCE pOpen,
	OPEN_STATE MaxState,
	BOOLEAN AtDispatchLevel
	)
{
	FILTER_ACQUIRE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);
	NT_ASSERT(pOpen->PendingIrps[MaxState] > 0);
	pOpen->PendingIrps[MaxState]--;
	FILTER_RELEASE_LOCK(&pOpen->OpenInUseLock, AtDispatchLevel);

	if (MaxState <= OpenAttached)
	{
		NPF_StopUsingBinding(pOpen->pFiltMod, AtDispatchLevel);
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_CloseOpenInstance(
	POPEN_INSTANCE pOpen
	)
{
	NDIS_EVENT Event;
	OPEN_STATE state;

	NdisInitializeEvent(&Event);
	NdisResetEvent(&Event);

	NdisAcquireSpinLock(&pOpen->OpenInUseLock);
	pOpen->OpenStatus = OpenClosed;

	// Wait for all IRPs to complete for all states
	for (state = OpenRunning; state < OpenClosed; state++)
	{
		while (pOpen->PendingIrps[state] > 0)
		{
			NdisReleaseSpinLock(&pOpen->OpenInUseLock);
			NdisWaitEvent(&Event, 1);
			NdisAcquireSpinLock(&pOpen->OpenInUseLock);
		}
	}

	NdisReleaseSpinLock(&pOpen->OpenInUseLock);
}

//-------------------------------------------------------------------

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
VOID
NPF_DecrementLoopbackInstances(
		_Inout_ PNPCAP_FILTER_MODULE pFiltMod)
{
	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	if (NpfInterlockedDecrement(&g_NumLoopbackInstances) == 0)
	{
		pFiltMod->OpsState = OpsDisabling;
		NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);

		// No more loopback handles open. Release WFP resources
		NPF_UnregisterCallouts();
		NPF_FreeInjectionHandles();

		// Set OpsState so we re-enable these if necessary
		NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
		pFiltMod->OpsState = OpsDisabled;
	}
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);
}
#endif

VOID
NPF_DetachOpenInstance(
	_Inout_ POPEN_INSTANCE pOpen
	)
{
	NDIS_EVENT Event;
	OPEN_STATE old_state;
	OPEN_STATE state;

	NdisInitializeEvent(&Event);
	NdisResetEvent(&Event);

	NdisAcquireSpinLock(&pOpen->OpenInUseLock);
	old_state = pOpen->OpenStatus;
	pOpen->OpenStatus = OpenDetached;

	// Wait for IRPs that require an attached adapter
	for (state = OpenDetached - 1; state >= OpenRunning; state--)
	{
		while (pOpen->PendingIrps[state] > 0)
		{
			NdisReleaseSpinLock(&pOpen->OpenInUseLock);
			NdisWaitEvent(&Event, 1);
			NdisAcquireSpinLock(&pOpen->OpenInUseLock);
		}
	}
	NdisReleaseSpinLock(&pOpen->OpenInUseLock);

	NPF_RemoveFromGroupOpenArray(pOpen); //Remove the Open from the filter module's list

	if (old_state == OpenRunning)
	{
#ifdef HAVE_WFP_LOOPBACK_SUPPORT
		if (pOpen->pFiltMod && pOpen->pFiltMod->Loopback)
		{
			NPF_DecrementLoopbackInstances(pOpen->pFiltMod);
		}
#endif
	}

	pOpen->pFiltMod = NULL;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_ReleaseOpenInstanceResources(
	POPEN_INSTANCE pOpen
	)
{

	TRACE_ENTER();

	NT_ASSERT(pOpen != NULL);

	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Open= %p", pOpen);


#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	if (pOpen->OpenStatus == OpenRunning && pOpen->pFiltMod && pOpen->pFiltMod->Loopback)
	{
		NPF_DecrementLoopbackInstances(pOpen->pFiltMod);
	}
#endif

	//
	// Free the filter if it's present
	//
	if (pOpen->bpfprogram != NULL)
	{
		ExFreePool(pOpen->bpfprogram);
		pOpen->bpfprogram = NULL;
	}

	//
	// Dereference the read event.
	//

	if (pOpen->ReadEvent != NULL)
	{
		ObDereferenceObject(pOpen->ReadEvent);
		pOpen->ReadEvent = NULL;
	}

	//
	// free the buffer
	//
	if (pOpen->Size > 0)
	{
		NPF_ResetBufferContents(pOpen, FALSE);
	}

	NdisFreeRWLock(pOpen->BufferLock);
	NdisFreeRWLock(pOpen->MachineLock);
	NdisFreeSpinLock(&pOpen->CountersLock);
	NdisFreeSpinLock(&pOpen->WriteLock);
	NdisFreeSpinLock(&pOpen->OpenInUseLock);

#ifdef NPCAP_KDUMP
	//
	// Free the string with the name of the dump file
	//
	if (pOpen->DumpFileName.Buffer != NULL)
	{
		ExFreePool(pOpen->DumpFileName.Buffer);
		pOpen->DumpFileName.Buffer = NULL;
	}
#endif

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_ReleaseFilterModuleResources(
	PNPCAP_FILTER_MODULE pFiltMod
	)
{
	TRACE_ENTER();

	NT_ASSERT(pFiltMod != NULL);

	if (pFiltMod->PacketPool) // Release the packet buffer pool
	{
		NdisFreeNetBufferListPool(pFiltMod->PacketPool);
		pFiltMod->PacketPool = NULL;
	}

	// Release the adapter name
	if (pFiltMod->AdapterName.Buffer)
	{
		ExFreePool(pFiltMod->AdapterName.Buffer);
		pFiltMod->AdapterName.Buffer = NULL;
		pFiltMod->AdapterName.Length = 0;
		pFiltMod->AdapterName.MaximumLength = 0;
	}

	NdisFreeSpinLock(&pFiltMod->OIDLock);
	NdisFreeRWLock(pFiltMod->OpenInstancesLock);
	NdisFreeSpinLock(&pFiltMod->AdapterHandleLock);

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_GetDeviceMTU(
	PNPCAP_FILTER_MODULE pFiltMod,
	PUINT pMtu
	)
{
	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);
	NT_ASSERT(pMtu != NULL);

	UINT Mtu = 0;
	ULONG BytesProcessed = 0;
    PVOID pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(Mtu), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	NPF_DoInternalRequest(pFiltMod,
		NdisRequestQueryInformation,
		OID_GEN_MAXIMUM_TOTAL_SIZE,
		pBuffer,
		sizeof(Mtu),
		0,
		0,
		&BytesProcessed
	);

    Mtu = *(UINT *)pBuffer;
    ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(Mtu) || Mtu == 0)
	{
		TRACE_EXIT();
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pMtu = Mtu;
		TRACE_EXIT();
		return STATUS_SUCCESS;
	}
}

//-------------------------------------------------------------------
#ifdef HAVE_DOT11_SUPPORT
_Use_decl_annotations_
NTSTATUS
NPF_GetDataRateMappingTable(
	PNPCAP_FILTER_MODULE pFiltMod,
	PDOT11_DATA_RATE_MAPPING_TABLE pDataRateMappingTable
)
{
	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);
	NT_ASSERT(pDataRateMappingTable != NULL);

	ULONG BytesProcessed = 0;
    PVOID pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(DOT11_DATA_RATE_MAPPING_TABLE), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	NPF_DoInternalRequest(pFiltMod,
		NdisRequestQueryInformation,
		OID_DOT11_DATA_RATE_MAPPING_TABLE,
		pBuffer,
		sizeof(DOT11_DATA_RATE_MAPPING_TABLE),
		0,
		0,
		&BytesProcessed
	);

	if (BytesProcessed != sizeof(DOT11_DATA_RATE_MAPPING_TABLE))
	{
        ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);
		TRACE_EXIT();
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pDataRateMappingTable = *(DOT11_DATA_RATE_MAPPING_TABLE *) pBuffer;
        ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);
		TRACE_EXIT();
		return STATUS_SUCCESS;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
USHORT
NPF_LookUpDataRateMappingTable(
	PNPCAP_FILTER_MODULE pFiltMod,
	UCHAR ucDataRate
)
{
	UINT i;
	PDOT11_DATA_RATE_MAPPING_TABLE pTable = &pFiltMod->DataRateMappingTable;
	USHORT usRetDataRateValue = 0;
	TRACE_ENTER();

	if (!pFiltMod->HasDataRateMappingTable)
	{
		TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Data rate mapping table not found, Open = %p\n", pFiltMod);
		TRACE_EXIT();
		return usRetDataRateValue;
	}

	for (i = 0; i < pTable->uDataRateMappingLength; i ++)
	{
		if (pTable->DataRateMappingEntries[i].ucDataRateIndex == ucDataRate)
		{
			usRetDataRateValue = pTable->DataRateMappingEntries[i].usDataRateValue;
			break;
		}
	}

	TRACE_EXIT();
	return usRetDataRateValue;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_GetCurrentOperationMode(
	PNPCAP_FILTER_MODULE pFiltMod,
	PDOT11_CURRENT_OPERATION_MODE pCurrentOperationMode
)
{
	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);
	NT_ASSERT(pCurrentOperationMode != NULL);

	DOT11_CURRENT_OPERATION_MODE CurrentOperationMode = { 0 };
	ULONG BytesProcessed = 0;
    PVOID pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(CurrentOperationMode), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	NPF_DoInternalRequest(pFiltMod,
		NdisRequestQueryInformation,
		OID_DOT11_CURRENT_OPERATION_MODE,
		pBuffer,
		sizeof(CurrentOperationMode),
		0,
		0,
		&BytesProcessed
	);

    CurrentOperationMode = *(DOT11_CURRENT_OPERATION_MODE *) pBuffer;
    ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(CurrentOperationMode))
	{
		TRACE_EXIT();
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pCurrentOperationMode = CurrentOperationMode;
		TRACE_EXIT();
		return STATUS_SUCCESS;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
ULONG
NPF_GetCurrentOperationMode_Wrapper(
	PNPCAP_FILTER_MODULE pFiltMod
)
{
	DOT11_CURRENT_OPERATION_MODE CurrentOperationMode;
	if (NPF_GetCurrentOperationMode(pFiltMod, &CurrentOperationMode) != STATUS_SUCCESS)
	{
		return DOT11_OPERATION_MODE_UNKNOWN;
	}
	else
	{
		// Possible return values are:
		// 1: DOT11_OPERATION_MODE_EXTENSIBLE_STATION
		// 2: DOT11_OPERATION_MODE_EXTENSIBLE_AP
		// 3: DOT11_OPERATION_MODE_NETWORK_MONITOR
		return CurrentOperationMode.uCurrentOpMode;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_GetCurrentChannel(
	PNPCAP_FILTER_MODULE pFiltMod,
	PULONG pCurrentChannel
)
{
	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);
	NT_ASSERT(pCurrentChannel != NULL);

	ULONG CurrentChannel = 0;
	ULONG BytesProcessed = 0;
    PVOID pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(CurrentChannel), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	NPF_DoInternalRequest(pFiltMod,
		NdisRequestQueryInformation,
		OID_DOT11_CURRENT_CHANNEL,
		pBuffer,
		sizeof(CurrentChannel),
		0,
		0,
		&BytesProcessed
	);

    CurrentChannel = *(ULONG *)pBuffer;
    ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(CurrentChannel))
	{
		TRACE_EXIT();
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pCurrentChannel = CurrentChannel;
		TRACE_EXIT();
		return STATUS_SUCCESS;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
ULONG
NPF_GetCurrentChannel_Wrapper(
	PNPCAP_FILTER_MODULE pFiltMod
)
{
	ULONG CurrentChannel;
	if (NPF_GetCurrentChannel(pFiltMod, &CurrentChannel) != STATUS_SUCCESS)
	{
		return 0;
	}
	else
	{
		// Possible return values are: 1 - 14
		return CurrentChannel;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_GetCurrentFrequency(
	PNPCAP_FILTER_MODULE pFiltMod,
	PULONG pCurrentFrequency
)
{
	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);
	NT_ASSERT(pCurrentFrequency != NULL);

	ULONG CurrentFrequency = 0;
	ULONG BytesProcessed = 0;
    PVOID pBuffer = NULL;

    pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(CurrentFrequency), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return STATUS_INSUFFICIENT_RESOURCES;
    }

	NPF_DoInternalRequest(pFiltMod,
		NdisRequestQueryInformation,
		OID_DOT11_CURRENT_FREQUENCY,
		pBuffer,
		sizeof(CurrentFrequency),
		0,
		0,
		&BytesProcessed
	);

    CurrentFrequency = *(ULONG *)pBuffer;
    ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(CurrentFrequency))
	{
		TRACE_EXIT();
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		*pCurrentFrequency = CurrentFrequency;
		TRACE_EXIT();
		return STATUS_SUCCESS;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
ULONG
NPF_GetCurrentFrequency_Wrapper(
	PNPCAP_FILTER_MODULE pFiltMod
)
{
	ULONG CurrentFrequency;
	if (NPF_GetCurrentFrequency(pFiltMod, &CurrentFrequency) != STATUS_SUCCESS)
	{
		return 0;
	}
	else
	{
		// Possible return values are: 0 - 200
		return CurrentFrequency;
	}
}
#endif
//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_CloseAdapter(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	POPEN_INSTANCE pOpen;
	PIO_STACK_LOCATION IrpSp;
	TRACE_ENTER();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	pOpen = IrpSp->FileObject->FsContext;
	if (!NPF_IsOpenInstance(pOpen))
	{
		Irp->IoStatus.Status = STATUS_INVALID_HANDLE;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		TRACE_EXIT();
		return STATUS_INVALID_HANDLE;
	}

	//
	// Free the open instance itself
	//
	if (pOpen)
	{
		ExFreePool(pOpen);
	}

	Irp->IoStatus.Status = STATUS_SUCCESS;
	Irp->IoStatus.Information = 0;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	TRACE_EXIT();
	return STATUS_SUCCESS;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NTSTATUS
NPF_Cleanup(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp
	)
{
	POPEN_INSTANCE Open;
	NDIS_STATUS Status;
	PIO_STACK_LOCATION IrpSp;

	TRACE_ENTER();

	IrpSp = IoGetCurrentIrpStackLocation(Irp);
	Open = IrpSp->FileObject->FsContext;
	if (!NPF_IsOpenInstance(Open))
	{
		Irp->IoStatus.Status = STATUS_INVALID_HANDLE;
		Irp->IoStatus.Information = 0;
		IoCompleteRequest(Irp, IO_NO_INCREMENT);
		TRACE_EXIT();
		return STATUS_INVALID_HANDLE;
	}

	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Open = %p\n", Open);

	NT_ASSERT(Open != NULL);

	NPF_RemoveFromGroupOpenArray(Open); //Remove the Open from the filter module's list

	NPF_CloseOpenInstance(Open);

	if (Open->ReadEvent != NULL)
		KeSetEvent(Open->ReadEvent, 0, FALSE);

#ifdef NPCAP_KDUMP
	LARGE_INTEGER ThreadDelay;
	
	if (AdapterAlreadyClosing == FALSE)
	{

	
		 Unfreeze the consumer
	
		if(Open->mode & MODE_DUMP)
			NdisSetEvent(&Open->DumpEvent);
		else
			KeSetEvent(Open->ReadEvent,0,FALSE);

		//
		// If this instance is in dump mode, complete the dump and close the file
		//
		if((Open->mode & MODE_DUMP) && Open->DumpFileHandle != NULL)
		{
			NTSTATUS wres;

			ThreadDelay.QuadPart = -50000000;

			//
			// Wait the completion of the thread
			//
			wres = KeWaitForSingleObject(Open->DumpThreadObject,
				UserRequest,
				KernelMode,
				TRUE,
				&ThreadDelay);

			ObDereferenceObject(Open->DumpThreadObject);

			//
			// Flush and close the dump file
			//
			NPF_CloseDumpFile(Open);
		}
	}
#endif //NPCAP_KDUMP


	//
	// release all the resources
	//
	NPF_ReleaseOpenInstanceResources(Open);
	NPF_RemoveFromAllOpensList(Open);

	Status = STATUS_SUCCESS;

	//
	// and complete the IRP with status success
	//
	Irp->IoStatus.Information = 0;
	Irp->IoStatus.Status = Status;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);

	TRACE_EXIT();

	return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
void
NPF_AddToFilterModuleArray(
	PNPCAP_FILTER_MODULE pFiltMod
	)
{
	TRACE_ENTER();

	NdisAcquireSpinLock(&g_FilterArrayLock);
	PushEntryList(&g_arrFiltMod, &pFiltMod->FilterModulesEntry);
	NdisReleaseSpinLock(&g_FilterArrayLock);

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
void
NPF_AddToGroupOpenArray(
	POPEN_INSTANCE pOpen,
	PNPCAP_FILTER_MODULE pFiltMod
	)
{
	TRACE_ENTER();

	LOCK_STATE_EX lockState;

	// Acquire lock for writing (modify list)
	NdisAcquireRWLockWrite(pFiltMod->OpenInstancesLock, &lockState, 0);

	PushEntryList(&pFiltMod->OpenInstances, &pOpen->OpenInstancesEntry);

	NdisReleaseRWLock(pFiltMod->OpenInstancesLock, &lockState);

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
void
NPF_RemoveFromFilterModuleArray(
	PNPCAP_FILTER_MODULE pFiltMod
	)
{
	PSINGLE_LIST_ENTRY Prev = NULL;
	PSINGLE_LIST_ENTRY Curr = NULL;

	TRACE_ENTER();
	NT_ASSERT(pFiltMod != NULL);

	NdisAcquireSpinLock(&g_FilterArrayLock);

	Prev = &g_arrFiltMod;
	Curr = Prev->Next;
	while (Curr != NULL)
	{
		if (Curr == &(pFiltMod->FilterModulesEntry)) {
			Prev->Next = Curr->Next;
			break;
		}
		Prev = Curr;
		Curr = Prev->Next;
	}

	NdisReleaseSpinLock(&g_FilterArrayLock);

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
void
NPF_RemoveFromGroupOpenArray(
	POPEN_INSTANCE pOpen
	)
{
	PNPCAP_FILTER_MODULE pFiltMod;
	PSINGLE_LIST_ENTRY Prev = NULL;
	PSINGLE_LIST_ENTRY Curr = NULL;

	ULONG OldPacketFilter;
	ULONG BytesProcessed;
	PVOID pBuffer = NULL;
	BOOLEAN found = FALSE;
	LOCK_STATE_EX lockState;

	TRACE_ENTER();

	pFiltMod = pOpen->pFiltMod;
	if (!pFiltMod) {
		/* This adapter was already removed, so no filter module exists.
		 * Nothing left to do!
		 */
		return;
	}

	// Acquire lock for writing (modify list)
	NdisAcquireRWLockWrite(pFiltMod->OpenInstancesLock, &lockState, 0);

	/* Store the previous combined packet filter */
	OldPacketFilter = pFiltMod->MyPacketFilter;
	/* Reset the combined packet filter and recalculate it */
	pFiltMod->MyPacketFilter = 0;
#ifdef HAVE_DOT11_SUPPORT
	// Reset the raw wifi filter in case this was the last instance
	OldPacketFilter |= pFiltMod->Dot11PacketFilter;
	pFiltMod->Dot11PacketFilter = 0;
#endif


	Prev = &(pFiltMod->OpenInstances);
	Curr = Prev->Next;
	while (Curr != NULL)
	{
		if (Curr == &(pOpen->OpenInstancesEntry)) {
			/* This is the one to remove. Ignore its filter. */
			Prev->Next = Curr->Next;
			found = TRUE;
		}
		else {
			/* OR the filter in */
			pFiltMod->MyPacketFilter |= CONTAINING_RECORD(Curr, OPEN_INSTANCE, OpenInstancesEntry)->MyPacketFilter;
#ifdef HAVE_DOT11_SUPPORT
			if (pFiltMod->Dot11) {
				// There's still an open instance, so keep the raw wifi filter
				pFiltMod->Dot11PacketFilter = NDIS_PACKET_TYPE_802_11_RAW_DATA | NDIS_PACKET_TYPE_802_11_RAW_MGMT;
			}
#endif
		}
		/* Regardless, keep traversing. */
		Prev = Curr;
		Curr = Prev->Next;
	}

	NdisReleaseRWLock(pFiltMod->OpenInstancesLock, &lockState);

	/* If the packet filter has changed, originate an OID Request to set it to the new value */
	if ((pFiltMod->MyPacketFilter
#ifdef HAVE_DOT11_SUPPORT
			| pFiltMod->Dot11PacketFilter
#endif
		) != OldPacketFilter)
	{
        pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(ULONG), NPF_INTERNAL_OID_TAG);
        if (pBuffer == NULL)
        {
            IF_LOUD(DbgPrint("Allocate pBuffer failed, can't reset packet filter\n");)
        }
		else
		{
#ifdef HAVE_DOT11_SUPPORT
		*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter | pFiltMod->Dot11PacketFilter;
#else
		*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter;
#endif

			NPF_DoInternalRequest(pFiltMod,
				NdisRequestSetInformation,
				OID_GEN_CURRENT_PACKET_FILTER,
				pBuffer,
				sizeof(ULONG),
				0,
				0,
				&BytesProcessed);
			ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);
			if (BytesProcessed != sizeof(ULONG))
			{
				IF_LOUD(DbgPrint("NPF_RemoveFromGroupOpenArray: Failed to set resulting packet filter.\n");)
			}
		}
	}

	if (!found)
	{
		IF_LOUD(DbgPrint("NPF_RemoveFromGroupOpenArray: error, the open isn't in the group open list.\n");)
	}

	TRACE_EXIT();
}

//-------------------------------------------------------------------

/*!
  \brief Compare two NDIS strings.
  \param s1 The first string.
  \param s2 The second string.
  \return  1 if s1 > s2
		   0 if s1 = s2
		  -1 if s1 < s2

  This function is used to help decide whether two adapter names are the same.
*/
BOOLEAN
NPF_EqualAdapterName(
	_In_ PNDIS_STRING s1,
	_In_ PNDIS_STRING s2
	)
{
	int i;
	BOOLEAN bResult = TRUE;
	// TRACE_ENTER();

	if (s1->Length != s2->Length)
	{
		IF_LOUD(DbgPrint("NPF_EqualAdapterName: length not the same\n");)
		// IF_LOUD(DbgPrint("NPF_EqualAdapterName: length not the same, s1->Length = %d, s2->Length = %d\n", s1->Length, s2->Length);)
		// TRACE_EXIT();
		return FALSE;
	}

	for (i = 0; i < s2->Length / 2; i ++)
	{
		if (L'A' <= s1->Buffer[i] && s1->Buffer[i] <= L'Z')
		{
			if (s2->Buffer[i] - s1->Buffer[i] != 0 && s2->Buffer[i] - s1->Buffer[i] != L'a' - L'A')
			{
				bResult = FALSE;
				break;
			}
		}
		else if (L'a' <= s1->Buffer[i] && s1->Buffer[i] <= L'z')
		{
			if (s2->Buffer[i] - s1->Buffer[i] != 0 && s2->Buffer[i] - s1->Buffer[i] != L'A' - L'a')
			{
				bResult = FALSE;
				break;
			}
		}
		else if (s2->Buffer[i] - s1->Buffer[i] != 0)
		{
			bResult = FALSE;
			break;
		}
	}

	// Print unicode strings using %ws will cause page fault blue screen with IRQL = DISPATCH_LEVEL, so we disable the string print for now.
	// IF_LOUD(DbgPrint("NPF_EqualAdapterName: bResult = %d, s1 = %ws, s2 = %ws\n", i, bResult, s1->Buffer, s2->Buffer);)
	if (bResult)
	{
		IF_LOUD(DbgPrint("NPF_EqualAdapterName: bResult == TRUE\n");)
	}
	// TRACE_EXIT();
	return bResult;
}

//-------------------------------------------------------------------
/* Length of a WCHAR string literal minus the terminating null */
#define CONST_WCHAR_BYTES(_A) (sizeof(_A) - sizeof(WCHAR))
/* Ensure string "a" is long enough to contain "b" after the offset.
 * Length does not include the null terminator, so account for that with sizeof(WCHAR).
 * Then compare memory. Length is length in bytes, but buffer is a PWCHAR.
 */
#define PUNICODE_CONTAINS(a, b, byteoffset) ((a->Length >= byteoffset + CONST_WCHAR_BYTES(b)) && CONST_WCHAR_BYTES(b) == RtlCompareMemory(a->Buffer + byteoffset/sizeof(WCHAR), b, CONST_WCHAR_BYTES(b)))
_Use_decl_annotations_
PNPCAP_FILTER_MODULE
NPF_GetFilterModuleByAdapterName(
	PNDIS_STRING pAdapterName
	)
{
	PSINGLE_LIST_ENTRY Curr = NULL;
	PNPCAP_FILTER_MODULE pFiltMod = NULL;
	size_t i = 0;
	USHORT cchShrink = 0;
#define BYTES(_cch) ((_cch) * sizeof(WCHAR))
#define CCH(_bytes) ((_bytes) / sizeof(WCHAR))
	BOOLEAN Dot11 = FALSE;
	BOOLEAN Loopback = FALSE;
	NDIS_STRING BaseName = NDIS_STRING_CONST("Loopback");
	WCHAR *pName = NULL;
	TRACE_ENTER();

	if (pAdapterName->Buffer == NULL || pAdapterName->Length == 0) {
		return NULL;
	}

	// strip off leading backslashes
	while (BYTES(cchShrink) < pAdapterName->Length && pAdapterName->Buffer[cchShrink] == L'\\') {
		cchShrink++;
	}

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	// If this is *not* the legacy loopback name, we'll have to set up BaseName to be the real name of the buffer.
	if (g_LoopbackAdapterName.Buffer != NULL // Legacy loopback name exists
		&& (g_LoopbackAdapterName.Length - devicePrefix.Length) == (pAdapterName->Length - BYTES(cchShrink)) // Length matches
		&& RtlCompareMemory(g_LoopbackAdapterName.Buffer + CCH(devicePrefix.Length), pAdapterName->Buffer + cchShrink,
					pAdapterName->Length - BYTES(cchShrink)) == pAdapterName->Length - BYTES(cchShrink)
		)
	{
		Loopback = TRUE;
	}

	if (!Loopback) {
#endif

	BaseName.MaximumLength = pAdapterName->MaximumLength;
	BaseName.Buffer = ExAllocatePoolWithTag(NonPagedPool, BaseName.MaximumLength, NPF_UNICODE_BUFFER_TAG);
	if (BaseName.Buffer == NULL) {
		IF_LOUD(DbgPrint("NPF_GetFilterModuleByAdapterName: failed to allocate BaseName.Buffer\n");)
		TRACE_EXIT();
		return NULL;
	}

	// Check for WIFI_ prefix and strip it
	if (PUNICODE_CONTAINS(pAdapterName, NPF_DEVICE_NAMES_TAG_WIDECHAR_WIFI, BYTES(cchShrink))) {
		cchShrink += CCH(CONST_WCHAR_BYTES(NPF_DEVICE_NAMES_TAG_WIDECHAR_WIFI));
		Dot11 = TRUE;
	}

	// Do the strip
	for (i=cchShrink; i < CCH(pAdapterName->Length) && (i - cchShrink) < CCH(BaseName.MaximumLength); i++) {
		BaseName.Buffer[i - cchShrink] = pAdapterName->Buffer[i];
	}
	BaseName.Length = pAdapterName->Length - BYTES(cchShrink);

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	} //end if !Loopback
#endif

	NdisAcquireSpinLock(&g_FilterArrayLock);
	for (Curr = g_arrFiltMod.Next; Curr != NULL; Curr = Curr->Next)
	{
		pFiltMod = CONTAINING_RECORD(Curr, NPCAP_FILTER_MODULE, FilterModulesEntry);
		if (NPF_StartUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN) == FALSE)
		{
			continue;
		}

		if (pFiltMod->Dot11 == Dot11 && NPF_EqualAdapterName(&pFiltMod->AdapterName, &BaseName))
		{
			NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
			NdisReleaseSpinLock(&g_FilterArrayLock);
			if (!Loopback) {
				ExFreePoolWithTag(BaseName.Buffer, NPF_UNICODE_BUFFER_TAG);
			}
			return pFiltMod;
		}
		else
		{
			NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
		}
	}
	NdisReleaseSpinLock(&g_FilterArrayLock);
	if (!Loopback) {
		ExFreePoolWithTag(BaseName.Buffer, NPF_UNICODE_BUFFER_TAG);
	}

	TRACE_EXIT();
	return NULL;
}

//-------------------------------------------------------------------

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
_Use_decl_annotations_
PNPCAP_FILTER_MODULE
NPF_GetLoopbackFilterModule()
{
	PSINGLE_LIST_ENTRY Curr = NULL;
	PNPCAP_FILTER_MODULE pFiltMod = NULL;
	TRACE_ENTER();

	NdisAcquireSpinLock(&g_FilterArrayLock);
	for (Curr = g_arrFiltMod.Next; Curr != NULL; Curr = Curr->Next)
	{
		pFiltMod = CONTAINING_RECORD(Curr, NPCAP_FILTER_MODULE, FilterModulesEntry);
		if (NPF_StartUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN) == FALSE)
		{
			continue;
		}

		if (pFiltMod->Loopback)
		{
			NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
			NdisReleaseSpinLock(&g_FilterArrayLock);
			return pFiltMod;
		}
		else
		{
			NPF_StopUsingBinding(pFiltMod, NPF_IRQL_UNKNOWN);
		}
	}
	NdisReleaseSpinLock(&g_FilterArrayLock);

	TRACE_EXIT();
	return NULL;
}
#endif
//-------------------------------------------------------------------

_Use_decl_annotations_
POPEN_INSTANCE
NPF_CreateOpenObject(NDIS_HANDLE NdisHandle)
{
	POPEN_INSTANCE Open;
	TRACE_ENTER();

	// allocate some memory for the open structure
	Open = ExAllocatePoolWithTag(NonPagedPool, sizeof(OPEN_INSTANCE), NPF_OPEN_TAG);

	if (Open == NULL)
	{
		// no memory
		TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate memory pool");
		TRACE_EXIT();
		return NULL;
	}

	RtlZeroMemory(Open, sizeof(OPEN_INSTANCE));

	/* Buffer */
	Open->BufferLock = NdisAllocateRWLock(NdisHandle);
	if (Open->BufferLock == NULL)
	{
		TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate BufferLock");
		ExFreePool(Open);
		TRACE_EXIT();
		return NULL;
	}

	InitializeListHead(&Open->PacketQueue);
	KeInitializeSpinLock(&Open->PacketQueueLock);
	Open->Accepted = 0;
	Open->Received = 0;
	Open->Dropped = 0;

	Open->OpenSignature = OPEN_SIGNATURE;
	Open->OpenStatus = OpenClosed;

	NdisInitializeEvent(&Open->WriteEvent);
	NdisInitializeEvent(&Open->NdisWriteCompleteEvent);
#ifdef NPCAP_KDUMP
	NdisInitializeEvent(&Open->DumpEvent);
#endif
	Open->MachineLock = NdisAllocateRWLock(NdisHandle);
	if (Open->MachineLock == NULL)
	{
		TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate MachineLock");
		NdisFreeRWLock(Open->BufferLock);
		ExFreePool(Open);
		TRACE_EXIT();
		return NULL;
	}
	NdisAllocateSpinLock(&Open->WriteLock);
	Open->WriteInProgress = FALSE;

	//
	// Initialize the open instance
	//
	//Open->BindContext = NULL;
	Open->TimestampMode = g_TimestampMode;
	Open->bpfprogram = NULL;	//reset the filter
	Open->mode = MODE_CAPT;
	Open->Nbytes.QuadPart = 0;
	Open->Npackets.QuadPart = 0;
	Open->Nwrites = 1;
	Open->Multiple_Write_Counter = 0;
	Open->MinToCopy = 0;
	Open->TimeOut.QuadPart = (LONGLONG)1;
#ifdef NPCAP_KDUMP
	Open->DumpFileName.Buffer = NULL;
	Open->DumpFileHandle = NULL;
	Open->DumpLimitReached = FALSE;
#endif
	Open->Size = 0;
	Open->SkipSentPackets = FALSE;
	Open->ReadEvent = NULL;

	//
	// we need to keep a counter of the pending IRPs
	// so that when the IRP_MJ_CLEANUP dispatcher gets called,
	// we can wait for those IRPs to be completed
	for (OPEN_STATE state = 0; state < OpenClosed; state++)
	{
		Open->PendingIrps[state] = 0;
	}
	NdisAllocateSpinLock(&Open->OpenInUseLock);
	Open->OpenInstancesEntry.Next = NULL;

	//
	//allocate the spinlock for the statistic counters
	//
	NdisAllocateSpinLock(&Open->CountersLock);

	Open->OpenStatus = OpenAttached;

	TRACE_EXIT();
	return Open;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
PNPCAP_FILTER_MODULE
NPF_CreateFilterModule(
	NDIS_HANDLE NdisFilterHandle,
	PNDIS_STRING AdapterName,
	UINT SelectedIndex)
{
	PNPCAP_FILTER_MODULE pFiltMod;
	NET_BUFFER_LIST_POOL_PARAMETERS PoolParameters;
	BOOLEAN bAllocFailed = FALSE;

	// allocate some memory for the filter module structure
	pFiltMod = ExAllocatePoolWithTag(NonPagedPool, sizeof(NPCAP_FILTER_MODULE), NPF_FILTMOD_TAG);

	if (pFiltMod == NULL)
	{
		// no memory
		TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate memory pool");
		TRACE_EXIT();
		return NULL;
	}

	RtlZeroMemory(pFiltMod, sizeof(NPCAP_FILTER_MODULE));

	pFiltMod->AdapterHandle = NdisFilterHandle;
	pFiltMod->AdapterBindingStatus = FilterAttaching;
#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	pFiltMod->Loopback = FALSE;
#endif

#ifdef HAVE_RX_SUPPORT
	pFiltMod->SendToRxPath = FALSE;
	pFiltMod->BlockRxPath = FALSE;
#endif

#ifdef HAVE_DOT11_SUPPORT
	pFiltMod->Dot11 = FALSE;
	pFiltMod->Dot11PacketFilter = 0x0;
	pFiltMod->HasDataRateMappingTable = FALSE;
#endif

	pFiltMod->FilterModulesEntry.Next = NULL;
	pFiltMod->OpenInstances.Next = NULL;

	// Pool sizes based on observations on a single-core Hyper-V VM while
	// running our test suite.

	//  Initialize the pools
	do {
		pFiltMod->OpenInstancesLock = NdisAllocateRWLock(NdisFilterHandle);
		if (pFiltMod->OpenInstancesLock == NULL)
		{
			TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate OpenInstancesLock");
			bAllocFailed = TRUE;
			break;
		}

		NdisZeroMemory(&PoolParameters, sizeof(NET_BUFFER_LIST_POOL_PARAMETERS));
		PoolParameters.Header.Type = NDIS_OBJECT_TYPE_DEFAULT;
		PoolParameters.Header.Revision = NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
		PoolParameters.Header.Size = NDIS_SIZEOF_NET_BUFFER_LIST_POOL_PARAMETERS_REVISION_1;
		PoolParameters.ProtocolId = NDIS_PROTOCOL_ID_DEFAULT;
		PoolParameters.fAllocateNetBuffer = TRUE;
		PoolParameters.ContextSize = sizeof(PACKET_RESERVED);
		PoolParameters.PoolTag = NPF_PACKET_POOL_TAG;
		PoolParameters.DataSize = 0;

		pFiltMod->PacketPool = NdisAllocateNetBufferListPool(NdisFilterHandle, &PoolParameters);
		if (pFiltMod->PacketPool == NULL)
		{
			TRACE_MESSAGE(PACKET_DEBUG_LOUD, "Failed to allocate packet pool");
			bAllocFailed = TRUE;
			break;
		}
	} while (0);

	if (bAllocFailed) {
		if (pFiltMod->PacketPool)
			NdisFreeNetBufferListPool(pFiltMod->PacketPool);
		if (pFiltMod->OpenInstancesLock)
			NdisFreeRWLock(pFiltMod->OpenInstancesLock);
		ExFreePool(pFiltMod);
		TRACE_EXIT();
		return NULL;
	}
	
	// Default; expect this will be overwritten in NPF_Restart,
	// or for Loopback when creating the fake module.
	pFiltMod->MaxFrameSize = 1514;

	pFiltMod->AdapterName.MaximumLength = AdapterName->MaximumLength - devicePrefix.Length;
	pFiltMod->AdapterName.Buffer = ExAllocatePoolWithTag(NonPagedPool, pFiltMod->AdapterName.MaximumLength, NPF_UNICODE_BUFFER_TAG);
	pFiltMod->AdapterName.Length = 0;
	RtlAppendUnicodeToString(&pFiltMod->AdapterName, AdapterName->Buffer + devicePrefix.Length / sizeof(WCHAR));

	//
	//allocate the spinlock for the OID requests
	//
	NdisAllocateSpinLock(&pFiltMod->OIDLock);

	//
	// set the proper binding flags before trying to open the MAC
	//
	pFiltMod->AdapterBindingStatus = FilterRunning;
	pFiltMod->AdapterHandleUsageCounter = 0;
	NdisAllocateSpinLock(&pFiltMod->AdapterHandleLock);

	pFiltMod->OpsState = OpsDisabled;

	TRACE_EXIT();
	return pFiltMod;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_RegisterOptions(
	NDIS_HANDLE  NdisFilterHandle,
	NDIS_HANDLE  FilterDriverContext
	)
/*++

Routine Description:

	Register optional handlers with NDIS.  This sample does not happen to
	have any optional handlers to register, so this routine does nothing
	and could simply have been omitted.  However, for illustrative purposes,
	it is presented here.

Arguments:

	NdisFilterHandle - pointer the driver handle received from
							 NdisFRegisterFilterDriver

	FilterDriverContext    - pointer to our context passed into
							 NdisFRegisterFilterDriver

Return Value:

	NDIS_STATUS_SUCCESS

--*/
{
	TRACE_ENTER();

	if (!NT_VERIFY(FilterDriverContext == (NDIS_HANDLE)FilterDriverObject))
	{
		IF_LOUD(DbgPrint("NPF_RegisterOptions: driver doesn't match error, FilterDriverContext = %p, FilterDriverObject = %p.\n", FilterDriverContext, FilterDriverObject);)
		return NDIS_STATUS_INVALID_PARAMETER;
	}

	TRACE_EXIT();

	return NDIS_STATUS_SUCCESS;
}

//-------------------------------------------------------------------

static NDIS_STATUS NPF_ValidateParameters(
	_In_ BOOLEAN bDot11,
	_In_ NDIS_MEDIUM MiniportMediaType
        )
{
    // Verify the media type is supported.  This is a last resort; the
    // the filter should never have been bound to an unsupported miniport
    // to begin with.  If this driver is marked as a Mandatory filter (which
    // is the default for this sample; see the INF file), failing to attach
    // here will leave the network adapter in an unusable state.
    //
    // Your setup/install code should not bind the filter to unsupported
    // media types.
    if ((MiniportMediaType != NdisMedium802_3)
            && (MiniportMediaType != NdisMediumNative802_11)
            && (MiniportMediaType != NdisMediumWan) //we don't care this kind of miniports
            && (MiniportMediaType != NdisMediumWirelessWan) //we don't care this kind of miniports
            && (MiniportMediaType != NdisMediumFddi)
            && (MiniportMediaType != NdisMediumArcnet878_2)
            && (MiniportMediaType != NdisMediumAtm)
            && (MiniportMediaType != NdisMedium802_5))
    {
		IF_LOUD(DbgPrint("Unsupported media type: MiniportMediaType = %d.\n", MiniportMediaType);)

		return NDIS_STATUS_INVALID_PARAMETER;
    }

	// The WiFi filter will only bind to the 802.11 wireless adapters.
	if (g_Dot11SupportMode && bDot11)
	{
		if (MiniportMediaType != NdisMediumNative802_11)
		{
			IF_LOUD(DbgPrint("Unsupported media type for the WiFi filter: MiniportMediaType = %d, expected = 16 (NdisMediumNative802_11).\n", MiniportMediaType);)

			return NDIS_STATUS_INVALID_PARAMETER;
		}
	}
	return NDIS_STATUS_SUCCESS;
}

_Use_decl_annotations_
NDIS_STATUS
NPF_AttachAdapter(
	NDIS_HANDLE                     NdisFilterHandle,
	NDIS_HANDLE                     FilterDriverContext,
	PNDIS_FILTER_ATTACH_PARAMETERS  AttachParameters
	)
{
	PNPCAP_FILTER_MODULE pFiltMod = NULL;
	NDIS_STATUS             Status;
	NDIS_STATUS				returnStatus;
	NDIS_FILTER_ATTRIBUTES	FilterAttributes;
	BOOLEAN					bFalse = FALSE;
	BOOLEAN					bDot11;

	TRACE_ENTER();

	do
	{
		if (!NT_VERIFY(FilterDriverContext == (NDIS_HANDLE)FilterDriverObject))
		{
			returnStatus = NDIS_STATUS_INVALID_PARAMETER;
			break;
		}

		// An example:
		// AdapterName = "\DEVICE\{4F4B4BD7-340D-45D3-8F59-8A1E167BC75D}"
		// FilterModuleGuidName = "{4F4B4BD7-340D-45D3-8F59-8A1E167BC75D}-{7DAF2AC8-E9F6-4765-A842-F1F5D2501351}-0000"
		IF_LOUD(DbgPrint("NPF_AttachAdapter: AdapterName=%ws, MacAddress=%02X-%02X-%02X-%02X-%02X-%02X, MiniportMediaType=%d\n",
			AttachParameters->BaseMiniportName->Buffer,
			AttachParameters->CurrentMacAddress[0],
			AttachParameters->CurrentMacAddress[1],
			AttachParameters->CurrentMacAddress[2],
			AttachParameters->CurrentMacAddress[3],
			AttachParameters->CurrentMacAddress[4],
			AttachParameters->CurrentMacAddress[5],
			AttachParameters->MiniportMediaType);
		);

		IF_LOUD(DbgPrint("NPF_AttachAdapter: FilterModuleGuidName=%ws, FilterModuleGuidName[%I64u]=%d\n",
			AttachParameters->FilterModuleGuidName->Buffer,
			SECOND_LAST_HEX_INDEX_OF_FILTER_UNIQUE_NAME,
			AttachParameters->FilterModuleGuidName->Buffer[SECOND_LAST_HEX_INDEX_OF_FILTER_UNIQUE_NAME]);
		);

		if (AttachParameters->FilterModuleGuidName->Buffer[SECOND_LAST_HEX_INDEX_OF_FILTER_UNIQUE_NAME] == L'4')
		{
			IF_LOUD(DbgPrint("NPF_AttachAdapter: This is the standard filter binding!\n");)
			bDot11 = FALSE;
		}
		else if (AttachParameters->FilterModuleGuidName->Buffer[SECOND_LAST_HEX_INDEX_OF_FILTER_UNIQUE_NAME] == L'5')
		{
			IF_LOUD(DbgPrint("NPF_AttachAdapter: This is the WiFi filter binding!\n");)
			bDot11 = TRUE;
		}
		else
		{
			IF_LOUD(DbgPrint("NPF_AttachAdapter: error, unrecognized filter binding!\n");)

			returnStatus = NDIS_STATUS_INVALID_PARAMETER;
			break;
		}

		returnStatus = NPF_ValidateParameters(bDot11, AttachParameters->MiniportMediaType);
		if (returnStatus != STATUS_SUCCESS)
			break;

		// Disable this code for now, because it invalidates most adapters to be bound, reason needs to be clarified.
// 		if (AttachParameters->LowerIfIndex != AttachParameters->BaseMiniportIfIndex)
// 		{
// 			IF_LOUD(DbgPrint("Don't bind to other altitudes than exactly over the miniport: LowerIfIndex = %d, BaseMiniportIfIndex = %d.\n", AttachParameters->LowerIfIndex, AttachParameters->BaseMiniportIfIndex);)
// 
// 			returnStatus = NDIS_STATUS_NOT_SUPPORTED;
// 			break;
// 		}

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
		// Determine whether this is the legacy loopback adapter
		if (g_LoopbackAdapterName.Buffer != NULL)
		{
			if (RtlCompareMemory(g_LoopbackAdapterName.Buffer + devicePrefix.Length / 2, AttachParameters->BaseMiniportName->Buffer + devicePrefix.Length / 2,
				AttachParameters->BaseMiniportName->Length - devicePrefix.Length) == AttachParameters->BaseMiniportName->Length - devicePrefix.Length)
			{
				// This request is for the legacy loopback adapter listed in the Registry.
				// Since we now have a fake filter module for this, deny the binding.
				// We'll intercept open requests for this name elsewhere and redirect to the fake one.
				returnStatus = NDIS_STATUS_NOT_SUPPORTED;
				break;
			}
		}
#endif

		// create the adapter object
		pFiltMod = NPF_CreateFilterModule(NdisFilterHandle, AttachParameters->BaseMiniportName, AttachParameters->MiniportMediaType);
		if (pFiltMod == NULL)
		{
			returnStatus = NDIS_STATUS_RESOURCES;
			TRACE_EXIT();
			return returnStatus;
		}
		pFiltMod->AdapterBindingStatus = FilterAttaching;

#ifdef HAVE_RX_SUPPORT
		// Determine whether this is our send-to-Rx adapter for the open_instance.
		if (g_SendToRxAdapterName.Buffer != NULL)
		{
			USHORT iAdapterCnt = (g_SendToRxAdapterName.Length / 2 + 1) / ADAPTER_NAME_SIZE_WITH_SEPARATOR;
			TRACE_MESSAGE2(PACKET_DEBUG_LOUD,
				"g_SendToRxAdapterName.Length=%u, iAdapterCnt=%u",
				g_SendToRxAdapterName.Length,
				iAdapterCnt);
			for (int i = 0; i < iAdapterCnt; i++)
			{
				if (RtlCompareMemory(g_SendToRxAdapterName.Buffer + devicePrefix.Length / 2 + ADAPTER_NAME_SIZE_WITH_SEPARATOR * i,
					AttachParameters->BaseMiniportName->Buffer + devicePrefix.Length / 2,
					AttachParameters->BaseMiniportName->Length - devicePrefix.Length)
					== AttachParameters->BaseMiniportName->Length - devicePrefix.Length)
				{
					pFiltMod->SendToRxPath = TRUE;
					break;
				}
			}
		}
		// Determine whether this is our block-Rx adapter for the open_instance.
		if (g_BlockRxAdapterName.Buffer != NULL)
		{
			USHORT iAdapterCnt = (g_BlockRxAdapterName.Length / 2 + 1) / ADAPTER_NAME_SIZE_WITH_SEPARATOR;
			TRACE_MESSAGE2(PACKET_DEBUG_LOUD,
				"g_BlockRxAdapterName.Length=%u, iAdapterCnt=%u",
				g_BlockRxAdapterName.Length,
				iAdapterCnt);
			for (int i = 0; i < iAdapterCnt; i++)
			{
				if (RtlCompareMemory(g_BlockRxAdapterName.Buffer + devicePrefix.Length / 2 + ADAPTER_NAME_SIZE_WITH_SEPARATOR * i,
					AttachParameters->BaseMiniportName->Buffer + devicePrefix.Length / 2,
					AttachParameters->BaseMiniportName->Length - devicePrefix.Length)
					== AttachParameters->BaseMiniportName->Length - devicePrefix.Length)
				{
					pFiltMod->BlockRxPath = TRUE;
					break;
				}
			}
		}
#endif

		NdisZeroMemory(&FilterAttributes, sizeof(NDIS_FILTER_ATTRIBUTES));
		FilterAttributes.Header.Revision = NDIS_FILTER_ATTRIBUTES_REVISION_1;
		FilterAttributes.Header.Size = sizeof(NDIS_FILTER_ATTRIBUTES);
		FilterAttributes.Header.Type = NDIS_OBJECT_TYPE_FILTER_ATTRIBUTES;
		FilterAttributes.Flags = 0;

		NDIS_DECLARE_FILTER_MODULE_CONTEXT(NPCAP_FILTER_MODULE);
		Status = NdisFSetAttributes(NdisFilterHandle,
			pFiltMod,
			&FilterAttributes);

		if (Status != NDIS_STATUS_SUCCESS)
		{
			returnStatus = Status;
			IF_LOUD(DbgPrint("NdisFSetAttributes: error, Status=%x.\n", Status);)
			break;
		}

		pFiltMod->HigherPacketFilter = NPF_GetPacketFilter(pFiltMod);
		TRACE_MESSAGE1(PACKET_DEBUG_LOUD,
			"HigherPacketFilter=%x",
			pFiltMod->HigherPacketFilter);

		pFiltMod->PhysicalMedium = AttachParameters->MiniportPhysicalMediaType;
		TRACE_MESSAGE1(PACKET_DEBUG_LOUD,
			"PhysicalMedium=%x",
			pFiltMod->PhysicalMedium);

#ifdef HAVE_DOT11_SUPPORT
		pFiltMod->Dot11 = g_Dot11SupportMode && bDot11;
#endif

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
		TRACE_MESSAGE4(PACKET_DEBUG_LOUD,
			"Opened the device %ws, BindingContext=%p, Loopback=%d, dot11=%d",
			AttachParameters->BaseMiniportName->Buffer,
			pFiltMod,
			pFiltMod->Loopback,
			pFiltMod->Dot11);
#else
		TRACE_MESSAGE3(PACKET_DEBUG_LOUD,
			"Opened the device %ws, BindingContext=%p, Loopback=<Not supported>, dot11=%d",
			AttachParameters->BaseMiniportName->Buffer,
			pFiltMod,
			pFiltMod->Dot11);
#endif

		returnStatus = STATUS_SUCCESS;
		pFiltMod->AdapterBindingStatus = FilterPaused;
		NPF_AddToFilterModuleArray(pFiltMod);
	}
	while (bFalse);

	if (!NT_SUCCESS(returnStatus) && pFiltMod != NULL) {
		NPF_ReleaseFilterModuleResources(pFiltMod);
		//
		// Free the object itself
		//
		ExFreePool(pFiltMod);
		pFiltMod = NULL;
	}
	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "returnStatus=%x", returnStatus);
	TRACE_EXIT();
	return returnStatus;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_Pause(
	NDIS_HANDLE                     FilterModuleContext,
	PNDIS_FILTER_PAUSE_PARAMETERS   PauseParameters
	)
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE)FilterModuleContext;
	NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
	NDIS_EVENT Event;
	BOOLEAN PendingWrites = FALSE;

	UNREFERENCED_PARAMETER(PauseParameters);
	TRACE_ENTER();

	NdisInitializeEvent(&Event);
	NdisResetEvent(&Event);

	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	NT_ASSERT(pFiltMod->AdapterBindingStatus == FilterRunning);
	pFiltMod->AdapterBindingStatus = FilterPausing;
	
	while (pFiltMod->AdapterHandleUsageCounter > 0)
	{
		NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);
		NdisWaitEvent(&Event, 1);
		NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	}

	pFiltMod->AdapterBindingStatus = FilterPaused;
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);

	Status = NDIS_STATUS_SUCCESS;
	
	TRACE_EXIT();
	return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_Restart(
	NDIS_HANDLE                     FilterModuleContext,
	PNDIS_FILTER_RESTART_PARAMETERS RestartParameters
	)
{

	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE)FilterModuleContext;
	NDIS_STATUS		Status;
	NTSTATUS ntStatus;
	UINT Mtu;
	PNDIS_RESTART_ATTRIBUTES Curr = RestartParameters->RestartAttributes;
	PNDIS_RESTART_GENERAL_ATTRIBUTES GenAttr = NULL;

	TRACE_ENTER();

	if (RestartParameters == NULL)
	{
		// Can't validate, but probably fine. Also, I don't think this is possible.
		return NDIS_STATUS_SUCCESS;
	}

	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	NT_ASSERT(pFiltMod->AdapterBindingStatus == FilterPaused);
	pFiltMod->AdapterBindingStatus = FilterRestarting;
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);

	Status = NPF_ValidateParameters(pFiltMod->Dot11, RestartParameters->MiniportMediaType);
	if (Status != NDIS_STATUS_SUCCESS) {
		goto NPF_Restart_End;
	}

	// MtuSize is actually OID_GEN_MAXIMUM_FRAME_SIZE and does not include link header
	// We'll grab it because it's available, but we'll try to get something better
	while (Curr) {
		if (Curr->Oid == OID_GEN_MINIPORT_RESTART_ATTRIBUTES) {
			GenAttr = (PNDIS_RESTART_GENERAL_ATTRIBUTES) Curr->Data;
			pFiltMod->MaxFrameSize = GenAttr->MtuSize;
			break;
		}
		Curr = Curr->Next;
	}
	// Now try OID_GEN_MAXIMUM_TOTAL_SIZE, including link header
	// If it fails, no big deal; we have the MTU at least.
	ntStatus = NPF_GetDeviceMTU(pFiltMod, &Mtu);
	if (NT_SUCCESS(ntStatus))
	{
		pFiltMod->MaxFrameSize = Mtu;
	}


NPF_Restart_End:
	NdisAcquireSpinLock(&pFiltMod->AdapterHandleLock);
	pFiltMod->AdapterBindingStatus = NDIS_STATUS_SUCCESS == Status ? FilterRunning : FilterPaused;
	NdisReleaseSpinLock(&pFiltMod->AdapterHandleLock);

	TRACE_EXIT();
	return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_DetachAdapter(
	NDIS_HANDLE     FilterModuleContext
	)
/*++

Routine Description:

	Filter detach routine.
	This is a required function that will deallocate all the resources allocated during
	FilterAttach. NDIS calls FilterDetach to remove a filter instance from a filter stack.

Arguments:

	FilterModuleContext - pointer to the filter context area.

Return Value:
	None.

NOTE: Called at PASSIVE_LEVEL and the filter is in paused state

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE)FilterModuleContext;
	PSINGLE_LIST_ENTRY Curr;
	POPEN_INSTANCE pOpen;

	TRACE_ENTER();

	NT_ASSERT(pFiltMod->AdapterBindingStatus == FilterPaused || pFiltMod->Loopback);
	/* No need to lock the group since we are paused. */
	for (Curr = pFiltMod->OpenInstances.Next; Curr != NULL; Curr = Curr->Next)
	{
		pOpen = CONTAINING_RECORD(Curr, OPEN_INSTANCE, OpenInstancesEntry);
		NPF_DetachOpenInstance(pOpen);

		if (pOpen->ReadEvent != NULL)
			KeSetEvent(pOpen->ReadEvent, 0, FALSE);
	}

	NPF_CloseBinding(pFiltMod);

	NPF_RemoveFromFilterModuleArray(pFiltMod); // Must add this, if not, SYSTEM_SERVICE_EXCEPTION BSoD will occur.
	NPF_ReleaseFilterModuleResources(pFiltMod);
	ExFreePool(pFiltMod);

	TRACE_EXIT();
	return;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_OidRequest(
	NDIS_HANDLE         FilterModuleContext,
	PNDIS_OID_REQUEST   Request
	)
/*++

Routine Description:

	Request handler
	Handle requests from upper layers

Arguments:

	FilterModuleContext   - our filter
	Request               - the request passed down


Return Value:

	 NDIS_STATUS_SUCCESS
	 NDIS_STATUS_PENDING
	 NDIS_STATUS_XXX

NOTE: Called at <= DISPATCH_LEVEL  (unlike a miniport's MiniportOidRequest)

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	NDIS_STATUS             Status;
	PNDIS_OID_REQUEST       ClonedRequest=NULL;
	BOOLEAN                 bSubmitted = FALSE;
	PFILTER_REQUEST_CONTEXT Context;
	BOOLEAN                 bFalse = FALSE;
    PVOID pBuffer = NULL;

	TRACE_ENTER();

	// Special case: if their new packet filter doesn't change the lower one
	// then we don't pass it down but just return success.
	if (Request->RequestType == NdisRequestSetInformation
			&& Request->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER
			&& (*(PULONG) Request->DATA.SET_INFORMATION.InformationBuffer | pFiltMod->MyPacketFilter) == (pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter))
	{
		pFiltMod->HigherPacketFilter = *(PULONG) Request->DATA.SET_INFORMATION.InformationBuffer;
		Request->DATA.SET_INFORMATION.BytesRead = sizeof(ULONG);
		return NDIS_STATUS_SUCCESS;
	}
	do
	{
		Status = NdisAllocateCloneOidRequest(pFiltMod->AdapterHandle,
											Request,
											NPF_CLONE_OID_TAG,
											&ClonedRequest);
		if (Status != NDIS_STATUS_SUCCESS)
		{
			TRACE_MESSAGE(PACKET_DEBUG_LOUD, "FilterOidRequest: Cannot Clone Request\n");
			break;
		}

		if (Request->RequestType == NdisRequestSetInformation && Request->DATA.SET_INFORMATION.Oid == OID_GEN_CURRENT_PACKET_FILTER)
		{
			// ExAllocatePoolWithTag is permitted to be used at DISPATCH_LEVEL iff allocating from NonPagedPool
#pragma warning(suppress: 28118)
			pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(ULONG), NPF_CLONE_OID_TAG);
			if (pBuffer == NULL)
			{
				IF_LOUD(DbgPrint("Allocate pBuffer failed, cannot modify packet filter.\n");)
			}
			else
			{


				pFiltMod->HigherPacketFilter = *(ULONG *) Request->DATA.SET_INFORMATION.InformationBuffer;
#ifdef HAVE_DOT11_SUPPORT
				*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter | pFiltMod->Dot11PacketFilter;
#else
				*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter;
#endif
				ClonedRequest->DATA.SET_INFORMATION.InformationBuffer = pBuffer;
			}
		}

		Context = (PFILTER_REQUEST_CONTEXT)(&ClonedRequest->SourceReserved[0]);
		*Context = Request; //SourceReserved != NULL indicates that this is other module's request

		bSubmitted = TRUE;

		//
		// Use same request ID
		//
		ClonedRequest->RequestId = Request->RequestId;

		pFiltMod->PendingOidRequest = ClonedRequest;

		Status = NdisFOidRequest(pFiltMod->AdapterHandle, ClonedRequest);

		if (Status != NDIS_STATUS_PENDING)
		{
			NPF_OidRequestComplete(pFiltMod, ClonedRequest, Status);
			Status = NDIS_STATUS_PENDING;
		}


	}while (bFalse);

	if (bSubmitted == FALSE)
	{
		switch(Request->RequestType)
		{
			case NdisRequestMethod:
				Request->DATA.METHOD_INFORMATION.BytesRead = 0;
				Request->DATA.METHOD_INFORMATION.BytesNeeded = 0;
				Request->DATA.METHOD_INFORMATION.BytesWritten = 0;
				break;

			case NdisRequestSetInformation:
				Request->DATA.SET_INFORMATION.BytesRead = 0;
				Request->DATA.SET_INFORMATION.BytesNeeded = 0;
				break;

			case NdisRequestQueryInformation:
			case NdisRequestQueryStatistics:
			default:
				Request->DATA.QUERY_INFORMATION.BytesWritten = 0;
				Request->DATA.QUERY_INFORMATION.BytesNeeded = 0;
				break;
		}

	}

	TRACE_EXIT();
	return Status;

}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_CancelOidRequest(
	NDIS_HANDLE             FilterModuleContext,
	PVOID                   RequestId
	)
/*++

Routine Description:

	Cancels an OID request

	If your filter driver does not intercept and hold onto any OID requests,
	then you do not need to implement this routine.  You may simply omit it.
	Furthermore, if the filter only holds onto OID requests so it can pass
	down a clone (the most common case) the filter does not need to implement
	this routine; NDIS will then automatically request that the lower-level
	filter/miniport cancel your cloned OID.

	Most filters do not need to implement this routine.

Arguments:

	FilterModuleContext   - our filter
	RequestId             - identifies the request(s) to cancel

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	PNDIS_OID_REQUEST                   Request = NULL;
	PFILTER_REQUEST_CONTEXT             Context;
	PNDIS_OID_REQUEST                   OriginalRequest = NULL;

	FILTER_ACQUIRE_LOCK(&pFiltMod->OIDLock, NPF_IRQL_UNKNOWN);

	Request = pFiltMod->PendingOidRequest;

	if (Request != NULL)
	{
		Context = (PFILTER_REQUEST_CONTEXT)(&Request->SourceReserved[0]);

		OriginalRequest = (*Context);
	}

	if ((OriginalRequest != NULL) && (OriginalRequest->RequestId == RequestId))
	{
		FILTER_RELEASE_LOCK(&pFiltMod->OIDLock, NPF_IRQL_UNKNOWN);

		NdisFCancelOidRequest(pFiltMod->AdapterHandle, RequestId);
	}
	else
	{
		FILTER_RELEASE_LOCK(&pFiltMod->OIDLock, NPF_IRQL_UNKNOWN);
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_OidRequestComplete(
	NDIS_HANDLE         FilterModuleContext,
	PNDIS_OID_REQUEST   Request,
	NDIS_STATUS         Status
	)
/*++

Routine Description:

	Notification that an OID request has been completed

	If this filter sends a request down to a lower layer, and the request is
	pended, the FilterOidRequestComplete routine is invoked when the request
	is complete.  Most requests we've sent are simply clones of requests
	received from a higher layer; all we need to do is complete the original
	higher request.

	However, if this filter driver sends original requests down, it must not
	attempt to complete a pending request to the higher layer.

Arguments:

	FilterModuleContext   - our filter context area
	NdisRequest           - the completed request
	Status                - completion status

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	PNDIS_OID_REQUEST                   OriginalRequest;
	PFILTER_REQUEST_CONTEXT             Context;

	TRACE_ENTER();

	Context = (PFILTER_REQUEST_CONTEXT)(&Request->SourceReserved[0]);
	OriginalRequest = (*Context);

	//
	// This is an internal request
	//
	if (OriginalRequest == NULL)
	{
		TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Status = %#x", Status);
		NPF_InternalRequestComplete(pFiltMod, Request, Status);
		TRACE_EXIT();
		return;
	}


	FILTER_ACQUIRE_LOCK(&pFiltMod->OIDLock, NPF_IRQL_UNKNOWN);

	NT_ASSERT(pFiltMod->PendingOidRequest == Request);
	pFiltMod->PendingOidRequest = NULL;

	FILTER_RELEASE_LOCK(&pFiltMod->OIDLock, NPF_IRQL_UNKNOWN);


	//
	// Copy the information from the returned request to the original request
	//
	switch(Request->RequestType)
	{
		case NdisRequestMethod:
			OriginalRequest->DATA.METHOD_INFORMATION.OutputBufferLength =  Request->DATA.METHOD_INFORMATION.OutputBufferLength;
			OriginalRequest->DATA.METHOD_INFORMATION.BytesRead = Request->DATA.METHOD_INFORMATION.BytesRead;
			OriginalRequest->DATA.METHOD_INFORMATION.BytesNeeded = Request->DATA.METHOD_INFORMATION.BytesNeeded;
			OriginalRequest->DATA.METHOD_INFORMATION.BytesWritten = Request->DATA.METHOD_INFORMATION.BytesWritten;
			break;

		case NdisRequestSetInformation:
			OriginalRequest->DATA.SET_INFORMATION.BytesRead = Request->DATA.SET_INFORMATION.BytesRead;
			OriginalRequest->DATA.SET_INFORMATION.BytesNeeded = Request->DATA.SET_INFORMATION.BytesNeeded;
            if (OriginalRequest->DATA.SET_INFORMATION.InformationBuffer != Request->DATA.SET_INFORMATION.InformationBuffer)
            {
                /* We modified the data on clone, e.g. OID_GEN_CURRENT_PACKET_FILTER */
                ExFreePoolWithTag(Request->DATA.SET_INFORMATION.InformationBuffer, NPF_CLONE_OID_TAG);
            }
			break;

		case NdisRequestQueryInformation:
		case NdisRequestQueryStatistics:
		default:
			OriginalRequest->DATA.QUERY_INFORMATION.BytesWritten = Request->DATA.QUERY_INFORMATION.BytesWritten;
			OriginalRequest->DATA.QUERY_INFORMATION.BytesNeeded = Request->DATA.QUERY_INFORMATION.BytesNeeded;
			break;
	}



	(*Context) = NULL;

	NdisFreeCloneOidRequest(pFiltMod->AdapterHandle, Request);

	NdisFOidRequestComplete(pFiltMod->AdapterHandle, OriginalRequest, Status);

	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_Status(
	NDIS_HANDLE             FilterModuleContext,
	PNDIS_STATUS_INDICATION StatusIndication
	)
/*++

Routine Description:

	Status indication handler

Arguments:

	FilterModuleContext     - our filter context
	StatusIndication        - the status being indicated

NOTE: called at <= DISPATCH_LEVEL

  FILTER driver may call NdisFIndicateStatus to generate a status indication to
  all higher layer modules.

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;

// 	TRACE_ENTER();
// 	IF_LOUD(DbgPrint("NPF: Status Indication\n");)

	IF_LOUD(DbgPrint("status %x\n", StatusIndication->StatusCode);)

	//
	// The filter may do processing on the status indication here, including
	// intercepting and dropping it entirely.  However, the sample does nothing
	// with status indications except pass them up to the higher layer.  It is
	// more efficient to omit the FilterStatus handler entirely if it does
	// nothing, but it is included in this sample for illustrative purposes.
	//
	NdisFIndicateStatus(pFiltMod->AdapterHandle, StatusIndication);

/*	TRACE_EXIT();*/
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_DevicePnPEventNotify(
	NDIS_HANDLE             FilterModuleContext,
	PNET_DEVICE_PNP_EVENT   NetDevicePnPEvent
	)
/*++

Routine Description:

	Device PNP event handler

Arguments:

	FilterModuleContext         - our filter context
	NetDevicePnPEvent           - a Device PnP event

NOTE: called at PASSIVE_LEVEL

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	NDIS_DEVICE_PNP_EVENT  DevicePnPEvent = NetDevicePnPEvent->DevicePnPEvent;

/*	TRACE_ENTER();*/

	//
	// The filter may do processing on the event here, including intercepting
	// and dropping it entirely.  However, the sample does nothing with Device
	// PNP events, except pass them down to the next lower* layer.  It is more
	// efficient to omit the FilterDevicePnPEventNotify handler entirely if it
	// does nothing, but it is included in this sample for illustrative purposes.
	//
	// * Trivia: Device PNP events percolate DOWN the stack, instead of upwards
	// like status indications and Net PNP events.  So the next layer is the
	// LOWER layer.
	//

	switch (DevicePnPEvent)
	{

		case NdisDevicePnPEventQueryRemoved:
		case NdisDevicePnPEventRemoved:
		case NdisDevicePnPEventSurpriseRemoved:
		case NdisDevicePnPEventQueryStopped:
		case NdisDevicePnPEventStopped:
		case NdisDevicePnPEventPowerProfileChanged:
		case NdisDevicePnPEventFilterListChanged:
			break;

		default:
			IF_LOUD(DbgPrint("FilterDevicePnPEventNotify: Invalid event.\n");)
			break;
	}

	NdisFDevicePnPEventNotify(pFiltMod->AdapterHandle, NetDevicePnPEvent);

/*	TRACE_EXIT();*/
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_NetPnPEvent(
	NDIS_HANDLE              FilterModuleContext,
	PNET_PNP_EVENT_NOTIFICATION NetPnPEventNotification
	)
/*++

Routine Description:

	Net PNP event handler

Arguments:

	FilterModuleContext         - our filter context
	NetPnPEventNotification     - a Net PnP event

NOTE: called at PASSIVE_LEVEL

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	NDIS_STATUS               Status = NDIS_STATUS_SUCCESS;

	TRACE_ENTER();

	//
	// The filter may do processing on the event here, including intercepting
	// and dropping it entirely.  However, the sample does nothing with Net PNP
	// events, except pass them up to the next higher layer.  It is more
	// efficient to omit the FilterNetPnPEvent handler entirely if it does
	// nothing, but it is included in this sample for illustrative purposes.
	//

	Status = NdisFNetPnPEvent(pFiltMod->AdapterHandle, NetPnPEventNotification);

	TRACE_EXIT();
	return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_ReturnEx(
	NDIS_HANDLE         FilterModuleContext,
	PNET_BUFFER_LIST    NetBufferLists,
	ULONG               ReturnFlags
	)
/*++

Routine Description:

	FilterReturnNetBufferLists handler.
	FilterReturnNetBufferLists is an optional function. If provided, NDIS calls
	FilterReturnNetBufferLists to return the ownership of one or more NetBufferLists
	and their embedded NetBuffers to the filter driver. If this handler is NULL, NDIS
	will skip calling this filter when returning NetBufferLists to the underlying
	miniport and will call the next lower driver in the stack. A filter that doesn't
	provide a FilterReturnNetBufferLists handler cannot originate a receive indication
	on its own.

Arguments:

	FilterInstanceContext       - our filter context area
	NetBufferLists              - a linked list of NetBufferLists that this
								  filter driver indicated in a previous call to
								  NdisFIndicateReceiveNetBufferLists
	ReturnFlags                 - flags specifying if the caller is at DISPATCH_LEVEL

--*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;

/*	TRACE_ENTER();*/

	// Return the received NBLs.  If you removed any NBLs from the chain, make
	// sure the chain isn't empty (i.e., NetBufferLists!=NULL).
	NdisFReturnNetBufferLists(pFiltMod->AdapterHandle, NetBufferLists, ReturnFlags);

/*	TRACE_EXIT();*/
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_CancelSendNetBufferLists(
	NDIS_HANDLE             FilterModuleContext,
	PVOID                   CancelId
	)
/*++

Routine Description:

	This function cancels any NET_BUFFER_LISTs pended in the filter and then
	calls the NdisFCancelSendNetBufferLists to propagate the cancel operation.

	If your driver does not queue any send NBLs, you may omit this routine.
	NDIS will propagate the cancelation on your behalf more efficiently.

Arguments:

	FilterModuleContext      - our filter context area.
	CancelId                 - an identifier for all NBLs that should be dequeued

Return Value:

	None

*/
{
	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;

	NdisFCancelSendNetBufferLists(pFiltMod->AdapterHandle, CancelId);
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_SetModuleOptions(
	NDIS_HANDLE             FilterModuleContext
	)
/*++

Routine Description:

	This function set the optional handlers for the filter

Arguments:

	FilterModuleContext: The FilterModuleContext given to NdisFSetAttributes

Return Value:

	NDIS_STATUS_SUCCESS
	NDIS_STATUS_RESOURCES
	NDIS_STATUS_FAILURE

--*/
{
   NDIS_STATUS Status = NDIS_STATUS_SUCCESS;
   UNREFERENCED_PARAMETER(FilterModuleContext);

   return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
ULONG
NPF_GetPacketFilter(
	NDIS_HANDLE FilterModuleContext
	)
{
	TRACE_ENTER();

	ULONG PacketFilter = 0;
	ULONG BytesProcessed = 0;
	PVOID pBuffer = NULL;
	
	pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(PacketFilter), NPF_INTERNAL_OID_TAG);
    if (pBuffer == NULL)
    {
        IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
            TRACE_EXIT();
        return 0;
    }


	// get the PacketFilter when filter driver loads
	NPF_DoInternalRequest(FilterModuleContext,
		NdisRequestQueryInformation,
		OID_GEN_CURRENT_PACKET_FILTER,
		pBuffer,
		sizeof(PacketFilter),
		0,
		0,
		&BytesProcessed
		);

    PacketFilter = *(ULONG *)pBuffer;
    ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(PacketFilter))
	{
		IF_LOUD(DbgPrint("BytesProcessed != sizeof(PacketFilter), BytesProcessed = %#lx, sizeof(PacketFilter) = %#zx\n", BytesProcessed, sizeof(PacketFilter));)
		TRACE_EXIT();
		return 0;
	}
	else
	{
		TRACE_EXIT();
		return PacketFilter;
	}
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS
NPF_SetPacketFilter(
	POPEN_INSTANCE pOpen,
	ULONG PacketFilter
)
{
	TRACE_ENTER();

	NDIS_STATUS Status;
	ULONG BytesProcessed = 0;
	PVOID pBuffer = NULL;
	ULONG NewPacketFilter = 0;
	PSINGLE_LIST_ENTRY Prev = NULL;
	PSINGLE_LIST_ENTRY Curr = NULL;
	PNPCAP_FILTER_MODULE pFiltMod = pOpen->pFiltMod;
	LOCK_STATE_EX lockState;
	
	NT_ASSERT(pFiltMod != NULL);

#ifdef HAVE_WFP_LOOPBACK_SUPPORT
	if (pFiltMod->Loopback) {
		// We don't really support OID requests on our fake loopback
		// adapter, but we can pretend.
		return NDIS_STATUS_SUCCESS;
	}
#endif

	// Not modifying list, read-lock
	NdisAcquireRWLockRead(pFiltMod->OpenInstancesLock, &lockState, 0);
	pOpen->MyPacketFilter = PacketFilter;
	Prev = &(pFiltMod->OpenInstances);
	Curr = Prev->Next;
	while (Curr != NULL)
	{
		/* OR the filter in */
		NewPacketFilter |= CONTAINING_RECORD(Curr, OPEN_INSTANCE, OpenInstancesEntry)->MyPacketFilter;
		Prev = Curr;
		Curr = Prev->Next;
	}
	NdisReleaseRWLock(pFiltMod->OpenInstancesLock, &lockState);

#ifdef HAVE_DOT11_SUPPORT
	// Check if we should be setting the raw wifi filter
	if (pFiltMod->Dot11 && pFiltMod->Dot11PacketFilter == 0) {
		pFiltMod->Dot11PacketFilter = NDIS_PACKET_TYPE_802_11_RAW_DATA | NDIS_PACKET_TYPE_802_11_RAW_MGMT;
	}
	// If we had to set the raw wifi filter, then we need to issue the OID request below. Don't
	// bother checking anything else:
	else
#endif
	// If the new packet filter is the same as the old one...
	if (NewPacketFilter == pFiltMod->MyPacketFilter
		// ...or it wouldn't add to the upper one
		|| (NewPacketFilter & (~pFiltMod->HigherPacketFilter)) == 0)
       	{
		pFiltMod->MyPacketFilter = NewPacketFilter;
		// Nothing left to do!
		return NDIS_STATUS_SUCCESS;
	}


	pFiltMod->MyPacketFilter = NewPacketFilter;

	pBuffer = ExAllocatePoolWithTag(NonPagedPool, sizeof(PacketFilter), NPF_INTERNAL_OID_TAG);
	if (pBuffer == NULL)
	{
		IF_LOUD(DbgPrint("Allocate pBuffer failed\n");)
			TRACE_EXIT();
		return NDIS_STATUS_RESOURCES;
	}
#ifdef HAVE_DOT11_SUPPORT
	*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter | pFiltMod->Dot11PacketFilter;
#else
	*(PULONG) pBuffer = pFiltMod->HigherPacketFilter | pFiltMod->MyPacketFilter;
#endif

	// set the PacketFilter
	Status = NPF_DoInternalRequest(pFiltMod,
		NdisRequestSetInformation,
		OID_GEN_CURRENT_PACKET_FILTER,
		pBuffer,
		sizeof(PacketFilter),
		0,
		0,
		&BytesProcessed
	);

	ExFreePoolWithTag(pBuffer, NPF_INTERNAL_OID_TAG);

	if (BytesProcessed != sizeof(PacketFilter))
	{
		IF_LOUD(DbgPrint("BytesProcessed != sizeof(PacketFilter), BytesProcessed = %#lx, sizeof(PacketFilter) = %#zx\n", BytesProcessed, sizeof(PacketFilter));)
		Status = NDIS_STATUS_FAILURE;
	}
	return Status;
	TRACE_EXIT();
}

//-------------------------------------------------------------------

_Use_decl_annotations_
NDIS_STATUS NPF_DoInternalRequest(
	NDIS_HANDLE FilterModuleContext,
	NDIS_REQUEST_TYPE RequestType,
	NDIS_OID Oid,
	PVOID InformationBuffer,
	ULONG InformationBufferLength,
	ULONG OutputBufferLength,
	ULONG MethodId,
	PULONG pBytesProcessed)
{
	TRACE_ENTER();

	PNPCAP_FILTER_MODULE pFiltMod = (PNPCAP_FILTER_MODULE) FilterModuleContext;
	INTERNAL_REQUEST            FilterRequest = { 0 };
	PNDIS_OID_REQUEST           NdisRequest = &FilterRequest.Request;
	NDIS_STATUS                 Status = NDIS_STATUS_FAILURE;

	FilterRequest.RequestStatus = NDIS_STATUS_PENDING;

	*pBytesProcessed = 0;
	NdisZeroMemory(NdisRequest, sizeof(NDIS_OID_REQUEST));

	NdisInitializeEvent(&FilterRequest.InternalRequestCompletedEvent);
	NdisResetEvent(&FilterRequest.InternalRequestCompletedEvent);

	if (*((PVOID *) FilterRequest.Request.SourceReserved) != NULL)
	{
		*((PVOID *) FilterRequest.Request.SourceReserved) = NULL; //indicates this is a self-sent request
	}

	NdisRequest->Header.Type = NDIS_OBJECT_TYPE_OID_REQUEST;
	NdisRequest->Header.Revision = NDIS_OID_REQUEST_REVISION_1;
	NdisRequest->Header.Size = NDIS_SIZEOF_OID_REQUEST_REVISION_1;
	NdisRequest->RequestType = RequestType;
	NdisRequest->RequestHandle = pFiltMod->AdapterHandle;

	switch (RequestType)
	{
		case NdisRequestQueryInformation:
			NdisRequest->DATA.QUERY_INFORMATION.Oid = Oid;
			NdisRequest->DATA.QUERY_INFORMATION.InformationBuffer = InformationBuffer;
			NdisRequest->DATA.QUERY_INFORMATION.InformationBufferLength = InformationBufferLength;
			break;

		case NdisRequestSetInformation:
			NdisRequest->DATA.SET_INFORMATION.Oid = Oid;
			NdisRequest->DATA.SET_INFORMATION.InformationBuffer = InformationBuffer;
			NdisRequest->DATA.SET_INFORMATION.InformationBufferLength = InformationBufferLength;
			break;

		case NdisRequestMethod:
			NdisRequest->DATA.METHOD_INFORMATION.Oid = Oid;
			NdisRequest->DATA.METHOD_INFORMATION.MethodId = MethodId;
			NdisRequest->DATA.METHOD_INFORMATION.InformationBuffer = InformationBuffer;
			NdisRequest->DATA.METHOD_INFORMATION.InputBufferLength = InformationBufferLength;
			NdisRequest->DATA.METHOD_INFORMATION.OutputBufferLength = OutputBufferLength;
			break;

		default:
			IF_LOUD(DbgPrint("Unsupported RequestType: %d\n", RequestType);)
			IF_LOUD(DbgPrint("Status = %x\n", Status);)
			TRACE_EXIT();
			return Status;
			// break;
	}

	NdisRequest->RequestId = (PVOID)NPF_REQUEST_ID;

	Status = NdisFOidRequest(pFiltMod->AdapterHandle, NdisRequest);

	if (Status == NDIS_STATUS_PENDING)
	{
		// Wait for this event which is signaled by NPF_InternalRequestComplete,
		// which also sets RequestStatus appropriately
		NdisWaitEvent(&FilterRequest.InternalRequestCompletedEvent, 0);
		Status = FilterRequest.RequestStatus;
	}

	if (Status == NDIS_STATUS_SUCCESS)
	{
		if (RequestType == NdisRequestSetInformation)
		{
			*pBytesProcessed = NdisRequest->DATA.SET_INFORMATION.BytesRead;
		}

		if (RequestType == NdisRequestQueryInformation)
		{
			*pBytesProcessed = NdisRequest->DATA.QUERY_INFORMATION.BytesWritten;
		}

		if (RequestType == NdisRequestMethod)
		{
			*pBytesProcessed = NdisRequest->DATA.METHOD_INFORMATION.BytesWritten;
		}

		//
		// The driver below should set the correct value to BytesWritten
		// or BytesRead. But now, we just truncate the value to InformationBufferLength
		//
		if (RequestType == NdisRequestMethod)
		{
			if (*pBytesProcessed > OutputBufferLength)
			{
				*pBytesProcessed = OutputBufferLength;
			}
		}
		else
		{
			if (*pBytesProcessed > InformationBufferLength)
			{
				*pBytesProcessed = InformationBufferLength;
			}
		}
	}

	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "Status = %#x", Status);
	TRACE_EXIT();
	return Status;
}

//-------------------------------------------------------------------

_Use_decl_annotations_
VOID
NPF_InternalRequestComplete(
	NDIS_HANDLE                  FilterModuleContext,
	PNDIS_OID_REQUEST            NdisRequest,
	NDIS_STATUS                  Status
	)
/*++

Routine Description:

	NDIS entry point indicating completion of a pended NDIS_OID_REQUEST.

Arguments:

	FilterModuleContext - pointer to filter module context
	NdisRequest - pointer to NDIS request
	Status - status of request completion

Return Value:

	None

--*/
{
	PINTERNAL_REQUEST pRequest;

	UNREFERENCED_PARAMETER(FilterModuleContext);

	TRACE_ENTER();

	pRequest = CONTAINING_RECORD(NdisRequest, INTERNAL_REQUEST, Request);

	//
	// Set the request result
	//
	pRequest->RequestStatus = Status;
	TRACE_MESSAGE1(PACKET_DEBUG_LOUD, "pRequest->RequestStatus = Status = %x", Status);

	//
	// and awake the caller
	//
	NdisSetEvent(&pRequest->InternalRequestCompletedEvent);

	TRACE_EXIT();
}
