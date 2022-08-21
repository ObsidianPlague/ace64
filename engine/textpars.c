// kgsws' ACE Engine
////
// Text parsing for scripts.
// Text parser offers various ways to parse text from memory.
// Memory used for text is in 'screens[]' so it is limited. Though the limit is quite high.
// This parser is DESCTRUCTIVE. Parsed script will be modified in memory.
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "textpars.h"

uint8_t *tp_text_ptr;
uint_fast8_t tp_is_string;

uint_fast8_t tp_enable_math;

static uint8_t script_char[2];
static uint8_t backup_char;

//
// funcs

static uint32_t is_script_char(uint8_t in)
{
	if(in == '{')
		return 1;
	if(in == '}')
		return 1;
	if(in == '[')
		return 1;
	if(in == ']')
		return 1;
	if(in == '#')
		return 1;
	if(in == ',')
		return 1;
	if(in == ';')
		return 1;
	if(in == ':')
		return 1;
	if(tp_enable_math)
	{
		if(in == '+')
			return 1;
		if(in == '-')
			return 1;
		if(in == '/')
			return 1;
		if(in == '*')
			return 1;
	}
	return 0;
}

//
// API - extra

uint64_t tp_hash64(uint8_t *name)
{
	// Converts string name into 64bit alias (pseudo-hash).
	// CRC64 could work too, but this does not require big table.
	uint64_t ret = 0;
	uint32_t sub = 0;

	while(*name)
	{
		uint8_t in = *name++;

		in &= 0x3F;

		ret ^= (uint64_t)in << sub;
		if(sub > 56)
			ret ^= (uint64_t)in >> (64 - sub);

		sub += 6;
		if(sub >= 64)
			sub -= 64;
	}

	return ret;
}

uint32_t tp_parse_fixed(uint8_t *text, fixed_t *value)
{
	fixed_t ret = 0;
	uint_fast8_t is_negative;

	if(!*text)
		return 1;

	if(*text == '-')
	{
		is_negative = 1;
		text++;
	} else
	{
		is_negative = 0;
		if(*text == '+')
			text++;
	}

	if(!*text)
		return 1;

	// integer part
	while(*text && *text != '.')
	{
		if(*text < '0' || *text > '9')
			return 1;
		ret *= 10;
		ret += *text - '0';
		text++;
	}

	ret <<= FRACBITS;

	// decimal part
	if(*text == '.')
	{
		text++;
		if(*text)
		{
			uint32_t num = 0;

			while(text[1])
				text++;

			while(*text != '.')
			{
				if(*text < '0' || *text > '9')
					return 1;
				num += (uint32_t)(*text - '0') << FRACBITS;
				num /= 10;
				text--;
			}

			ret |= num;
		}
	}

	// store
	if(value)
	{
		if(is_negative)
			*value = -ret;
		else
			*value = ret;
	}

	return 0;
}

//
// API

uint8_t *tp_get_keyword()
{
	// This function returns whitespace-separated keywords.
	uint8_t *ret, *ptr;

	if(backup_char)
	{
		// there's a script character waiting
		script_char[0] = backup_char;
		backup_char = 0;
		return script_char;
	}

	ptr = tp_text_ptr;

try_again:

	// end check
	if(!*ptr)
		return NULL;

	// skip whitespaces and newlines
	while(*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')
		ptr++;

	// end check
	if(!*ptr)
	{
		tp_text_ptr = ptr;
		return NULL;
	}

	tp_is_string = 0;

	// check for special handling
	if(*ptr == '"')
	{
		// string in quotes
		uint_fast8_t escaped = 0;
		uint8_t *dst;

		tp_is_string = 1;
		ptr++;

		// this is the keyword
		ret = ptr;

		// process string
		dst = ptr;
		while(1)
		{
			uint8_t in = *ptr++;

			if(!in || in == '\r' || in == '\n')
				I_Error("[TP] Unterminated string!");

			if(!escaped)
			{
				if(in == '"')
				{
					*dst = 0;
					break;
				} else
				if(in == '\\')
				{
					escaped = 1;
					continue;
				}
			} else
			{
				escaped = 0;
				if(in == 'n')
					in = '\n';
			}

			*dst++ = in;
		}
	} else
	if(*((uint16_t*)ptr) == 0x2F2F)
	{
		// single line comment
		while(*ptr && *ptr != '\r' && *ptr != '\n')
			ptr++;

		tp_text_ptr = ptr;
		goto try_again;
	} else
	if(*((uint16_t*)ptr) == 0x2A2F)
	{
		// multi line comment
		ptr += 2;
		while(*ptr)
		{
			if(*((uint16_t*)ptr) == 0x2F2A)
			{
				ptr += 2;
				break;
			}
			ptr++;
		}

		tp_text_ptr = ptr;
		goto try_again;
	} else
	if(is_script_char(*ptr))
	{
		// this is a scripting stuff
		// return just this character
		script_char[0] = *ptr;
		ret = script_char;
		ptr++;
	} else
	{
		// this is the keyword
		ret = ptr;

		// skip the text
		while(1)
		{
			uint8_t in = *ptr;

			if(!in)
				// end of file
				break;

			if(is_script_char(*ptr))
			{
				// backup
				backup_char = *ptr;
				// terminate string
				*ptr++ = 0;
				// stop
				break;
			}

			if(*ptr == ' ' || *ptr == '\t' || *ptr == '\r' || *ptr == '\n')
			{
				// terminate string
				*ptr++ = 0;
				// stop
				break;
			}

			ptr++;
		}
	}

	// update pointer
	tp_text_ptr = ptr;

	// return the keyword
	return ret;
}

uint8_t *tp_get_keyword_uc()
{
	// uppercase
	uint8_t *data;

	data = tp_get_keyword();
	if(!data)
		return NULL;

	for(uint8_t *ptr = data; *ptr; ptr++)
	{
		uint8_t in = *ptr;
		if(in >= 'A' && in <= 'Z')
			*ptr = in | 0x20;
	}

	return data;
}

uint32_t tp_skip_code_block(uint32_t depth)
{
	// Skip '{}' block without any parsing.
	uint8_t *kw;

	while(1)
	{
		kw = tp_get_keyword();
		if(!kw)
			// end of file
			return 1;

		if(kw[0] == '}')
		{
			depth--;
			if(!depth)
				// done
				return 0;
		}

		if(kw[0] == '{')
		{
			depth++;
			continue;
		}
	}
}

void tp_load_lump(lumpinfo_t *li)
{
	if(li->size > TP_MEMORY_SIZE)
		I_Error("TextParser: '%.8s' is too large! Limit is %u but size is %u.\n", TP_MEMORY_SIZE, li->size);

	tp_text_ptr = TP_MEMORY_ADDR;
	tp_text_ptr[li->size] = 0;

	wad_read_lump(tp_text_ptr, li - *lumpinfo, TP_MEMORY_SIZE);

	backup_char = 0;
}

