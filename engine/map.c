// kgsws' ACE Engine
////
#include "sdk.h"
#include "engine.h"
#include "utils.h"
#include "wadfile.h"
#include "decorate.h"
#include "mobj.h"
#include "inventory.h"
#include "animate.h"
#include "think.h"
#include "player.h"
#include "hitscan.h"
#include "config.h"
#include "demo.h"
#include "map.h"
#include "ldr_flat.h"

mapthing_t *playerstarts;
mapthing_t *deathmatchstarts;
mapthing_t **deathmatch_p;

uint32_t *nomonsters;
uint32_t *fastparm;
uint32_t *respawnparm;

uint32_t *netgame;
uint32_t *deathmatch;
uint32_t *gamemap;
uint32_t *gameepisode;
uint32_t *gameskill;
uint32_t *leveltime;

uint32_t *totalkills;
uint32_t *totalitems;
uint32_t *totalsecret;

uint32_t *skytexture;
uint32_t *skyflatnum;

uint32_t *numsides;
uint32_t *numlines;
uint32_t *numsectors;
line_t **lines;
vertex_t **vertexes;
side_t **sides;
sector_t **sectors;

uint32_t *prndindex;

plat_t **activeplats;
ceiling_t **activeceilings;

uint32_t *netgame;
uint32_t *usergame;

uint32_t *nofit;
uint32_t *crushchange;

fixed_t *tmdropoffz;
fixed_t *openrange;
fixed_t *opentop;
fixed_t *openbottom;

line_t **ceilingline;

mobj_t **linetarget;

fixed_t *bmaporgx;
fixed_t *bmaporgy;

uint8_t map_lump_name[9];
int32_t map_lump_idx;

uint_fast8_t map_skip_stuff;
uint_fast8_t is_title_map;

uint32_t num_clusters;
map_cluster_t *map_cluster;

static int32_t cluster_music;

//

static const uint8_t skillbits[] = {1, 1, 2, 4, 4};

// original clusters
static const map_cluster_t d2_cluster[] =
{
	[CLUSTER_D1_EPISODE1] = {.text_leave = (void*)0x000204C0, .lump_patch = -1},
	[CLUSTER_D1_EPISODE2] = {.text_leave = (void*)0x0002067C, .lump_patch = -1},
	[CLUSTER_D1_EPISODE3] = {.text_leave = (void*)0x00020850, .lump_patch = -1},
	[CLUSTER_D1_EPISODE4] = {.text_leave = NULL, .lump_patch = -1}, // text is added manually
	[CLUSTER_D2_1TO6] = {.text_leave = (void*)0x00020A40, .lump_patch = -1},
	[CLUSTER_D2_7TO11] = {.text_leave = (void*)0x00020BD8, .lump_patch = -1},
	[CLUSTER_D2_12TO20] = {.text_leave = (void*)0x00020E44, .lump_patch = -1},
	[CLUSTER_D2_21TO30] = {.text_leave = (void*)0x00020F80, .lump_patch = -1},
	[CLUSTER_D2_LVL31] = {.text_leave = (void*)0x00021170, .lump_patch = -1},
	[CLUSTER_D2_LVL32] = {.text_leave = (void*)0x00021218, .lump_patch = -1},
};
static const uint8_t *cluster_flat[DEF_CLUSTER_COUNT] =
{
	[CLUSTER_D1_EPISODE1] = (void*)0x000212A8,
	[CLUSTER_D1_EPISODE2] = (void*)0x000212B4,
	[CLUSTER_D1_EPISODE3] = (void*)0x000212BC,
	[CLUSTER_D1_EPISODE4] = NULL, // MFLR8_3
	[CLUSTER_D2_1TO6] = (void*)0x00021278,
	[CLUSTER_D2_7TO11] = (void*)0x00021280,
	[CLUSTER_D2_12TO20] = (void*)0x00021288,
	[CLUSTER_D2_21TO30] = (void*)0x00021290,
	[CLUSTER_D2_LVL31] = (void*)0x00021298,
	[CLUSTER_D2_LVL32] = (void*)0x000212A0,
};

// missing text
static uint8_t *ep4_text = "the spider mastermind must have sent forth\n"
	"its legions of hellspawn before your\n"
	"final confrontation with that terrible\n"
	"beast from hell.  but you stepped forward\n"
	"and brought forth eternal damnation and\n"
	"suffering upon the horde as a true hero\n"
	"would in the face of something so evil.\n"
	"\n"
	"besides, someone was gonna pay for what\n"
	"happened to daisy, your pet rabbit.\n"
	"\n"
	"but now, you see spread before you more\n"
	"potential pain and gibbitude as a nation\n"
	"of demons run amok among our cities.\n"
	"\n"
	"next stop, hell on earth!";

