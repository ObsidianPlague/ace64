// kgsws' ACE Engine
////

extern void *parse_action_func;
extern void *parse_action_arg;

//

typedef struct
{
	uint16_t type;
	int16_t chance;
	uint16_t amount;
	uint16_t __padding;
} args_dropitem_t;

//

#define CMF_AIMOFFSET	1
#define CMF_AIMDIRECTION	2
#define CMF_TRACKOWNER	4
#define CMF_CHECKTARGETDEAD	8
#define CMF_ABSOLUTEPITCH	16
#define CMF_OFFSETPITCH	32
#define CMF_SAVEPITCH	64
#define CMF_ABSOLUTEANGLE	128

typedef struct
{
	uint16_t missiletype;
	uint8_t ptr;
	fixed_t spawnheight;
	fixed_t spawnofs_xy;
	angle_t angle;
	uint32_t flags;
	fixed_t pitch;
} args_SpawnProjectile_t;

//

typedef struct
{
	angle_t angle;
	uint8_t ptr;
	uint32_t flags;
} args_SetAngle_t;

//

uint8_t *action_parser(uint8_t *name);

