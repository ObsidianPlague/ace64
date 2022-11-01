// kgsws' ACE Engine
////

#define MVF_CRUSH	1
#define MVF_TOP_REVERSE	2
#define MVF_BOT_REVERSE	4

#define ACT_CEILING	1
#define ACT_FLOOR	2

enum
{
	DIR_UP,
	DIR_DOWN,
};

//

typedef struct
{
	thinker_t thinker;
	sector_t *sector;
	fixed_t top_height;
	fixed_t bot_height;
	fixed_t speed;
	uint16_t flags;
	uint8_t type;
	uint8_t direction;
	uint16_t delay;
	uint16_t wait;
} generic_mover_t;

//

generic_mover_t *generic_ceiling();
generic_mover_t *generic_ceiling_by_sector(sector_t *sec);

