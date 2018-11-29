/****************************************************************************************
 * Copyright 2014 - 2016 Infineon Technologies AG ( www.infineon.com )					*
 *																						*
 * All rights reserved.																	*
 *																						*
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,				*
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,			*
 * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO			*
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,		*
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,					*
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;			*
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY				*
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR				*
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF				*
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.											*
 *																						*
 ****************************************************************************************/

/**
 *	@addtogroup	RunIFXTPMUpdate
 *	@brief		Implements the sample application RunIFXTPMUpdate.
 *	@details	Sample application that shows how to update the TPM firmware with the Infineon TPM Firmware Update Driver.
 *	@file		RunIFXTPMUpdate.c
 */
#include <Uefi.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/AdapterInformation.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/FirmwareManagement.h>

#include "IFXTPMUpdate.h"

/**
 *	@brief		Shows the usage of the program.
 *	@details	The function shows the usage of the program.
 */
void
ShowUsage()
{
	Print(L"Usage:\n");
	Print(L" RunIFXTPMUpdate.efi <update-type> <driver> <firmware> [policy-session-handle]\n");
	Print(L"\n");
	Print(L"Mandatory parameters:\n");
	Print(L" <update-type>:           The update type to use. Supports the following types:\n");
	Print(L"  tpm20                    Update a TPM2.0 using Policy Session authorization\n");
	Print(L"  tpm12-PP                 Update a TPM1.2 using Deferred Physical Presence\n");
	Print(L"  tpm12-owned              Update a TPM1.2 using TPM Owner authorization\n");
	Print(L" <driver>:                Path to the Infineon TPM Firmware Update Driver\n");
	Print(L" <firmware>:              Path to the TPM firmware image\n");
	Print(L"\n");
	Print(L"Additional parameters:\n");
	Print(L" [policy-session-handle]: Handle of Policy Session (hex, only usable for TPM2.0)\n");
	Print(L"\n");
	Print(L"Samples:\n");
	Print(L" RunIFXTPMUpdate.efi tpm20 IFXTPMUpdate.efi TPM20_5.51.2098.0_to_TPM12_4.40.119.0.bin 3000000\n");
	Print(L" RunIFXTPMUpdate.efi tpm12-PP IFXTPMUpdate.efi TPM12_4.40.119.0_to_TPM20_5.51.2098.0.bin\n");
	Print(L" RunIFXTPMUpdate.efi tpm12-owned IFXTPMUpdate.efi TPM12_4.40.119.0_to_TPM20_5.51.2098.0.bin\n");
}

/**
 *	@brief		Prints out a GUID to the console.
 *
 *	@param		PpGuid	Pointer to a GUID value.
 **/
void
PrintGuid(
	IN const EFI_GUID* PpGuid)
{
	Print(L"    Guid = {%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
		  PpGuid->Data1, PpGuid->Data2, PpGuid->Data3,
		  PpGuid->Data4[0], PpGuid->Data4[1], PpGuid->Data4[2], PpGuid->Data4[3],
		  PpGuid->Data4[4], PpGuid->Data4[5], PpGuid->Data4[6], PpGuid->Data4[7]);
}

