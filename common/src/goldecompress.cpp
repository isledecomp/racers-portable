#include "goldecompress.h"

#include "compat.h"

#ifdef COMPAT_MODE

LegoU32 GolDecompress(LegoU8* p_src, LegoU8* p_dst)
{
	LegoU8* src = p_src;
	LegoU8* dst = p_dst;

	*dst++ = *src++;

	for (;;) {
		LegoS32 ctrl = ((*src++) << 24) + 1;
		for (;;) {
			if (ctrl >= 0) {
				// Overflow of a signed integer is undefined behavior, so cast to a unsigned one.
				ctrl = static_cast<LegoS32>(2 * static_cast<LegoU32>(ctrl));
				*dst++ = *src++;
				if (!(ctrl & 0xff)) {
					break;
				}
			}
			else {
				LegoU8 token = *src++;
				LegoS32 count = token & 0xf;
				LegoS32 back = ((token & 0xf0) << 4) | *src;

				if (count == 0) {
					count = -*++src - 18;
				}
				else {
					count -= 18;
				}
				src++;
				back = -back;
				if (back == 0) {
					return dst - p_dst;
				}

				do {
					count++;
					*dst = dst[back];
					dst++;
				} while (count != 0);

				// Overflow of a signed integer is undefined behavior, so cast to a unsigned one.
				ctrl = static_cast<LegoS32>(2 * static_cast<LegoU32>(ctrl));
				if (!(ctrl & 0xff)) {
					break;
				}
			}
		}
	}
}

#else

// FUNCTION: GOLDP 0x10032840
// FUNCTION: LEGORACERS 0x0044e9a0
__declspec(naked) LegoU32 GolDecompress(LegoU8* p_src, LegoU8* p_dst)
{
	__asm {
		push ebx
		push esi
		push edi
		xor eax, eax
		mov esi, dword ptr [esp + 0x10]
		mov edi, dword ptr [esp + 0x14]
		mov al, byte ptr [esi]
	storeByte:
		inc esi
		mov byte ptr [edi], al
		inc edi
	newCtrl:
		mov bl, byte ptr [esi]
		shl ebx, 0x18
		inc esi
		inc ebx
		js backRef
	bitLoop:
		add ebx, ebx
		mov al, byte ptr [esi]
		jp storeByte
		lea esi, [esi + 1]
		mov byte ptr [edi], al
		lea edi, [edi + 1]
		jns bitLoop
	backRef:
		mov al, byte ptr [esi]
		inc esi
		mov edx, eax
		and eax, 0xf
		rol edx, 4
		jne shortBack
		mov dl, byte ptr [esi]
		mov al, byte ptr [esi + 1]
		inc esi
		neg eax
		sub eax, 0x12
		jmp haveCount
	shortBack:
		mov dl, byte ptr [esi]
		sub eax, 0x12
	haveCount:
		neg edx
		lea esi, [esi + 1]
		je doneDec
	copyLoop:
		mov cl, byte ptr [edi + edx]
		inc eax
		mov byte ptr [edi], cl
		lea edi, [edi + 1]
		jne copyLoop
		add ebx, ebx
		jp newCtrl
		jns bitLoop
		jmp backRef
	doneDec:
		mov ecx, dword ptr [esp + 0x14]
		mov eax, edi
		pop edi
		pop esi
		pop ebx
		sub eax, ecx
		ret
	}
}

#endif