//
static const hook_t patch_new[];
static const hook_t patch_old[];

//
// callbacks

static uint32_t cb_free_inventory(mobj_t *mo)
{
	inventory_clear(mo);
	return 0;
}

//
// line scroller

__attribute((regparm(2),no_caller_saved_registers))
void think_line_scroll(line_scroll_t *ls)
{
	side_t *side = *sides + ls->line->sidenum[0];
	side->textureoffset += (fixed_t)ls->x * FRACUNIT;
	side->rowoffset += (fixed_t)ls->y * FRACUNIT;
}

static inline void spawn_line_scroll()
{
	for(uint32_t i = 0; i < *numlines; i++)
	{
		line_t *ln = *lines + i;

		if(ln->special == 48)
		{
			line_scroll_t *ls;

			ls = Z_Malloc(sizeof(line_scroll_t), PU_LEVEL, NULL);
			ls->line = ln;
			ls->x = 1;
			ls->y = 0;
			ls->thinker.function = think_line_scroll;
			think_add(&ls->thinker);
		}
	}
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
void map_load_setup()
{
	uint32_t cache;

	// viewactive = 1
	// automapactive = 0
	*paused = 0;
	*gamestate = GS_LEVEL;

	if(*gameepisode)
	{
		if(*gamemode)
			doom_sprintf(map_lump_name, "MAP%02u", *gamemap);
		else
			doom_sprintf(map_lump_name, "E%uM%u", *gameepisode, *gamemap);
		is_title_map = 0;
		cache = !*demoplayback;
	} else
	{
		doom_sprintf(map_lump_name, "TITLEMAP");
		*gameepisode = 1;
		*usergame = 0;
		*netgame = 0;
		*netdemo = 0;
		is_title_map = 1;
		cache = 0;
	}

	map_lump_idx = W_GetNumForName(map_lump_name); // TODO: do not crash

	// free old inventories
	mobj_for_each(cb_free_inventory);

	// reset netID
	mobj_netid = 1; // 0 is NULL, so start with 1

	think_clear();

	// apply game patches
	if(*demoplayback == DEMO_OLD)
		utils_install_hooks(patch_old, 0);
	else
		utils_install_hooks(patch_new, 0);

	P_SetupLevel();

	// reset some stuff
	for(uint32_t i = 0; i < MAXPLATS; i++)
		activeplats[i] = NULL;

	for(uint32_t i = 0; i < MAXCEILINGS; i++)
		activeceilings[i] = NULL;

	clear_buttons();

	if(cache)
		R_PrecacheLevel();

	// specials
	if(!map_skip_stuff)
	{
		P_SpawnSpecials();
		spawn_line_scroll();
	}
}

__attribute((regparm(2),no_caller_saved_registers))
static void spawn_map_thing(mapthing_t *mt)
{
	uint32_t idx;
	mobj_t *mo;
	mobjinfo_t *info;
	fixed_t x, y, z;

	// deathmatch starts
	if(mt->type == 11)
	{
		if(*deathmatch_p < deathmatchstarts + 10)
		{
			**deathmatch_p = *mt;
			*deathmatch_p = *deathmatch_p + 1;
		}
		return;
	}

	// player starts
	if(mt->type && mt->type <= 4)
	{
		playerstarts[mt->type - 1] = *mt;
		if(!*deathmatch && !map_skip_stuff)
			spawn_player(mt);
		return;
	}

	if(map_skip_stuff)
		return;

	// check network game
	if(!*netgame && mt->options & 16)
		return;

	// check skill level
	if(*gameskill > sizeof(skillbits) || !(mt->options & skillbits[*gameskill]))
		return;

	// backward search for type
	idx = num_mobj_types;
	do
	{
		idx--;
		if(mobjinfo[idx].doomednum == mt->type)
			break;
	} while(idx);
	if(!idx)
		idx = MOBJ_IDX_UNKNOWN;
	info = mobjinfo + idx;

	// 'not in deathmatch'
	if(*deathmatch && info->flags & MF_NOTDMATCH)
		return;

	// '-nomonsters'
	if(*nomonsters && info->flags1 & MF1_ISMONSTER)
		return;

	// position
	x = (fixed_t)mt->x << FRACBITS;
	y = (fixed_t)mt->y << FRACBITS;
	z = info->flags & MF_SPAWNCEILING ? 0x7FFFFFFF : 0x80000000;

	// spawn
	mo = P_SpawnMobj(x, y, z, idx);
	mo->spawnpoint = *mt;
	mo->angle = ANG45 * (mt->angle / 45);

	if(mo->tics > 0)
		mo->tics = 1 + (P_Random() % mo->tics);

	if(mt->options & 8)
		mo->flags |= MF_AMBUSH;

	if(mo->flags & MF_COUNTKILL)
		*totalkills = *totalkills + 1;

	if(mo->flags & MF_COUNTITEM)
		*totalitems = *totalitems + 1;
}

__attribute((regparm(2),no_caller_saved_registers))
static uint32_t check_door_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 26:
		case 32:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C90 + doom_data_segment;
		break;
		case 27:
		case 34:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022CB8 + doom_data_segment;
		break;
		case 28:
		case 33:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022CE0 + doom_data_segment;
		break;
		default:
			return 1;
	}

	if(inventory_check(mo, k0))
		return 1;

	if(inventory_check(mo, k1))
		return 1;

	pl->message = text;
	S_StartSound(mo, mo->info->player.sound.usefail);

	return 0;
}