/**
 *	@brief		Loads a file from disk.
 *	@details	The function allocates PppData with AllocateZeroPool(). It is the callers responsibility to free the memory with FreePool().
 *
 *	@param		PwszPath		Path to the file on disk. Omit the drive part.
 *	@param		PppData			Will receive a pointer to the file contents.
 *	@param		PpunSizeData	Will receive the size in bytes of the file contents.
 *
 *	@retval		EFI_SUCCESS		The function executed successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
LoadFile(
	IN const CHAR16* PwszPath,
	OUT VOID** PppData,
	OUT UINT32* PpunSizeData)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	SHELL_FILE_HANDLE hFile = NULL;
	UINT64 ullSize = 0;
	VOID* pBuffer = NULL;

	*PppData = NULL;
	*PpunSizeData = 0;

	do
	{
		efiStatus = ShellOpenFileByName((CHAR16*)PwszPath, &hFile, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);
		if (EFI_ERROR(efiStatus))
			break;

		efiStatus = ShellGetFileSize(hFile, &ullSize);
		if (EFI_ERROR(efiStatus))
			break;

		pBuffer = (VOID*)AllocateZeroPool(ullSize);

		efiStatus = ShellReadFile(hFile, &ullSize, pBuffer);
		if (EFI_ERROR(efiStatus))
			break;

		efiStatus = ShellCloseFile(&hFile);
		hFile = NULL;
		if (EFI_ERROR(efiStatus))
			break;

		*PppData = pBuffer;
		*PpunSizeData = (UINT32)ullSize;
	}
	while (FALSE);

	if (hFile != NULL)
		ShellCloseFile(&hFile);

	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Checks if IFXTPMUpdate.efi is loaded.
 *	@details	The function searches for IFXTPMUpdate.efi with @ref IFXTPMUpdate_ComponentName_GetDriverName "EFI_COMPONENT_NAME_PROTOCOL.GetDriverName()" and
 *				@ref IFXTPMUpdate_ComponentName_GetDriverName "EFI_COMPONENT_NAME2_PROTOCOL.GetDriverName()". It parses all loaded drivers for a driver with name "Infineon TPM Firmware Update Driver".
 *
 *	@retval		EFI_SUCCESS		The IFXTPMUpdate.efi driver is loaded.
 *	@retval		EFI_NOT_FOUND	The IFXTPMUpdate.efi driver is not loaded.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
IsDriverLoaded()
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_COMPONENT_NAME_PROTOCOL* pCompName = NULL;
	EFI_COMPONENT_NAME2_PROTOCOL* pCompName2 = NULL;
	BOOLEAN fIsLoaded = FALSE;
	UINTN ullHandleCount = 0;
	EFI_HANDLE* pHandleBuffer = NULL;
	UINTN ullIndex = 0;
	CHAR16* wszDriverName = NULL;
	// Driver name which is returned by IFXTPMUpdate.efi in EFI_COMPONENT_NAME_PROTOCOL and EFI_COMPONENT_NAME2_PROTOCOL
	CHAR16 wszInfineonDriverName[] = L"Infineon TPM Firmware Update Driver";

	Print(L"\nIsDriverLoaded()\n");
	Print(L"  Search driver with EFI_COMPONENT_NAME2_PROTOCOL:\n");
	efiStatus = gBS->LocateHandleBuffer(ByProtocol, &gEfiComponentName2ProtocolGuid, NULL, &ullHandleCount, &pHandleBuffer);
	for (ullIndex = 0; ullIndex < ullHandleCount; ullIndex++)
	{
		efiStatus = gBS->OpenProtocol(pHandleBuffer[ullIndex], &gEfiComponentName2ProtocolGuid, (VOID **)&pCompName2, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			continue;

		efiStatus = pCompName2->GetDriverName(pCompName2, "en", &wszDriverName);
		if (EFI_SUCCESS == efiStatus)
		{
			if (0 == (int)StrnCmp(wszDriverName, wszInfineonDriverName, sizeof(wszInfineonDriverName) / sizeof(wszInfineonDriverName[0])))
			{
				Print(L"    ComponentName2.Name: %s\n", wszDriverName);
				fIsLoaded = TRUE;
			}
		}

		efiStatus = gBS->CloseProtocol(pHandleBuffer[ullIndex], &gEfiComponentName2ProtocolGuid, gImageHandle, NULL);
	}
	if (NULL != pHandleBuffer)
		FreePool(pHandleBuffer);

	Print(L"  Search driver with EFI_COMPONENT_NAME_PROTOCOL:\n");
	efiStatus = gBS->LocateHandleBuffer(ByProtocol, &gEfiComponentNameProtocolGuid, NULL, &ullHandleCount, &pHandleBuffer);
	for (ullIndex = 0; ullIndex < ullHandleCount; ullIndex++)
	{
		efiStatus = gBS->OpenProtocol(pHandleBuffer[ullIndex], &gEfiComponentNameProtocolGuid, (VOID **)&pCompName, gImageHandle, NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			continue;

		efiStatus = pCompName->GetDriverName(pCompName, "eng;en", &wszDriverName);
		if (EFI_SUCCESS == efiStatus)
		{
			if (0 == (int)StrnCmp(wszDriverName, wszInfineonDriverName, sizeof(wszInfineonDriverName) / sizeof(wszInfineonDriverName[0])))
			{
				Print(L"    ComponentName.Name: %s\n", wszDriverName);
				fIsLoaded = TRUE;
			}
		}

		efiStatus = gBS->CloseProtocol(pHandleBuffer[ullIndex], &gEfiComponentNameProtocolGuid, gImageHandle, NULL);
	}
	if (NULL != pHandleBuffer)
		FreePool(pHandleBuffer);

	if (EFI_SUCCESS == efiStatus && !fIsLoaded)
		efiStatus = EFI_NOT_FOUND;

	Print(L"End IsDriverLoaded(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Loads the Infineon TPM Firmware Update Driver.
 *	@details	The function loads the Infineon TPM Firmware Update Driver.
 *
 *	@param		PwszDriverPath	Path to the driver image file.
 *	@param		PphDriver		Handle to the loaded driver.
 *
 *	@retval		EFI_SUCCESS		The driver was loaded successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
LoadDriver(
	IN const CHAR16* PwszDriverPath,
	OUT EFI_HANDLE* PphDriver)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	UINT32 unSize = 0;
	VOID* pBuffer = NULL;
	EFI_LOADED_IMAGE_PROTOCOL* pLoadedDriverImage = NULL;
	EFI_HANDLE hImage = NULL;
	UINTN ullExitDataSize = 0;

	Print(L"\nLoadDriver()\n");
	do
	{
		efiStatus = LoadFile(PwszDriverPath, &pBuffer, &unSize);
		Print(L"  LoadFileStatus: 0x%.16lX\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;

		efiStatus = gBS->LoadImage(FALSE, gImageHandle, NULL, pBuffer, unSize, &hImage);
		Print(L"  LoadImageStatus: 0x%.16lX\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;

		// Make sure it is a driver image
		efiStatus = gBS->HandleProtocol(hImage, &gEfiLoadedImageProtocolGuid, (VOID**)&pLoadedDriverImage);
		if ((NULL == pLoadedDriverImage) ||
				EFI_ERROR(efiStatus) ||
				(pLoadedDriverImage->ImageCodeType != EfiBootServicesCode && pLoadedDriverImage->ImageCodeType != EfiRuntimeServicesCode))
		{
			// Exit and unload the non-driver image
			gBS->UnloadImage(hImage);
			efiStatus = EFI_INVALID_PARAMETER;
			break;
		}

		efiStatus = gBS->StartImage(hImage, &ullExitDataSize, NULL);
		Print(L"    StartStatus: 0x%.16lX\n", efiStatus);
		*PphDriver = hImage;
		if (!EFI_ERROR(efiStatus))
			break;
	}
	while(FALSE);

	if (pBuffer != NULL)
		FreePool(pBuffer);

	Print(L"End LoadDriver(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Unloads the Infineon TPM Firmware Update Driver.
 *	@details	The function unloads the Infineon TPM Firmware Update Driver.
 *
 *	@param		PhDriver		Handle to the loaded driver.
 *
 *	@retval		EFI_SUCCESS		The driver was unloaded successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
UnloadDriver(
	IN EFI_HANDLE PhDriver)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	Print(L"\nUnloadDriver()\n");
	efiStatus = gBS->UnloadImage(PhDriver);
	Print(L"End UnloadDriver(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Callback function for logging of IFXTPMUpdate.efi.
 *	@details	The function is used as logging callback for IFXTPMUpdate.efi. IFXTPMUpdate.efi calls this function to log messages. The function appends the messages to a log file RunIFXTPMUpdate.log.
 *
 *	@param		PullBufferSize	Size of PwszBuffer in bytes including zero termination
 *	@param		PwszBuffer		Buffer containing the null-terminated message to be logged
 *
 *	@retval		EFI_SUCCESS		The callback executed successfully.
 *	@retval		other			The callback failed (will be ignored by IFXTPMUpdate.efi).
 */
