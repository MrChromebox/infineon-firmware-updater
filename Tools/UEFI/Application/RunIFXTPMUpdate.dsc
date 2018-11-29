##
#	@brief		Infineon TPM Tools for UEFI implementation.
#	@file		RunIFXTPMUpdate.dsc
#	@copyright	Copyright (c) 2013 - 2017 Infineon Technologies AG. This software is the confidential and proprietary information of Infineon Technologies AG. All rights reserved.
#
#	@internal
#	@date		2017/03/08
#	@endinternal
##

[Defines]
	PLATFORM_NAME				= RunIFXTPMUpdatePkg
	PLATFORM_GUID				= 84EFA6EE-D383-4206-9916-6B1B621BFFA3
	PLATFORM_VERSION			= 1.00
	DSC_SPECIFICATION			= 0x00010018
	OUTPUT_DIRECTORY			= Build/RunIFXTPMUpdatePkg
	SUPPORTED_ARCHITECTURES		= X64
	BUILD_TARGETS				= DEBUG|NOOPT|RELEASE
	SKUID_IDENTIFIER			= DEFAULT

[LibraryClasses]

# Basic
	BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
	BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
	PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf

# UEFI & PI
	DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
	MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
	UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
	UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
	UefiLib|MdePkg/Library/UefiLib/UefiLib.inf

# Generic Modules
	PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf

# Misc
	DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf

# Entry point
	UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf

# File I/O
	FileHandleLib|ShellPkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
	HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
	ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
	SortLib|ShellPkg/Library/UefiSortLib/UefiSortLib.inf
	UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf

# Input arguments
	ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf

[Components]
	RunIFXTPMUpdatePkg/RunIFXTPMUpdate.inf