__attribute((regparm(2),no_caller_saved_registers))
uint32_t check_obj_key(line_t *line, mobj_t *mo)
{
	player_t *pl = mo->player;
	uint16_t k0, k1;
	uint8_t *text;

	switch(line->special)
	{
		case 99:
		case 133:
			if(!pl)
				return 0;
			k0 = 47;
			k1 = 52;
			text = (uint8_t*)0x00022C08 + doom_data_segment;
		break;
		case 134:
		case 135:
			if(!pl)
				return 0;
			k0 = 48;
			k1 = 51;
			text = (uint8_t*)0x00022C34 + doom_data_segment;
		break;
		case 136:
		case 137:
			if(!pl)
				return 0;
			k0 = 49;
			k1 = 50;
			text = (uint8_t*)0x00022C60 + doom_data_segment;
		break;
		default:
			goto do_door;
	}

	if(inventory_check(mo, k0))
		goto do_door;

	if(inventory_check(mo, k1))
		goto do_door;

	pl->message = text;
	S_StartSound(mo, mo->info->player.sound.usefail);

	return 0;

do_door:
	return EV_DoDoor(line, 6); // only 'blazeOpen' is ever used here
}

//
// TITLEMAP

void map_start_title()
{
	int32_t lump;

	lump = W_CheckNumForName("TITLEMAP");
	if(lump < 0)
	{
		*gameaction = ga_nothing;
		*demosequence = -1;
		*advancedemo = 1;
		return;
	}

	if(*gameepisode)
		*wipegamestate = -1;
	else
		*wipegamestate = GS_LEVEL;

	*gameskill = sk_medium;
	*fastparm = 0;
	*respawnparm = 0;
	*nomonsters = 0;
	*deathmatch = 0;
	*gameepisode = 0;
	*gamemap = 1;
	*prndindex = 0;

	*consoleplayer = 0;
	memset(players, 0, sizeof(player_t) * MAXPLAYERS);
	players[0].playerstate = PST_REBORN;

	memset(playeringame, 0, sizeof(uint32_t) * MAXPLAYERS);
	playeringame[0] = 1;

	map_load_setup();
}

//
// API

void init_map()
{
	uint8_t text[16];

	doom_printf("[ACE] init MAPs\n");
	ldr_alloc_message = "Map and game info memory allocation failed!";

	// default cluster music
	if(*gamemode)
		doom_sprintf(text, "D_%s", (void*)0x00024B40 + doom_data_segment);
	else
		doom_sprintf(text, "D_%s", (void*)0x00024A30 + doom_data_segment);
	cluster_music = wad_check_lump(text);

	// allocate clusters
	map_cluster = ldr_malloc(mod_config.cluster_count * sizeof(map_cluster_t));

	// prepare default clusters
	for(uint32_t i = 0; i < DEF_CLUSTER_COUNT; i++)
	{
		const uint8_t *flat;

		map_cluster[i] = d2_cluster[i];
		map_cluster[i].lump_music = cluster_music;
		if(map_cluster[i].text_leave)
			map_cluster[i].text_leave += doom_data_segment;
		if(cluster_flat[i])
			flat = cluster_flat[i] + doom_data_segment;
		else
			flat = "MFLR8_3";
		map_cluster[i].flat_num = flatlump[flat_num_get(flat)];
	}
	map_cluster[CLUSTER_D1_EPISODE4].text_leave = ep4_text;
}

//
// hooks