EFI_STATUS
EFIAPI
LoggingCallback(
	IN UINTN PullBufferSize,
	IN CHAR16* PwszBuffer)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	SHELL_FILE_HANDLE hFile = NULL;

	do
	{
		CHAR16 wszLogFile[] = L"RunIFXTPMUpdate.log";
		efiStatus = ShellFileExists(wszLogFile);
		if (EFI_NOT_FOUND == efiStatus)
		{
			CHAR8 bom_ucs2le[] = {0xFF, 0xFE};
			UINTN ullSizeBom = sizeof(bom_ucs2le);

			efiStatus = ShellOpenFileByName(wszLogFile, &hFile, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ | EFI_FILE_MODE_CREATE, 0);
			if (EFI_ERROR(efiStatus))
				break;

			efiStatus = ShellWriteFile(hFile, &ullSizeBom, bom_ucs2le);
			if (EFI_ERROR(efiStatus))
				break;
		}
		else if (EFI_SUCCESS == efiStatus)
		{
			efiStatus = ShellOpenFileByName(wszLogFile, &hFile, EFI_FILE_MODE_WRITE | EFI_FILE_MODE_READ, 0);
			if (EFI_ERROR(efiStatus))
				break;

			efiStatus = ShellSetFilePosition(hFile, 0xFFFFFFFFFFFFFFFF);
			if (EFI_ERROR(efiStatus))
				break;
		}
		else
			break;

		// Write buffer without zero termination
		PullBufferSize -= 2;
		efiStatus = ShellWriteFile(hFile, &PullBufferSize, PwszBuffer);
		if (EFI_ERROR(efiStatus))
			break;

		efiStatus = ShellCloseFile(&hFile);
		if (EFI_ERROR(efiStatus))
			break;

		hFile = NULL;
	} while(FALSE);

	if (hFile != NULL)
		ShellCloseFile(&hFile);

	return EFI_SUCCESS;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Configures logging for IFXTPMUpdate.efi.
 *	@details	The function enables or disables logging with @ref IFXTPMUpdate_AdapterInformation_SetInformation "EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()".
 *
 *	@param		PhDriver				Handle to the driver.
 *	@param		PfEnableLogging			Set to TRUE to enable logging, set to FALSE otherwise.
 *
 *	@retval		EFI_SUCCESS		Logging was enabled or disabled successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
