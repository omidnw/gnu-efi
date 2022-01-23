#include <efi.h>
#include <elf.h>
#include <efilib.h>

// this is a not normal define, two variables need to be define for initialize header font.

#define fontidentifier_magic0 0x36
#define fontidentifier_magic1 0x04

// finish definition.

// typedef

typedef unsigned long long size_t;

// font struct.

typedef struct{
	unsigned char fontidentifier[2]; // magic array.
	unsigned char fontmode; // font mode.
	unsigned char charactersize; // size of character

} PSFone_HEADER;

typedef struct{
	PSFone_HEADER* psfone_HEADER;
	void* glyphBuffer;
} PSFone_Font;

// finish

// initialize all functions.

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
int memcmp(const void* firstpointer, const void* secondpointer, size_t x);
PSFone_Font* LoadPSFFont(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

// finish initialization.

// initialize font reader

PSFone_Font* LoadPSFFont(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_FILE* font = LoadFile(Directory, Path, ImageHandle, SystemTable);
	if(font == NULL)
	{
		return NULL; // check file exists.
	}
	PSFone_HEADER* fontHeader;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSFone_HEADER), (void**)&fontHeader);
	UINTN size = sizeof(PSFone_HEADER);
	font->Read(font, &size, fontHeader);
	if(fontHeader->fontidentifier[0] != fontidentifier_magic0 || 
	fontHeader->fontidentifier[1] != fontidentifier_magic1)
	{
		return NULL;
	}
	UINTN glyphBufferSize = fontHeader->charactersize * 256;
	if(fontHeader->fontmode == 1)// 512 glyph mode
	{
		glyphBufferSize = fontHeader->charactersize * 512;
	}
	void* glyphBuffer;
	{
		font->SetPosition(font, sizeof(PSFone_HEADER));
		SystemTable->BootServices->AllocatePool(EfiLoaderData, glyphBufferSize, (void**)&glyphBuffer);
		font->Read(font, &glyphBufferSize, glyphBuffer);
	}

	PSFone_Font* fontloader;
	SystemTable->BootServices->AllocatePool(EfiLoaderData, sizeof(PSFone_Font), (void**)&fontloader);
	fontloader->psfone_HEADER = fontHeader;
	fontloader->glyphBuffer = glyphBuffer;
	return fontloader;
}

// Finish initialization

// initialize Graphic Output Protocol

typedef struct{
	void* BaseAddress;
	size_t BufferSize;
	unsigned int HorizontalResolution;
	unsigned int VerticalResolution;
	unsigned int PixelsPerScanLine;
} FrameBuffer;

FrameBuffer framebuffer;

FrameBuffer* InitializeGOP()
{
	EFI_GUID gopGuid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
	EFI_GRAPHICS_OUTPUT_PROTOCOL* graphicoutputprotocol;
	EFI_STATUS status;

	status = uefi_call_wrapper(BS->LocateProtocol, 3, &gopGuid, NULL, (void**)&graphicoutputprotocol);

	if(EFI_ERROR(status))
	{
		Print(L"unable to locate graphics output protocol\n\r");
		return NULL;
	}
	else
	{
		Print(L"Graphic output protocol located successfully\n\r");
	}

	framebuffer.BaseAddress = (void*)graphicoutputprotocol->Mode->FrameBufferBase;
	framebuffer.BufferSize = graphicoutputprotocol->Mode->FrameBufferSize;
	framebuffer.HorizontalResolution = graphicoutputprotocol->Mode->Info->HorizontalResolution;
	framebuffer.VerticalResolution = graphicoutputprotocol->Mode->Info->VerticalResolution;
	framebuffer.PixelsPerScanLine = graphicoutputprotocol->Mode->Info->PixelsPerScanLine;

	return &framebuffer;
}

// Finish

// initialize memory compare function

int memcmp(const void* firstpointer, const void* secondpointer, size_t x) // verify bootloader and kernel os.
{
	const unsigned char *a = firstpointer, *b = secondpointer;
	for (size_t i = 0; i < x; i++)
	{
		if(a[i] < b[i])
		{
			return -1;
		}
		else if(a[i] > b[i])
		{
			return 1;
		}
	}
	return 0;
}

// Finish

// loadfile created to load kernel file.

EFI_FILE* LoadFile(EFI_FILE* Directory, CHAR16* Path, EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_FILE* LoadedFile; // create loadfile

	// load image file.
	EFI_LOADED_IMAGE_PROTOCOL* LoadedImage;
	SystemTable->BootServices->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (void**)&LoadedImage);

	// load folders from image file location.
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;
	SystemTable->BootServices->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (void**)&FileSystem);

	if(Directory == NULL)
	{
		FileSystem->OpenVolume(FileSystem, &Directory);
	}

	EFI_STATUS s = Directory->Open(Directory, &LoadedFile, Path, EFI_FILE_MODE_READ, EFI_FILE_READ_ONLY);

	if(s != EFI_SUCCESS)
	{
		return NULL;
	}
	else
	{
		return LoadedFile;
	}
}

