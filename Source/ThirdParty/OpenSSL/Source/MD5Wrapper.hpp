#pragma once
#include "openssl/md5.h"
#include <cstring>
#include <cstdio>
#pragma warning(disable:4996)
struct FMD5Wrapper
{
	FMD5Wrapper()
	{
		std::memset(md5string,0,sizeof(md5string));
		MD5_Init(&Md5CTX);
	}

	inline void Update(const void *data, size_t len)
	{
		if(!bFinaled)
			MD5_Update(&Md5CTX, data, len);
	}
	inline char* Final()
	{
		unsigned char Digest[16] = { 0 };
		MD5_Final(Digest, &Md5CTX);
		for (int i = 0; i < 16; ++i)
			std::sprintf(&md5string[i * 2], "%02x", (unsigned int)Digest[i]);
		return md5string;
	}

	inline const char* GetMd5()const
	{
		if (bFinaled)
			return md5string;
		else
			return NULL;
	}

	inline void Reset()
	{
		bFinaled = false;
		std::memset(md5string, 0, sizeof(md5string));
		Md5CTX = MD5_CTX();
		MD5_Init(&Md5CTX);
	}

private:
	MD5_CTX Md5CTX;
	char md5string[33];
	bool bFinaled;
};