ConfigureLogging(
	IN EFI_HANDLE PhDriver,
	IN BOOLEAN PfEnableLogging)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_ADAPTER_INFORMATION_PROTOCOL* pAdapterInfo = NULL;
	Print(L"\nConfigureLogging\n");

	do
	{
		EFI_GUID guid = EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_LOGGING_1_GUID;
		EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_LOGGING_1 descriptor = {0};

		// Open AdapterInformation protocol
		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, (VOID**)&pAdapterInfo, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		// Call SetInformation()
		Print(L"  EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()\n");
		if (TRUE == PfEnableLogging)
		{
			Print(L"    Enable logging\n");
			descriptor.LogCallback = &LoggingCallback;
			descriptor.AddTimeStamps = TRUE;
		}
		else
		{
			Print(L"    Disable logging\n");
			descriptor.LogCallback = NULL;
		}
		efiStatus = pAdapterInfo->SetInformation(pAdapterInfo, &guid, &descriptor, sizeof(descriptor));
		Print(L"    Status: 0x%.16lX\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;
	}
	while (FALSE);	// Loop construct for error handling

	if (pAdapterInfo != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, gImageHandle, NULL);

	Print(L"End ConfigureLogging(), Status: 0x%.16lX\n", efiStatus);

	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Checks if the TPM can be updated.
 *	@details	The function calls @ref IFXTPMUpdate_FirmwareManagement_GetImageInfo "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.GetImageInfo()" to check whether the TPM can be updated.
 *
 *	@param		PhDriver				Handle to the driver.
 *	@param		PpfUpdatableOnce		Set to TRUE if TPM is only updatable once. FALSE if updatable multiple times.
 *
 *	@retval		EFI_SUCCESS						The TPM can be updated.
 *	@retval		EFI_IFXTPM_NO_MORE_UPDATES		The TPM cannot be updated.
 *	@retval		EFI_IFXTPM_RESTART_REQUIRED		The system must be restarted first.
 *	@retval		other							An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
IsTpmUpdatable(
	IN EFI_HANDLE PhDriver,
	OUT BOOLEAN* PpfUpdatableOnce)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_FIRMWARE_MANAGEMENT_PROTOCOL *pFmp = NULL;
	EFI_FIRMWARE_IMAGE_DESCRIPTOR* pImageInfo = NULL;
	CHAR16* wszPackageVersionName = NULL;
	Print(L"\nIsTpmUpdatable()\n");

	do
	{
		UINTN ullImageInfoSize = 0;
		UINT32 unDescriptorVersion = 0;
		UINT8 bDescriptorCount = 0;
		UINTN ullDescriptorSize = 0;
		UINT32 unPackageVersion = 0;

		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, (VOID**)&pFmp, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		// Obtain size required for image info structure
		Print(L"  EFI_FIRMWARE_MANAGEMENT_PROTOCOL.GetImageInfo()\n");
		efiStatus = pFmp->GetImageInfo(pFmp, &ullImageInfoSize, NULL, NULL, NULL, NULL, NULL, NULL);
		Print(L"    Status: 0x%.16lX\n", efiStatus);
		if (EFI_BUFFER_TOO_SMALL != efiStatus)
			break;

		pImageInfo = (EFI_FIRMWARE_IMAGE_DESCRIPTOR*)AllocateZeroPool(ullImageInfoSize);

		// Get the actual image info
		Print(L"  EFI_FIRMWARE_MANAGEMENT_PROTOCOL.GetImageInfo()\n");
		efiStatus = pFmp->GetImageInfo(pFmp, &ullImageInfoSize, pImageInfo, &unDescriptorVersion, &bDescriptorCount, &ullDescriptorSize, &unPackageVersion, &wszPackageVersionName);
		Print(L"    Status: 0x%.16lX\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;

		// Check if system restart is required
		if ((pImageInfo->AttributesSetting & IMAGE_ATTRIBUTE_IFXTPM_RESTART_REQUIRED) == IMAGE_ATTRIBUTE_IFXTPM_RESTART_REQUIRED)
		{
			// The value of attributes IMAGE_ATTRIBUTE_IFXTPM_LAST_UPDATE and IMAGE_ATTRIBUTE_IMAGE_UPDATABLE cannot be detected before the system is restarted.
			efiStatus = EFI_IFXTPM_RESTART_REQUIRED;
			break;
		}

		Print(L"    Version: %s\n", pImageInfo->VersionName);
		Print(L"    AttributesSetting: 0x%.16lX\n", pImageInfo->AttributesSetting);

		// Detect whether the TPM is still updatable
		if ((pImageInfo->AttributesSetting & IMAGE_ATTRIBUTE_IFXTPM_LAST_UPDATE) == IMAGE_ATTRIBUTE_IFXTPM_LAST_UPDATE)
			*PpfUpdatableOnce = TRUE;
		else if ((pImageInfo->AttributesSetting & IMAGE_ATTRIBUTE_IMAGE_UPDATABLE) == IMAGE_ATTRIBUTE_IMAGE_UPDATABLE)
			*PpfUpdatableOnce = FALSE;
		else
			efiStatus = EFI_IFXTPM_NO_MORE_UPDATES;
	}
	while (FALSE);	// Loop construct for error handling

	if (pFmp != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, gImageHandle, NULL);

	if (wszPackageVersionName != NULL)
		FreePool(wszPackageVersionName);

	if (pImageInfo != NULL)
		FreePool(pImageInfo);

	Print(L"End IsTpmUpdatable(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Loads a TPM firmware image from disk.
 *	@details	The function allocates PppFirmwareImage with AllocateZeroPool(). It is the callers responsibility to free the memory with FreePool().
 *
 *	@param		PwszPath				Path to the firmware image on disk. Omit the drive part.
 *	@param		PppFirmwareImage		Will receive pointer to the firmware image.
 *	@param		PpunSizeFirmwareImage	Will receive size in bytes of the firmware image.
 *
 *	@retval		EFI_SUCCESS		The function executed successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
LoadFirmwareImage(
	IN const CHAR16* PwszPath,
	OUT VOID** PppFirmwareImage,
	OUT UINT32* PpunSizeFirmwareImage)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	Print(L"\nLoadFirmwareImage()\n");
	efiStatus = LoadFile(PwszPath, PppFirmwareImage, PpunSizeFirmwareImage);
	Print(L"End LoadFirmwareImage(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Checks if the given firmware package can be used to update the TPM.
 *	@details	The function calls @ref IFXTPMUpdate_FirmwareManagement_CheckImage "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.CheckImage()"
 *				to check whether the TPM can be updated with the given firmware package.
 *
 *	@param		PhDriver				Handle to the driver.
 *	@param		PpFirmwareImage			Pointer to the firmware image.
 *	@param		PunSizeFirmwareImage	Size in bytes of the firmware image.
 *
 *	@retval		EFI_SUCCESS						The TPM can be updated with given firmware image.
 *	@retval		EFI_IFXTPM_WRONG_FIRMWARE_IMAGE	The TPM can not be updated with given firmware image.
 *	@retval		other							An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
IsTpmUpdatableWithFirmware(
	IN EFI_HANDLE PhDriver,
	IN const VOID* PpFirmwareImage,
	IN UINT32 PunSizeFirmwareImage)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_FIRMWARE_MANAGEMENT_PROTOCOL *pFmp = NULL;
	Print(L"\nIsTpmUpdatableWithFirmware()\n");

	do
	{
		UINT32 unUpdatable = 0;
		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, (VOID**)&pFmp, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		Print(L"  EFI_FIRMWARE_MANAGEMENT_PROTOCOL.CheckImage()\n");
		efiStatus = pFmp->CheckImage(pFmp, 1, PpFirmwareImage, PunSizeFirmwareImage, &unUpdatable);
		Print(L"    Status: 0x%.16lX\n", efiStatus);
		Print(L"    Updatable: 0x%8X\n", unUpdatable);

		// Check if TPM will be reset to factory defaults after firmware update
		if ((unUpdatable & IMAGE_UPDATABLE_IFXTPM_FACTORY_DEFAULTS) == IMAGE_UPDATABLE_IFXTPM_FACTORY_DEFAULTS)
			Print(L"    Attention! This firmware update resets the TPM to factory defaults, i.e. all\n    data and state from the old TPM firmware will be lost!\n");

		if (EFI_ERROR(efiStatus))
			break;

		if ((unUpdatable & IMAGE_UPDATABLE_VALID) != IMAGE_UPDATABLE_VALID)
		{
			efiStatus = EFI_IFXTPM_WRONG_FIRMWARE_IMAGE;
			break;
		}
	}
	while (FALSE);	// Loop construct for error handling

	if (pFmp != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, gImageHandle, NULL);

	Print(L"End IsTpmUpdatableWithFirmware(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Sets the policy session handle with @ref IFXTPMUpdate_AdapterInformation_SetInformation "EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()".
 *	@details	The function calls @ref IFXTPMUpdate_AdapterInformation_SetInformation "EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()" to set the policy session handle.
 *
 *	@param		PhDriver				Handle to the driver.
 *	@param		PunPolicySessionHandle	Handle to the policy session.
 *
 *	@retval		EFI_SUCCESS		The policy session handle was set successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
SetPolicySessionHandle(
	IN EFI_HANDLE PhDriver,
	IN UINT32 PunPolicySessionHandle)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_ADAPTER_INFORMATION_PROTOCOL* pAdapterInfo = NULL;
	Print(L"\nSetPolicySessionHandle()\n");

	do
	{
		UINTN ullCurrent = 0;
		EFI_GUID* pBuffers = NULL;
		UINTN ullBufferCount = 0;

		// AdapterInformation protocol
		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, (VOID**)&pAdapterInfo, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		// Call GetSupportedTypes()
		Print(L"  EFI_ADAPTER_INFORMATION_PROTOCOL.GetSupportedTypes()\n");
		efiStatus = pAdapterInfo->GetSupportedTypes(pAdapterInfo, &pBuffers, &ullBufferCount);
		Print(L"    Status: 0x%8X\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;

		Print(L"    Count: %d\n", ullBufferCount);
		for (ullCurrent = 0; ullCurrent < ullBufferCount; ullCurrent++)
			PrintGuid(&pBuffers[ullCurrent]);

		FreePool(pBuffers);
		pBuffers = NULL;

		{
			// Call SetInformation()
			EFI_GUID guid = EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_TPM20_1_GUID;
			EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_TPM20_1 descriptor;
			descriptor.SessionHandle = PunPolicySessionHandle;
			Print(L"  EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()\n");
			efiStatus = pAdapterInfo->SetInformation(pAdapterInfo, &guid, &descriptor, sizeof(descriptor));
			Print(L"    Status: 0x%.16lX\n", efiStatus);
			if (EFI_ERROR(efiStatus))
				break;
		}
	}
	while (FALSE);	// Loop construct for error handling

	if (pAdapterInfo != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, gImageHandle, NULL);

	Print(L"End SetPolicySessionHandle(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Sets TPM Owner authentication hash with @ref IFXTPMUpdate_AdapterInformation_SetInformation "EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()".
 *	@details	The function calls @ref IFXTPMUpdate_AdapterInformation_SetInformation "EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()" to set TPM Owner authentication hash.
 *
 *	@param		PhDriver				Handle to the driver.
 *
 *	@retval		EFI_SUCCESS		TPM Owner authentication hash was set successfully.
 *	@retval		other			An error occurred when executing this function.
 */
EFI_STATUS
EFIAPI
SetOwnerAuthHash(
	IN EFI_HANDLE PhDriver)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_ADAPTER_INFORMATION_PROTOCOL* pAdapterInfo = NULL;
	Print(L"\nSetOwnerAuthHash()\n");

	do
	{
		UINTN ullCurrent = 0;
		EFI_GUID* pBuffers = NULL;
		UINTN ullBufferCount = 0;

		// Open AdapterInformation protocol
		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, (VOID**)&pAdapterInfo, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		// Call GetSupportedTypes()
		Print(L"  EFI_ADAPTER_INFORMATION_PROTOCOL.GetSupportedTypes()\n");
		efiStatus = pAdapterInfo->GetSupportedTypes(pAdapterInfo, &pBuffers, &ullBufferCount);
		Print(L"    Status: 0x%8X\n", efiStatus);
		if (EFI_ERROR(efiStatus))
			break;

		Print(L"    Count: %d\n", ullBufferCount);
		for (ullCurrent = 0; ullCurrent < ullBufferCount; ullCurrent++)
			PrintGuid(&pBuffers[ullCurrent]);

		FreePool(pBuffers);
		pBuffers = NULL;

		{
			// Exemplary SHA-1 hash value of ASCII string '12345678' (assumes that TPM Ownership has been taken with this string as TPM Owner authentication)
			UINT8 rgbOwnerAuthHash[20] =
			{
				0x7c, 0x22, 0x2f, 0xb2, 0x92,
				0x7d, 0x82, 0x8a, 0xf2, 0x2f,
				0x59, 0x21, 0x34, 0xe8, 0x93,
				0x24, 0x80, 0x63, 0x7c, 0x0d
			};

			// Call SetInformation() to set TPM Owner authentication hash
			EFI_GUID guid = EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_TPM12_1_GUID;
			EFI_IFXTPM_FIRMWARE_UPDATE_DESCRIPTOR_TPM12_1 descriptor;
			CopyMem(descriptor.OwnerPasswordSha1, rgbOwnerAuthHash, sizeof(rgbOwnerAuthHash));
			Print(L"  EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation()\n");
			efiStatus = pAdapterInfo->SetInformation(pAdapterInfo, &guid, &descriptor, sizeof(descriptor));
			Print(L"    Status: 0x%.16lX\n", efiStatus);
			if (EFI_ERROR(efiStatus))
				break;
		}
	}
	while (FALSE);	// Loop construct for error handling

	if (pAdapterInfo != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiAdapterInformationProtocolGuid, gImageHandle, NULL);

	Print(L"End SetOwnerAuthHash(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Callback function for progress report of @ref IFXTPMUpdate_FirmwareManagement_SetImage "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()".
 *	@details	The function is called by @ref IFXTPMUpdate_FirmwareManagement_SetImage "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()" to update the progress (1 - 100). It prints the
 *				progress to the console.
 *
 *	@param		PullCompletion	Progress completion value between 1 and 100
 *
 *	@retval		EFI_SUCCESS		The callback executed successfully.
 */
EFI_STATUS
EFIAPI
ProgressCallback(
	IN UINTN PullCompletion)
{
	Print(L"    Completion: %d\r", PullCompletion);
	return EFI_SUCCESS;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Updates the TPM firmware.
 *	@details	The function calls @ref IFXTPMUpdate_FirmwareManagement_SetImage "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()" to update the TPM firmware.
 *
 *	@param		PhDriver				Handle to the driver.
 *	@param		PpFirmwareImage			Pointer to the firmware image.
 *	@param		PunSizeFirmwareImage	Size in bytes of the firmware image.
 *
 *	@retval		EFI_SUCCESS		The firmware was updated successfully.
 *	@retval		other			Errors returned by @ref IFXTPMUpdate_FirmwareManagement_SetImage "EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()".
 */
EFI_STATUS
EFIAPI
UpdateTpmFirmware(
	IN EFI_HANDLE PhDriver,
	IN const VOID* PpFirmwareImage,
	IN UINT32 PunSizeFirmwareImage)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_FIRMWARE_MANAGEMENT_PROTOCOL *pFmp = NULL;
	Print(L"\nUpdateTpmFirmware()\n\nDO NOT TURN OFF OR SHUT DOWN THE SYSTEM DURING THE UPDATE PROCESS!\n\n");

	do
	{
		CHAR16* wszAbortReason = NULL;

		efiStatus = gBS->OpenProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, (VOID**)&pFmp, gImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
		if (EFI_ERROR(efiStatus))
			break;

		Print(L"  EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage()\n");
		efiStatus = pFmp->SetImage(pFmp, 1, PpFirmwareImage, PunSizeFirmwareImage, NULL, &ProgressCallback, &wszAbortReason);
		Print(L"\n");
		if (EFI_ERROR(efiStatus))
			Print(L"    Abort reason: %s\n", wszAbortReason);
		if (wszAbortReason != NULL)
			FreePool(wszAbortReason);
	}
	while (FALSE);	// Loop construct for error handling

	if (pFmp != NULL)
		gBS->CloseProtocol(PhDriver, &gEfiFirmwareManagementProtocolGuid, gImageHandle, NULL);

	Print(L"End UpdateTpmFirmware(), Status: 0x%.16lX\n", efiStatus);
	return efiStatus;
}

/**
 *	@ingroup	RunIFXTPMUpdate
 *	@brief		Entry point for RunIFXTPMUpdate.efi.
 *	@details	The function demonstrates the usage of the IFXTPMUpdate.efi driver for updating TPM2.0 to TPM1.2 or updating TPM1.2 to TPM2.0.
 *
 *	@param		PullArgc			Number of command line arguments.
 *	@param		PpwszArgv			List of command line arguments.
 *
 *	@retval		EFI_SUCCESS		The entry point was executed successfully.
 *	@retval		other			An error occurred when executing this function.
 */
INTN
EFIAPI
ShellAppMain(
	IN UINTN PullArgc,
	IN CHAR16** PpwszArgv)
{
	EFI_STATUS efiStatus = EFI_DEVICE_ERROR;
	EFI_HANDLE hDriver = NULL;
	VOID* pFirmwareImage = NULL;
	UINT32 unSizeFirmwareImage = 0;
	CHAR16* pwszUpdateType = NULL;
	CHAR16* pwszDriverPath = NULL;
	CHAR16* pwszFirmwareImagePath = NULL;
	BOOLEAN fUpdatableOnce = FALSE;
	UINT32 unPolicySessionHandle = 0;
	BOOLEAN fDriverUnloaded = FALSE;

	// Check for mandatory command line arguments
	if (PullArgc < 4)
	{
		ShowUsage();
		return EFI_INVALID_PARAMETER;
	}

	pwszUpdateType = PpwszArgv[1];
	pwszDriverPath = PpwszArgv[2];
	pwszFirmwareImagePath = PpwszArgv[3];

	// Get policy session handle argument value, if present
	if (PullArgc > 4)
	{
		unPolicySessionHandle = (UINT32)ShellHexStrToUintn(PpwszArgv[4]);
		if (unPolicySessionHandle == -1)
		{
			ShowUsage();
			return EFI_INVALID_PARAMETER;
		}
	}

	Print(L"Parameters:\n");
	Print(L"  Update Type: %s\n", pwszUpdateType);
	Print(L"  Driver path: %s\n", pwszDriverPath);
	Print(L"  Firmware image path: %s\n", pwszFirmwareImagePath);
	if (PullArgc > 4)
		Print(L"  Policy session handle: 0x%.8X\n", unPolicySessionHandle);

	do
	{
		// Verify that IFXTPMUpdate.efi driver is not loaded.
		// (with EFI_COMPONENT_NAME_PROTOCOL.GetDriverName() and EFI_COMPONENT_NAME2_PROTOCOL.GetDriverName())
		efiStatus = IsDriverLoaded();
		if (EFI_NOT_FOUND != efiStatus)
			break;

		// Load the IFXTPMUpdate.efi driver.
		efiStatus = LoadDriver(pwszDriverPath, &hDriver);
		if (EFI_ERROR(efiStatus))
			break;

		// Verify that IFXTPMUpdate.efi driver is loaded. NOTE: This is an optional step, it can be omitted.
		efiStatus = IsDriverLoaded();
		if (EFI_ERROR(efiStatus))
			break;

		// Enable logging (optional)
		efiStatus = ConfigureLogging(hDriver, TRUE);
		if (EFI_ERROR(efiStatus))
			break;

		// Verify that the TPM allows updates with EFI_FIRMWARE_MANAGEMENT_PROTOCOL.GetImageInfo().
		efiStatus = IsTpmUpdatable(hDriver, &fUpdatableOnce);
		if (EFI_IFXTPM_NO_MORE_UPDATES == efiStatus)
			Print(L"- TPM cannot be updated.\n");
		if (EFI_IFXTPM_RESTART_REQUIRED == efiStatus)
			Print(L"- System restart required.\n");
		if (EFI_ERROR(efiStatus))
			break;
		if (!fUpdatableOnce)
			Print(L"- TPM can be updated multiple times.\n");
		else
			Print(L"- TPM can be updated one last time.\n");

		// Load the firmware image that shall be used to update the TPM.
		efiStatus = LoadFirmwareImage(pwszFirmwareImagePath, &pFirmwareImage, &unSizeFirmwareImage);
		if (EFI_ERROR(efiStatus))
			break;

		// Verify that the given firmware image can be used to update the TPM with EFI_FIRMWARE_MANAGEMENT_PROTOCOL.CheckImage().
		efiStatus = IsTpmUpdatableWithFirmware(hDriver, pFirmwareImage, unSizeFirmwareImage);
		if (EFI_ERROR(efiStatus))
			break;
		// Possible results:
		// - EFI_SUCCESS: This firmware can be flashed onto the TPM.
		// - EFI_IFXTPM_WRONG_FIRMWARE_IMAGE: This firmware cannot be flashed onto the TPM.

		if (PullArgc > 4)
			// Set the policy session handle with EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation() (only required when starting a TPM2.0 firmware update).
			efiStatus = SetPolicySessionHandle(hDriver, unPolicySessionHandle);
		if (EFI_ERROR(efiStatus))
			break;

		if (StrCmp(pwszUpdateType, L"tpm12-owned") == 0)
			// Set TPM Owner authentication hash with EFI_ADAPTER_INFORMATION_PROTOCOL.SetInformation() (only required for TPM1.2 if TPM Ownership has been taken).
			efiStatus = SetOwnerAuthHash(hDriver);
		if (EFI_ERROR(efiStatus))
			break;

		// Update the TPM firmware with EFI_FIRMWARE_MANAGEMENT_PROTOCOL.SetImage().
		efiStatus = UpdateTpmFirmware(hDriver, pFirmwareImage, unSizeFirmwareImage);
		if (EFI_ERROR(efiStatus))
			break;
		// Possible results:
		// - EFI_SUCCESS: The TPM Firmware was updated successfully.
		// - other: An error occurred while updating the TPM firmware, for example:
		// -- The policy session was incorrect
		// -- The update operation was started but failed
		// -- The firmware image cannot be used to update the TPM

		// Disable logging (only required in case logging has been enabled)
		efiStatus = ConfigureLogging(hDriver, FALSE);
		if (EFI_ERROR(efiStatus))
			break;

		// Unload the IFXTPMUpdate.efi driver.
		efiStatus = UnloadDriver(hDriver);
		if (EFI_ERROR(efiStatus))
			break;

		// Verify that IFXTPMUpdate.efi driver is not loaded. NOTE: This is an optional step, it can be omitted.
		efiStatus = IsDriverLoaded();
		if (EFI_NOT_FOUND != efiStatus)
			break;

		// Set flag to avoid redundant calls to IsDriverLoaded() and UnloadDriver()
		fDriverUnloaded = TRUE;
		efiStatus = EFI_SUCCESS;
	}
	while (FALSE);

	// For testing only, disable logging and unload driver in error scenarios.
	if (!fDriverUnloaded && IsDriverLoaded() == EFI_SUCCESS)
	{
		ConfigureLogging(hDriver, FALSE);
		UnloadDriver(hDriver);
	}

	if (pFirmwareImage != NULL)
		FreePool(pFirmwareImage);

	if (EFI_SUCCESS == efiStatus)
		Print(L"\n\nRunIFXTPMUpdate completed successfully.\n");
	else
		Print(L"\n\nRunIFXTPMUpdate failed, Status: 0x%.16lX\n", efiStatus);

	return efiStatus;
}