// Finish

// EFI start point

EFI_STATUS efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
	InitializeLib(ImageHandle, SystemTable); // Initialize imagetable and systemtable.
	Print(L"AderinaOS Loaded First Version In 17/01/2022\n\r");

	EFI_FILE *Kernel = LoadFile(NULL, L"kernel.elf", ImageHandle, SystemTable);
	if(Kernel == NULL)
	{
		Print(L"Error: Failed to load kernel\n\r");
	}
	else
	{
		Print(L"Kernel file loaded successfully\n\r");
	}

	Elf64_Ehdr header;
	{
		UINTN FileInfoSize;
		EFI_FILE_INFO* FileInfo;

		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, NULL);
		SystemTable->BootServices->AllocatePool(EfiLoaderData, FileInfoSize, (void**)&FileInfo);
		Kernel->GetInfo(Kernel, &gEfiFileInfoGuid, &FileInfoSize, (void**)&FileInfo);

		UINTN size = sizeof(header);
		Kernel->Read(Kernel, &size, &header);
	}

	if(memcmp(&header.e_ident[EI_MAG0], ELFMAG, SELFMAG) != 0 ||
		header.e_ident[EI_CLASS] != ELFCLASS64 ||
		header.e_ident[EI_DATA] != ELFDATA2LSB ||
		header.e_type != ET_EXEC ||
		header.e_machine != EM_X86_64 ||
		header.e_version != EV_CURRENT
	)
	{
		Print(L"kernel format is bad\r\n");
	}
	else
	{
		Print(L"kernel header successfully verified\r\n");
	}

	Elf64_Phdr* pheaders;
	{
		Kernel->SetPosition(Kernel, header.e_phoff);
		UINTN size = header.e_phnum * header.e_phentsize;
		SystemTable->BootServices->AllocatePool(EfiLoaderData, size, (void**)&pheaders);
		Kernel->Read(Kernel, &size, pheaders);
	}

	for(Elf64_Phdr* pheader= pheaders;
	(char*)pheader<(char*)pheaders + header.e_phnum * header.e_phentsize;
	pheader = (Elf64_Phdr*)((char*)pheader + header.e_phentsize))
	{
		switch(pheader->p_type)
		{
			case PT_LOAD:
			{
				int pages = (pheader->p_memsz + 0x1000 - 1) / 0x1000;
				Elf64_Addr segment = pheader->p_paddr;
				SystemTable->BootServices->AllocatePages(AllocateAddress, EfiLoaderData, pages, &segment);

				Kernel->SetPosition(Kernel, pheader->p_offset);
				UINTN size = pheader->p_filesz;
				Kernel->Read(Kernel, &size, (void*)segment);
				break;
			}
		}
	}

	Print(L"Kernel Loaded Successfully\n\r");

	void(*KernelStart)(FrameBuffer*, PSFone_Font*) = ((__attribute__((sysv_abi)) void(*)(FrameBuffer*, PSFone_Font*)) header.e_entry);

	FrameBuffer* MainGOPBuffer = InitializeGOP();

	// Directory == NULL means root directory.
	PSFone_Font* newFontload = LoadPSFFont(NULL, L"zap-ext-light16.psf", ImageHandle, SystemTable);

	if(newFontload == NULL)
	{
		Print(L"font not found\n\r");
	}
	else
	{
		Print(L"font found. character size: %d\n\r", newFontload->psfone_HEADER->charactersize);
	}

	/*

	Print(L"Base: %d\n\rSize: %d\n\rWidth: %d\n\rHeight: %d\n\rPixelsPerScanLine: %d\n\r",
	MainGOPBuffer.BaseAddress,
	MainGOPBuffer.BufferSize,
	MainGOPBuffer.HorizontalResolution,
	MainGOPBuffer.VerticalResolution,
	MainGOPBuffer.PixelsPerScanLine);

	Print(L"Base: 0x%x\n\rSize: 0x%x\n\rWidth: %d\n\rHeight: %d\n\rPixelsPerScanLine: %d\n\r",
	MainGOPBuffer.BaseAddress,
	MainGOPBuffer.BufferSize,
	MainGOPBuffer.HorizontalResolution,
	MainGOPBuffer.VerticalResolution,
	MainGOPBuffer.PixelsPerScanLine);

	*/

	KernelStart(MainGOPBuffer, newFontload);

	return EFI_SUCCESS; // Exit the UEFI Application
}

// Finish