__attribute((regparm(2),no_caller_saved_registers))
static void projectile_sky_flat(mobj_t *mo)
{
	if(mo->subsector && !(mo->flags1 & MF1_SKYEXPLODE))
	{
		sector_t *sec = mo->subsector->sector;
		if(mo->z <= sec->floorheight)
		{
			if(sec->floorpic == *skyflatnum)
			{
				P_RemoveMobj(mo);
				return;
			}
		} else
		if(mo->z + mo->height >= sec->ceilingheight)
		{
			if(sec->ceilingpic == *skyflatnum)
			{
				P_RemoveMobj(mo);
				return;
			}
		}
	}

	explode_missile(mo);
}

__attribute((regparm(2),no_caller_saved_registers))
static void projectile_sky_wall(mobj_t *mo)
{
	if(mo->flags1 & MF1_SKYEXPLODE)
		explode_missile(mo);
	else
		P_RemoveMobj(mo);
}

//
// hooks

static const hook_t patch_new[] =
{
	// fix 'A_Tracer' - make it leveltime based
	{0x00027E2A, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002CF80},
	// fix typo in 'P_DivlineSide'
	{0x0002EA00, CODE_HOOK | HOOK_UINT16, 0xCA39},
	// projectile sky explosion
	{0x0003137E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)projectile_sky_flat},
	{0x00031089, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)projectile_sky_wall},
	// replace 'P_PathTraverse' on multiple places
	{0x0002B5F6, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B616, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002B636, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_SlideMove
	{0x0002BBFA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_AimLineAttack
	{0x0002BC89, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_LineAttack
	{0x0002BD55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_path_traverse}, // P_UseLines
	// replace pointers to 'PTR_SlideTraverse' in 'P_SlideMove'
	{0x0002B5DD, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B5FC, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	{0x0002B61C, CODE_HOOK | HOOK_UINT32, (uint32_t)hs_slide_traverse},
	// enable sliding on things
	{0x0002B5EA, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B60F, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	{0x0002B62C, CODE_HOOK | HOOK_UINT8, PT_ADDLINES | PT_ADDTHINGS},
	// terminator
	{0}
};

static const hook_t patch_old[] =
{
	// restore 'A_Tracer'
	{0x00027E2A, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B3BC},
	// restore typo in 'P_DivlineSide'
	{0x0002EA00, CODE_HOOK | HOOK_UINT16, 0xC839},
	// projectile sky explosion
	{0x0003137E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)explode_missile},
	{0x00031089, CODE_HOOK | HOOK_CALL_DOOM, 0x00031660},
	// restore 'P_PathTraverse' on multiple places
	{0x0002B5F6, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002B616, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002B636, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_SlideMove
	{0x0002BBFA, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_AimLineAttack
	{0x0002BC89, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_LineAttack
	{0x0002BD55, CODE_HOOK | HOOK_CALL_DOOM, 0x0002C8A0}, // P_UseLines
	// restore pointers to 'PTR_SlideTraverse' in 'P_SlideMove'
	{0x0002B5DD, CODE_HOOK | HOOK_CALL_DOOM, 0x0002B4B0},
	{0x0002B5FC, CODE_HOOK | HOOK_CALL_DOOM, 0x0002B4B0},
	{0x0002B61C, CODE_HOOK | HOOK_CALL_DOOM, 0x0002B4B0},
	// disable sliding on things
	{0x0002B5EA, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	{0x0002B60F, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	{0x0002B62C, CODE_HOOK | HOOK_UINT8, PT_ADDLINES},
	// terminator
	{0}
};

static const hook_t hooks[] __attribute__((used,section(".hooks"),aligned(4))) =
{
	// replace 'D_StartTitle'
	{0x00022A4E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_start_title},
	{0x0001EB1F, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_start_title},
	// replace 'demoplayback' check for 'usergame' in 'G_Responder'
	{0x000201CE, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B40C},
	{0x000201D3, CODE_HOOK | HOOK_UINT8, 0x74},
	// replace 'demoplayback' check for 'usergame' in 'P_Ticker'
	{0x00032F83, CODE_HOOK | HOOK_ABSADDR_DATA, 0x0002B40C},
	{0x00032F88, CODE_HOOK | HOOK_UINT8, 0x74},
	// replace call to 'P_SetupLevel' in 'G_DoLoadLevel'
	{0x000200AA, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)map_load_setup},
	// replace call to 'P_SpawnMapThing' in 'P_LoadThings'
	{0x0002E1F9, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)spawn_map_thing},
	// disable call to 'P_SpawnSpecials' and 'R_PrecacheLevel' in 'P_SetupLevel'
	{0x0002E981, CODE_HOOK | HOOK_UINT16, 0x11EB},
	// disable line scroller and stuff cleanup in 'P_SpawnSpecials'
	{0x00030155, CODE_HOOK | HOOK_JMP_DOOM, 0x000301E1},
	// replace key checks in 'EV_VerticalDoor'
	{0x00026C85, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)check_door_key},
	{0x00026C8A, CODE_HOOK | HOOK_UINT16, 0xC085},
	{0x00026C8D, CODE_HOOK | HOOK_JMP_DOOM, 0x00026E97},
	{0x00026C8C, CODE_HOOK | HOOK_UINT16, 0x840F},
	{0x00026C92, CODE_HOOK | HOOK_JMP_DOOM, 0x00026D3E},
	// replace calls to 'EV_DoLockedDoor'
	{0x00030B0E, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	{0x00030E55, CODE_HOOK | HOOK_CALL_ACE, (uint32_t)hook_obj_key},
	// change map name variable in 'P_SetupLevel'
	{0x0002E858, CODE_HOOK | HOOK_UINT16, 0x61EB},
	{0x0002E8BB, CODE_HOOK | HOOK_UINT8, 0xB8},
	{0x0002E8BC, CODE_HOOK | HOOK_UINT32, (uint32_t)map_lump_name},
	// remove sky setup in 'G_InitNew'
	{0x00021803, CODE_HOOK | HOOK_UINT16, 0x68EB},
	// import variables
	{0x0002C0D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&playerstarts},
	{0x0002C154, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatchstarts},
	{0x0002C150, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch_p},
	{0x0002A3C0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nomonsters},
	{0x0002A3C4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&fastparm},
	{0x0002A3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&respawnparm},
	{0x0002B400, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netgame},
	{0x0002B3FC, DATA_HOOK | HOOK_IMPORT, (uint32_t)&deathmatch},
	{0x0002B3E8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gamemap},
	{0x0002B3F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameepisode},
	{0x0002B3E0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&gameskill},
	{0x0002CF80, DATA_HOOK | HOOK_IMPORT, (uint32_t)&leveltime},
	{0x0002B3C8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalsecret},
	{0x0002B3D0, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalitems},
	{0x0002B3D4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&totalkills},
	{0x0005A170, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skytexture},
	{0x0005A164, DATA_HOOK | HOOK_IMPORT, (uint32_t)&skyflatnum},
	{0x0002C11C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsides},
	{0x0002C134, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numlines},
	{0x0002C14C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&numsectors},
	{0x0002C120, DATA_HOOK | HOOK_IMPORT, (uint32_t)&lines},
	{0x0002C138, DATA_HOOK | HOOK_IMPORT, (uint32_t)&vertexes},
	{0x0002C118, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sides},
	{0x0002C148, DATA_HOOK | HOOK_IMPORT, (uint32_t)&sectors},
	{0x0002C040, DATA_HOOK | HOOK_IMPORT, (uint32_t)&activeplats},
	{0x0002B840, DATA_HOOK | HOOK_IMPORT, (uint32_t)&activeceilings},
	{0x0002B40C, DATA_HOOK | HOOK_IMPORT, (uint32_t)&usergame},
	{0x0002B400, DATA_HOOK | HOOK_IMPORT, (uint32_t)&netgame},
	{0x00012720, DATA_HOOK | HOOK_IMPORT, (uint32_t)&prndindex},
	// more variables
	{0x0002B990, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nofit},
	{0x0002B994, DATA_HOOK | HOOK_IMPORT, (uint32_t)&crushchange},
	{0x0002B9E4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&tmdropoffz},
	{0x0002C038, DATA_HOOK | HOOK_IMPORT, (uint32_t)&openrange},
	{0x0002C034, DATA_HOOK | HOOK_IMPORT, (uint32_t)&opentop},
	{0x0002C030, DATA_HOOK | HOOK_IMPORT, (uint32_t)&openbottom},
	{0x0002B9F4, DATA_HOOK | HOOK_IMPORT, (uint32_t)&ceilingline},
	{0x0002B9F8, DATA_HOOK | HOOK_IMPORT, (uint32_t)&linetarget},
	{0x0002B990, DATA_HOOK | HOOK_IMPORT, (uint32_t)&nofit},
	{0x0002C104, DATA_HOOK | HOOK_IMPORT, (uint32_t)&bmaporgx},
	{0x0002C108, DATA_HOOK | HOOK_IMPORT, (uint32_t)&bmaporgy},
};

