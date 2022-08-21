// kgsws' ACE Engine
////
// Various utility stuff.
//

enum
{
	// hooks that modify Doom memory
	HOOK_JMP_ACE,	// create a hook using 'jmp' opcode; destination is ACE Engine
	HOOK_JMP_DOOM,	// create a hook using 'jmp' opcode; destination is Doom code
	HOOK_CALL_ACE,	// create a hook using 'call' opcode; destination is ACE Engine
	HOOK_CALL_DOOM,	// create a hook using 'call' opcode; destination is Doom code
	HOOK_UINT32,	// replace 32bit value
	HOOK_UINT16,	// replace 16bit value
	HOOK_UINT8,	// replace 8bit value
	HOOK_CSTR_ACE,	// replace cstring from ACE segment
	HOOK_CSTR_DOOM,	// replace cstring from Doom DATA segment
	HOOK_BUF8_ACE,	// replace buffer from ACE segment
	HOOK_ABSADDR_CODE, // replace 32bit pointer destination to Doom CODE segment
	HOOK_ABSADDR_DATA, // replace 32bit pointer destination to Doom DATA segment
	HOOK_MOVE_OFFSET, // move block by offset
	HOOK_SET_NOPS, // memset with 0x90
	// 'hooks' that modify ACE memory
	HOOK_IMPORT,	// get address of any variable
	HOOK_READ8,	// get value of 8bit variable
	HOOK_READ16,	// get value of 16bit variable
	HOOK_READ32,	// get value of 32bit variable
	// address is referenced to
	CODE_HOOK = 0x40000000,
	DATA_HOOK = 0x80000000,
};

#define HOOK_MOVE_VAL(size,offset)	(((size) << 16) | ((offset) & 0xFFFF))

typedef struct
{
	uint32_t addr;
	uint32_t type;
	uint32_t value;
} hook_t;

#define DOOM_CODE_LOAD	282624
#define DOOM_DATA_LOAD	152829

extern uint32_t ace_segment;
extern uint32_t doom_code_segment;
extern uint32_t doom_data_segment;

void utils_init();
void utils_install_hooks(const hook_t *table, uint32_t count);

