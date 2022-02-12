#include "Utils.hpp"

#define NOMINMAX
#include <Windows.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "../sdk/structs.hpp"
#include "Math.hpp"

HANDLE _out = NULL, _old_out = NULL;
HANDLE _err = NULL, _old_err = NULL;
HANDLE _in = NULL, _old_in = NULL;

namespace Utils {
	unsigned int FindInDataMap(datamap_t* pMap, const char* name) {
		while (pMap) {
			for (int i = 0; i < pMap->dataNumFields; i++) {
				if (pMap->dataDesc[i].fieldName == NULL)
					continue;

				if (strcmp(name, pMap->dataDesc[i].fieldName) == 0)
					return pMap->dataDesc[i].fieldOffset[TD_OFFSET_NORMAL];

				if (pMap->dataDesc[i].fieldType == FIELD_EMBEDDED) {
					if (pMap->dataDesc[i].td) {
						unsigned int offset;

						if ((offset = FindInDataMap(pMap->dataDesc[i].td, name)) != 0)
							return offset;
					}
				}
			}
			pMap = pMap->baseMap;
		}
		return 0;
	}

	void AttachConsole() {
		_old_out = GetStdHandle(STD_OUTPUT_HANDLE);
		_old_err = GetStdHandle(STD_ERROR_HANDLE);
		_old_in = GetStdHandle(STD_INPUT_HANDLE);

		::AllocConsole() && ::AttachConsole(GetCurrentProcessId());

		_out = GetStdHandle(STD_OUTPUT_HANDLE);
		_err = GetStdHandle(STD_ERROR_HANDLE);
		_in = GetStdHandle(STD_INPUT_HANDLE);

		SetConsoleMode(_out,
			ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);

		SetConsoleMode(_in,
			ENABLE_INSERT_MODE | ENABLE_EXTENDED_FLAGS |
			ENABLE_PROCESSED_INPUT | ENABLE_QUICK_EDIT_MODE);
	}

	void DetachConsole() {
		if (_out && _err && _in) {
			FreeConsole();

			if (_old_out) SetStdHandle(STD_OUTPUT_HANDLE, _old_out);
			if (_old_err) SetStdHandle(STD_ERROR_HANDLE, _old_err);
			if (_old_in)  SetStdHandle(STD_INPUT_HANDLE, _old_in);
		}
	}

	bool ConsolePrint(const char* fmt, ...) {
		if (!_out)
			return false;

		char buf[1024];
		va_list va;

		va_start(va, fmt);
		_vsnprintf_s(buf, 1024, fmt, va);
		va_end(va);

		return !!WriteConsoleA(_out, buf, static_cast<DWORD>(strlen(buf)), nullptr, nullptr);
	}

	char ConsoleReadKey() {
		if (!_in) return false;

		auto key = char{ 0 };
		auto keysread = DWORD{ 0 };

		ReadConsoleA(_in, &key, 1, &keysread, nullptr);

		return key;
	}

	int WaitForModules(std::int32_t timeout, const std::initializer_list<std::wstring>& modules) {
		bool signaled[32] = { 0 };
		bool success = false;

		std::uint32_t totalSlept = 0;

		if (timeout == 0) {
			for (auto& mod : modules) {
				if (GetModuleHandleW(std::data(mod)) == NULL)
					return WAIT_TIMEOUT;
			}
			return WAIT_OBJECT_0;
		}

		if (timeout < 0) timeout = INT32_MAX;

		while (true) {
			for (auto i = 0u; i < modules.size(); ++i) {
				auto& module = *(modules.begin() + i);
				if (!signaled[i] && GetModuleHandleW(std::data(module)) != NULL) {
					signaled[i] = true;

					bool done = true;
					for (auto j = 0u; j < modules.size(); ++j) {
						if (!signaled[j]) {
							done = false;
							break;
						}
					}
					if (done) {
						success = true;
						goto exit;
					}
				}
			}
			if (totalSlept > std::uint32_t(timeout)) {
				break;
			}
			Sleep(10);
			totalSlept += 10;
		}

	exit:
		return success ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
	}

	std::uint8_t* PatternScan(void* module, const char* signature) {
		static auto pattern_to_byte = [](const char* pattern) {
			auto bytes = std::vector<int>{};
			auto start = const_cast<char*>(pattern);
			auto end = const_cast<char*>(pattern) + strlen(pattern);

			for (auto current = start; current < end; ++current) {
				if (*current == '?') {
					++current;
					if (*current == '?')
						++current;
					bytes.push_back(-1);
				}
				else bytes.push_back(strtoul(current, &current, 16));				
			}
			return bytes;
		};

		auto dosHeader = (PIMAGE_DOS_HEADER)module;
		auto ntHeaders = (PIMAGE_NT_HEADERS)((std::uint8_t*)module + dosHeader->e_lfanew);

		auto sizeOfImage = ntHeaders->OptionalHeader.SizeOfImage;
		auto patternBytes = pattern_to_byte(signature);
		auto scanBytes = reinterpret_cast<std::uint8_t*>(module);

		auto s = patternBytes.size();
		auto d = patternBytes.data();

		for (auto i = 0ul; i < sizeOfImage - s; ++i) {
			bool found = true;
			for (auto j = 0ul; j < s; ++j) {
				if (scanBytes[i + j] != d[j] && d[j] != -1) {
					found = false;
					break;
				}
			}
			if (found) return &scanBytes[i];			
		}
		return nullptr;
	}

	bool LineGoesThroughSmoke(Vector vStartPos, Vector vEndPos) {

		typedef bool(__cdecl* _LineGoesThroughSmoke) (Vector, Vector);
		static _LineGoesThroughSmoke LineGoesThroughSmokeFn = 0;

		if (!LineGoesThroughSmokeFn)
			LineGoesThroughSmokeFn = (_LineGoesThroughSmoke)Utils::PatternScan(GetModuleHandleW(L"client.dll"), ("55 8B EC 83 EC 08 8B 15 ? ? ? ? 0F 57 C0"));

		if (LineGoesThroughSmokeFn)
			return LineGoesThroughSmokeFn(vStartPos, vEndPos);
		return false;
	}

	void SetClantag(const char* tag) {
		static auto fnClantagChanged = (int(__fastcall*)(const char*, const char*))PatternScan(GetModuleHandleW(L"engine.dll"), "53 56 57 8B DA 8B F9 FF 15");

		fnClantagChanged(tag, tag);
	}

	void SetName(const char* name) {
		auto nameConvar = g_CVar->FindVar("name");
		nameConvar->m_fnChangeCallbacks.m_Size = 0;
		auto do_once = (nameConvar->SetValue("\n���"), true);
		nameConvar->SetValue(name);
	}

	void RankRevealAll() {
		g_CHLClient->DispatchUserMessage(50, 0, 0, nullptr);
	}

	float AngleDiff(float destAngle, float srcAngle) {
		float delta;

		delta = fmodf(destAngle - srcAngle, 360.0f);
		if (destAngle > srcAngle) if (delta >= 180) delta -= 360;
		else if (delta <= -180) delta += 360;

		return delta;
	}
}
