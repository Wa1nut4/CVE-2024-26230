#pragma once

#include <Windows.h>
#include <rpc.h>
#include <iostream>

void string_handle_bind(const char *endpoint, RPC_BINDING_HANDLE &BindingHandle) {
	RPC_CSTR StringBinding;


	auto status = RpcStringBindingComposeA(
		(RPC_CSTR)"2f5f6520-ca46-1067-b319-00dd010662da",
		// ncacn_np   ncalrpc
		(RPC_CSTR)"ncalrpc",
		NULL, 
		//tapsrvlpc \\pipe\\tapsrv
		(RPC_CSTR)"tapsrvlpc",
		NULL,
		&StringBinding
	);
	std::cout << StringBinding << std::endl;
	if (status != RPC_S_OK) {
		std::cerr << "[string_handle_bind]>> RpcStringBindingComposeA failed" << std::endl;
	}

	status = RpcBindingFromStringBindingA(StringBinding, &BindingHandle);

	RpcStringFreeA(&StringBinding);

	if (status != RPC_S_OK) {
		std::cerr << "[string_handle_bind]>> RpcBindingFromStringBindingA failed" << std::endl;
		return ;
	}

	
}


void string_handle_unbind(RPC_BINDING_HANDLE BindingHandle) {
	if (BindingHandle) {
		RpcBindingFree(&BindingHandle);
	}
}