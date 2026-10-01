#include "StdInc.h"

#if defined(_WIN32) && defined(STATE_FIVE)
#include <windows.h>

#include <array>

#include <ServerInstanceBase.h>

namespace
{
struct CodePatch
{
	uint32_t rva;
	std::array<uint8_t, 8> original;
	std::array<uint8_t, 8> replacement;
};

// citizen-server-impl.dll 36897: turn both `continue` on a full reliable window in enet_protocol_send_reliable_outgoing_commands into a jump to the loop exit
constexpr uint32_t kTimeDateStamp = 0x6ABCDFA3;
constexpr uint32_t kSizeOfImage = 0x864000;
constexpr uint32_t kLoopExitRva = 0x1AE874;
constexpr std::array<uint8_t, 7> kLoopExit = { 0x44, 0x8B, 0x7C, 0x24, 0x70, 0xEB, 0x07 };

constexpr CodePatch kPatches[] = {
	{ 0x1AE66D, { 0x48, 0x8B, 0x1B, 0xE9, 0xE4, 0x01, 0x00, 0x00 }, { 0xE9, 0x02, 0x02, 0x00, 0x00, 0x90, 0x90, 0x90 } },
	{ 0x1AE6CE, { 0x48, 0x8B, 0x1B, 0xE9, 0x83, 0x01, 0x00, 0x00 }, { 0xE9, 0xA1, 0x01, 0x00, 0x00, 0x90, 0x90, 0x90 } },
};

const char* g_patchStatus = "^3ENet reliable window patch not applied: citizen-server-impl.dll not loaded.^7\n";

void PatchEnetReliableWindowScan()
{
	auto base = reinterpret_cast<uint8_t*>(GetModuleHandleW(L"citizen-server-impl.dll"));

	if (!base)
	{
		return;
	}

	auto ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + reinterpret_cast<const IMAGE_DOS_HEADER*>(base)->e_lfanew);

	if (ntHeaders->FileHeader.TimeDateStamp != kTimeDateStamp || ntHeaders->OptionalHeader.SizeOfImage != kSizeOfImage || memcmp(base + kLoopExitRva, kLoopExit.data(), kLoopExit.size()) != 0)
	{
		g_patchStatus = "^3ENet reliable window patch not applied: unexpected citizen-server-impl.dll build.^7\n";
		return;
	}

	for (const auto& patch : kPatches)
	{
		auto code = base + patch.rva;

		if (memcmp(code, patch.original.data(), patch.original.size()) != 0 && memcmp(code, patch.replacement.data(), patch.replacement.size()) != 0)
		{
			g_patchStatus = "^3ENet reliable window patch not applied: unexpected code in citizen-server-impl.dll.^7\n";
			return;
		}
	}

	for (const auto& patch : kPatches)
	{
		auto code = base + patch.rva;
		DWORD oldProtect;

		if (!VirtualProtect(code, patch.replacement.size(), PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			g_patchStatus = "^1ENet reliable window patch failed: VirtualProtect refused.^7\n";
			return;
		}

		memcpy(code, patch.replacement.data(), patch.replacement.size());
		VirtualProtect(code, patch.replacement.size(), oldProtect, &oldProtect);
	}

	FlushInstructionCache(GetCurrentProcess(), base + kPatches[0].rva, kLoopExitRva - kPatches[0].rva);

	g_patchStatus = "^2ENet reliable window patch applied.^7\n";
}
}

static InitFunction initFunction([]()
{
	PatchEnetReliableWindowScan();

	fx::ServerInstanceBase::OnServerCreate.Connect([](fx::ServerInstanceBase*)
	{
		trace("%s", g_patchStatus);
	});
});
#endif
