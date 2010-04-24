/*
 * player.c
 * Copyright (C) 2009, 2010 Joachim de Groot <jdegroot@web.de>
 *
 * NLarn is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * NLarn is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "container.h"
#include "display.h"
#include "food.h"
#include "game.h"
#include "nlarn.h"
#include "player.h"
#include "sobjects.h"

static const char aa1[] = "mighty evil master";
static const char aa2[] = "apprentice demi-god";
static const char aa3[] = "minor demi-god";
static const char aa4[] = "major demi-god";
static const char aa5[] = "minor deity";
static const char aa6[] = "major deity";
static const char aa7[] = "novice guardian";
static const char aa8[] = "apprentice guardian";

static const char *player_level_desc[] =
{
    "novice explorer",		"apprentice explorer",	"practiced explorer",	/* -3 */
    "expert explorer",		"novice adventurer",	"adventurer",			/* -6 */
    "apprentice conjurer",	"conjurer",				"master conjurer",		/*  -9 */
    "apprentice mage",		"mage", 				"experienced mage",		/* -12 */
    "master mage",			"apprentice warlord", 	"novice warlord",		/* -15 */
    "expert warlord",		"master warlord", 		"apprentice gorgon",	/* -18 */
    "gorgon", 				"practiced gorgon", 	"master gorgon",		/* -21 */
    "demi-gorgon", 			"evil master", 			"great evil master",	/* -24 */
    aa1, aa1, aa1, /* -27*/
    aa1, aa1, aa1, /* -30*/
    aa1, aa1, aa1, /* -33*/
    aa1, aa1, aa1, /* -36*/
    aa1, aa1, aa1, /* -39*/
    aa2, aa2, aa2, /* -42*/
    aa2, aa2, aa2, /* -45*/
    aa2, aa2, aa2, /* -48*/
    aa3, aa3, aa3, /* -51*/
    aa3, aa3, aa3, /* -54*/
    aa3, aa3, aa3, /* -57*/
    aa4, aa4, aa4, /* -60*/
    aa4, aa4, aa4, /* -63*/
    aa4, aa4, aa4, /* -66*/
    aa5, aa5, aa5, /* -69*/
    aa5, aa5, aa5, /* -72*/
    aa5, aa5, aa5, /* -75*/
    aa6, aa6, aa6, /* -78*/
    aa6, aa6, aa6, /* -81*/
    aa6, aa6, aa6, /* -84*/
    aa7, aa7, aa7, /* -87*/
    aa8, aa8, aa8, /* -90*/
    aa8, aa8, aa8, /* -93*/
    "earth guardian", "air guardian",	"fire guardian",	/* -96*/
    "water guardian", "time guardian",	"ethereal guardian",/* -99*/
    "The Creator", /* 100*/
};

/*
    table of experience needed to be a certain level of player
    FIXME: this is crap. Use a formula instead an get rid of this...
 */
static const guint32 player_lvl_exp[] =
{
    0, 10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120,                                        /*  1-11 */
    10240, 20480, 40960, 100000, 200000, 400000, 700000, 1000000,                              /* 12-19 */
    2000000, 3000000, 4000000, 5000000, 6000000, 8000000, 10000000,                            /* 20-26 */
    12000000, 14000000, 16000000, 18000000, 20000000, 22000000, 24000000, 26000000, 28000000,  /* 27-35 */
    30000000, 32000000, 34000000, 36000000, 38000000, 40000000, 42000000, 44000000, 46000000,  /* 36-44 */
    48000000, 50000000, 52000000, 54000000, 56000000, 58000000, 60000000, 62000000, 64000000,  /* 45-53 */
    66000000, 68000000, 70000000, 72000000, 74000000, 76000000, 78000000, 80000000, 82000000,  /* 54-62 */
    84000000, 86000000, 88000000, 90000000, 92000000, 94000000, 96000000, 98000000, 100000000, /* 63-71 */
    105000000, 110000000, 115000000, 120000000, 125000000, 130000000, 135000000, 140000000,    /* 72-79 */
    145000000, 150000000, 155000000, 160000000, 165000000, 170000000, 175000000, 180000000,    /* 80-87 */
    185000000, 190000000, 195000000, 200000000, 210000000, 220000000, 230000000, 240000000,    /* 88-95 */
    250000000, 260000000, 270000000, 280000000, 290000000, 300000000                           /* 96-101*/
};

/* a little helper: will be set by player_autopickup() to prevent
   player_item_pickup() asking how many items of a kind shall be picked up */

static gboolean called_by_autopickup = FALSE;

/* function declarations */

static void player_calculate_octant(player *p, int row, float start, float end, int radius, int xx, int xy, int yx, int yy);
static void player_sobject_memorize(player *p, map_sobject_t sobject, position pos);
static void player_sobject_forget(player *p, position pos);
static int player_sobjects_sort(gconstpointer a, gconstpointer b);
static cJSON *player_memory_serialize(player *p, position pos);
static void player_memory_deserialize(player *p, position pos, cJSON *mser);
static char *player_death_description(game_score_t *score, int verbose);

player *player_new()
{
    player *p;
    item *it;

    /* initialize player */
    p = g_malloc0(sizeof(player));

    p->stats.str_orig = p->strength = 12 + rand_0n(6);
    p->stats.cha_orig = p->charisma = 12 + rand_0n(6);
    p->stats.dex_orig = p->dexterity = 12 + rand_0n(6);
    p->stats.wis_orig = p->wisdom = 12 + rand_0n(6);
    p->stats.int_orig = p->intelligence = 12 + rand_0n(6);
    p->stats.con_orig = p->constitution = 12 + rand_0n(6);

    p->hp = p->hp_max = (p->constitution + rand_0n(10));
    p->mp = p->mp_max = (p->intelligence + rand_0n(10));

    p->level = p->stats.max_level = 1;

    /* set the player's default speed */
    p->speed = SPEED_NORMAL;

    p->known_spells = g_ptr_array_new();
    p->effects = g_ptr_array_new();
    p->inventory = inv_new(p);

    inv_callbacks_set(p->inventory, &player_inv_pre_add, &player_inv_weight_recalc,
                      NULL, &player_inv_weight_recalc);

    it = item_new(IT_ARMOUR, AT_LEATHER);
    it->bonus = 1;
    player_item_identify(p, NULL, it);
    inv_add(&p->inventory, it);
    player_item_equip(p, NULL, it);

    it = item_new(IT_WEAPON, WT_DAGGER);
    player_item_identify(p, NULL, it);
    inv_add(&p->inventory, it);
    player_item_equip(p, NULL, it);

    /* start a new diary */
    p->log = log_new();

    /* potion of cure dianthroritis is always known */
    p->identified_potions[PO_CURE_DIANTHR] = TRUE;

    return p;
}

void player_destroy(player *p)
{
    guint idx;

    assert(p != NULL);

    /* release spells */
    while ((p->known_spells)->len > 0)
        spell_destroy(g_ptr_array_remove_index_fast(p->known_spells,
                      (p->known_spells)->len - 1));

    g_ptr_array_free(p->known_spells, TRUE);

    /* release effects */
    for (idx = 0; idx < p->effects->len; idx++)
    {
        gpointer effect_id = g_ptr_array_index(p->effects, idx);
        effect *e = game_effect_get(nlarn, effect_id);

        if (e->item == NULL)
        {
            effect_destroy(e);
        }
    }

    g_ptr_array_free(p->effects, TRUE);

    /* free memorized stationary objects */
    if (p->sobjmem != NULL)
    {
        g_array_free(p->sobjmem, TRUE);
    }

    inv_destroy(p->inventory, FALSE);
    log_destroy(p->log);

    if (p->name)
    {
        g_free(p->name);
    }

    g_free(p);
}

cJSON *player_serialize(player *p)
{
    cJSON *obj;
    cJSON *pser = cJSON_CreateObject();

    cJSON_AddStringToObject(pser, "name", p->name);
    cJSON_AddNumberToObject(pser, "sex", p->sex);

    cJSON_AddNumberToObject(pser, "strength", p->strength);
    cJSON_AddNumberToObject(pser, "intelligence", p->intelligence);
    cJSON_AddNumberToObject(pser, "wisdom", p->wisdom);
    cJSON_AddNumberToObject(pser, "constitution", p->constitution);
    cJSON_AddNumberToObject(pser, "dexterity", p->dexterity);
    cJSON_AddNumberToObject(pser, "charisma", p->charisma);

    cJSON_AddNumberToObject(pser, "hp", p->hp);
    cJSON_AddNumberToObject(pser, "hp_max", p->hp_max);
    cJSON_AddNumberToObject(pser, "mp", p->mp);
    cJSON_AddNumberToObject(pser, "mp_max", p->mp_max);
    cJSON_AddNumberToObject(pser, "regen_counter", p->regen_counter);

    cJSON_AddNumberToObject(pser, "bank_account", p->bank_account);
    cJSON_AddNumberToObject(pser, "outstanding_taxes", p->outstanding_taxes);
    cJSON_AddNumberToObject(pser, "interest_lasttime", p->interest_lasttime);

    cJSON_AddNumberToObject(pser, "experience", p->experience);
    cJSON_AddNumberToObject(pser, "level", p->level);

    cJSON_AddNumberToObject(pser, "speed", p->speed);
    cJSON_AddNumberToObject(pser, "movement", p->movement);

    /* known spells */
    if (p->known_spells->len > 0)
    {
        cJSON_AddItemToObject(pser, "known_spells",
                              spells_serialize(p->known_spells));
    }
    /* inventory */
    if (inv_length(p->inventory) > 0)
    {
        cJSON_AddItemToObject(pser, "inventory", inv_serialize(p->inventory));
    }

    /* effects */
    if (p->effects->len > 0)
    {
        cJSON_AddItemToObject(pser, "effects", effects_serialize(p->effects));
    }

    /* equipped items */
    if (p->eq_amulet)
        cJSON_AddNumberToObject(pser, "eq_amulet",
                                GPOINTER_TO_UINT(p->eq_amulet->oid));

    if (p->eq_weapon)
        cJSON_AddNumberToObject(pser, "eq_weapon",
                                GPOINTER_TO_UINT(p->eq_weapon->oid));

    if (p->eq_boots)
        cJSON_AddNumberToObject(pser, "eq_boots",
                                GPOINTER_TO_UINT(p->eq_boots->oid));

    if (p->eq_cloak)
        cJSON_AddNumberToObject(pser, "eq_cloak",
                                GPOINTER_TO_UINT(p->eq_cloak->oid));

    if (p->eq_gloves)
        cJSON_AddNumberToObject(pser, "eq_gloves",
                                GPOINTER_TO_UINT(p->eq_gloves->oid));

    if (p->eq_helmet)
        cJSON_AddNumberToObject(pser, "eq_helmet",
                                GPOINTER_TO_UINT(p->eq_helmet->oid));

    if (p->eq_shield)
        cJSON_AddNumberToObject(pser, "eq_shield",
                                GPOINTER_TO_UINT(p->eq_shield->oid));

    if (p->eq_suit)
        cJSON_AddNumberToObject(pser, "eq_suit",
                                GPOINTER_TO_UINT(p->eq_suit->oid));

    if (p->eq_ring_l)
        cJSON_AddNumberToObject(pser, "eq_ring_l",
                                GPOINTER_TO_UINT(p->eq_ring_l->oid));

    if (p->eq_ring_r)
        cJSON_AddNumberToObject(pser, "eq_ring_r",
                                GPOINTER_TO_UINT(p->eq_ring_r->oid));

    /* identified items */
    cJSON_AddItemToObject(pser, "identified_amulets",
                          cJSON_CreateIntArray(p->identified_amulets, AM_MAX));

    cJSON_AddItemToObject(pser, "identified_books",
                          cJSON_CreateIntArray(p->identified_books, SP_MAX));

    cJSON_AddItemToObject(pser, "identified_potions",
                          cJSON_CreateIntArray(p->identified_potions, PO_MAX));

    cJSON_AddItemToObject(pser, "identified_rings",
                          cJSON_CreateIntArray(p->identified_rings, RT_MAX));

    cJSON_AddItemToObject(pser, "identified_scrolls",
                          cJSON_CreateIntArray(p->identified_scrolls, ST_MAX));

    cJSON_AddItemToObject(pser, "courses_taken",
                          cJSON_CreateIntArray(p->school_courses_taken, SCHOOL_COURSE_COUNT));

    cJSON_AddItemToObject(pser, "position", pos_serialize(p->pos));

    position pos;

    /* store players' memory of the map */
    cJSON_AddItemToObject(pser, "memory", obj = cJSON_CreateArray());

    for (pos.z = 0; pos.z < MAP_MAX; pos.z++)
    {
        cJSON *mm = cJSON_CreateArray();

        for (pos.y = 0; pos.y < MAP_MAX_Y; pos.y++)
            for (pos.x = 0; pos.x < MAP_MAX_X; pos.x++)
                cJSON_AddItemToArray(mm, player_memory_serialize(p, pos));

        cJSON_AddItemToArray(obj, mm);
    }

    /* store remembered stationary objects */
    if (p->sobjmem != NULL)
    {
        int idx;

        obj = cJSON_CreateArray();
        cJSON_AddItemToObject(pser, "sobjmem", obj);

        for (idx = 0; idx < p->sobjmem->len; idx++)
        {
            player_sobject_memory *som;
            cJSON *soms = cJSON_CreateObject();

            som = &g_array_index(p->sobjmem, player_sobject_memory, idx);

            cJSON_AddItemToObject(soms, "pos", pos_serialize(som->pos));
            cJSON_AddNumberToObject(soms, "sobject", som->sobject);

            cJSON_AddItemToArray(obj, soms);
        }
    }

    /* log */
    cJSON_AddItemToObject(pser, "log", log_serialize(p->log));

    /* statistics */
    cJSON_AddItemToObject(pser, "stats", obj = cJSON_CreateObject());

    cJSON_AddNumberToObject(obj, "deepest_level", p->stats.deepest_level);
    cJSON_AddItemToObject(obj, "monsters_killed",
                          cJSON_CreateIntArray(p->stats.monsters_killed, MT_MAX));

    cJSON_AddNumberToObject(obj, "spells_cast", p->stats.spells_cast);
    cJSON_AddNumberToObject(obj, "potions_quaffed", p->stats.potions_quaffed);
    cJSON_AddNumberToObject(obj, "scrolls_read", p->stats.scrolls_read);
    cJSON_AddNumberToObject(obj, "books_read", p->stats.books_read);
    cJSON_AddNumberToObject(obj, "cookies_nibbled", p->stats.cookies_nibbled);
    cJSON_AddNumberToObject(obj, "max_level", p->stats.max_level);
    cJSON_AddNumberToObject(obj, "max_xp", p->stats.max_xp);

    cJSON_AddNumberToObject(obj, "str_orig", p->stats.str_orig);
    cJSON_AddNumberToObject(obj, "int_orig", p->stats.int_orig);
    cJSON_AddNumberToObject(obj, "wis_orig", p->stats.wis_orig);
    cJSON_AddNumberToObject(obj, "con_orig", p->stats.con_orig);
    cJSON_AddNumberToObject(obj, "dex_orig", p->stats.dex_orig);
    cJSON_AddNumberToObject(obj, "cha_orig", p->stats.cha_orig);

    return pser;
}

player *player_deserialize(cJSON *pser)
{
    player *p;
    int idx;
    cJSON *obj, *elem;

    p = g_malloc0(sizeof(player));

    p->name = g_strdup(cJSON_GetObjectItem(pser, "name")->valuestring);
    p->sex = cJSON_GetObjectItem(pser, "sex")->valueint;

    p->strength = cJSON_GetObjectItem(pser, "strength")->valueint;
    p->intelligence = cJSON_GetObjectItem(pser, "intelligence")->valueint;
    p->wisdom = cJSON_GetObjectItem(pser, "wisdom")->valueint;
    p->constitution = cJSON_GetObjectItem(pser, "constitution")->valueint;
    p->dexterity = cJSON_GetObjectItem(pser, "dexterity")->valueint;
    p->charisma = cJSON_GetObjectItem(pser, "charisma")->valueint;

    p->hp = cJSON_GetObjectItem(pser, "hp")->valueint;
    p->hp_max = cJSON_GetObjectItem(pser, "hp_max")->valueint;
    p->mp = cJSON_GetObjectItem(pser, "mp")->valueint;
    p->mp_max = cJSON_GetObjectItem(pser, "mp_max")->valueint;
    p->regen_counter = cJSON_GetObjectItem(pser, "regen_counter")->valueint;

    p->bank_account = cJSON_GetObjectItem(pser, "bank_account")->valueint;
    p->outstanding_taxes = cJSON_GetObjectItem(pser, "outstanding_taxes")->valueint;
    p->interest_lasttime = cJSON_GetObjectItem(pser, "interest_lasttime")->valueint;

    p->experience = cJSON_GetObjectItem(pser, "experience")->valueint;
    p->level = cJSON_GetObjectItem(pser, "level")->valueint;

    p->speed = cJSON_GetObjectItem(pser, "speed")->valueint;
    p->movement = cJSON_GetObjectItem(pser, "movement")->valueint;

    /* known spells */
    obj = cJSON_GetObjectItem(pser, "known_spells");
    if (obj != NULL)
        p->known_spells = spells_deserialize(obj);
    else
        p->known_spells = g_ptr_array_new();


    /* inventory */
    obj = cJSON_GetObjectItem(pser, "inventory");
    if (obj != NULL)
        p->inventory = inv_deserialize(obj);
    else
        p->inventory = inv_new(p);

    p->inventory->owner = p;
    inv_callbacks_set(p->inventory, &player_inv_pre_add, &player_inv_weight_recalc,
                      NULL, &player_inv_weight_recalc);


    /* effects */
    obj = cJSON_GetObjectItem(pser, "effects");
    if (obj != NULL)
        p->effects = effects_deserialize(obj);
    else
        p->effects = g_ptr_array_new();

    /* equipped items */
    obj = cJSON_GetObjectItem(pser, "eq_amulet");
    if (obj != NULL) p->eq_amulet = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_weapon");
    if (obj != NULL) p->eq_weapon = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_boots");
    if (obj != NULL) p->eq_boots = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_cloak");
    if (obj != NULL) p->eq_cloak = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_gloves");
    if (obj != NULL) p->eq_gloves = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_helmet");
    if (obj != NULL) p->eq_helmet = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_shield");
    if (obj != NULL) p->eq_shield = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_suit");
    if (obj != NULL) p->eq_suit = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_ring_l");
    if (obj != NULL) p->eq_ring_l = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    obj = cJSON_GetObjectItem(pser, "eq_ring_r");
    if (obj != NULL) p->eq_ring_r = game_item_get(nlarn, GUINT_TO_POINTER(obj->valueint));

    /* identified items */
    obj = cJSON_GetObjectItem(pser, "identified_amulets");
    for (idx = 0; idx < AM_MAX; idx++)
        p->identified_amulets[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    obj = cJSON_GetObjectItem(pser, "identified_books");
    for (idx = 0; idx < SP_MAX; idx++)
        p->identified_books[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    obj = cJSON_GetObjectItem(pser, "identified_potions");
    for (idx = 0; idx < PO_MAX; idx++)
        p->identified_potions[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    obj = cJSON_GetObjectItem(pser, "identified_rings");
    for (idx = 0; idx < RT_MAX; idx++)
        p->identified_rings[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    obj = cJSON_GetObjectItem(pser, "identified_scrolls");
    for (idx = 0; idx < ST_MAX; idx++)
        p->identified_scrolls[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    obj = cJSON_GetObjectItem(pser, "courses_taken");
    for (idx = 0; idx < SCHOOL_COURSE_COUNT; idx++)
        p->school_courses_taken[idx] = cJSON_GetArrayItem(obj, idx)->valueint;

    p->pos = pos_deserialize(cJSON_GetObjectItem(pser, "position"));


    /* restore players' memory of the map */
    position pos;
    obj = cJSON_GetObjectItem(pser, "memory");

    for (pos.z = 0; pos.z < MAP_MAX; pos.z++)
    {
        elem = cJSON_GetArrayItem(obj, pos.z);

        for (pos.y = 0; pos.y < MAP_MAX_Y; pos.y++)
        {
            for (pos.x = 0; pos.x < MAP_MAX_X; pos.x++)
            {
                cJSON *tile = cJSON_GetArrayItem(elem, pos.x + (MAP_MAX_X * pos.y));
                player_memory_deserialize(p, pos, tile);
            }
        }
    }

    /* remembered stationary objects */
    obj = cJSON_GetObjectItem(pser, "sobjmem");
    if (obj != NULL)
    {
        int idx;
        int count = cJSON_GetArraySize(obj);

        p->sobjmem = g_array_sized_new(FALSE, FALSE, sizeof(player_sobject_memory), count);

        for (idx = 0; idx < count; idx++)
        {
            player_sobject_memory som;
            cJSON *soms = cJSON_GetArrayItem(obj, idx);

            som.pos = pos_deserialize(cJSON_GetObjectItem(soms, "pos"));
            som.sobject = cJSON_GetObjectItem(soms, "sobject")->valueint;

            g_array_append_val(p->sobjmem, som);
        }
    }

    /* log */
    p->log = log_deserialize(cJSON_GetObjectItem(pser, "log"));

    /* statistics */
    obj = cJSON_GetObjectItem(pser, "stats");

    p->stats.deepest_level = cJSON_GetObjectItem(obj, "deepest_level")->valueint;

    elem = cJSON_GetObjectItem(obj, "monsters_killed");
    for (idx = 0; idx < MT_MAX; idx++)
        p->stats.monsters_killed[idx] = cJSON_GetArrayItem(elem, idx)->valueint;

    p->stats.spells_cast = cJSON_GetObjectItem(obj, "spells_cast")->valueint;
    p->stats.potions_quaffed = cJSON_GetObjectItem(obj, "potions_quaffed")->valueint;
    p->stats.scrolls_read = cJSON_GetObjectItem(obj, "scrolls_read")->valueint;
    p->stats.books_read = cJSON_GetObjectItem(obj, "books_read")->valueint;
    p->stats.cookies_nibbled = cJSON_GetObjectItem(obj, "cookies_nibbled")->valueint;
    p->stats.max_level = cJSON_GetObjectItem(obj, "max_level")->valueint;
    p->stats.max_xp = cJSON_GetObjectItem(obj, "max_xp")->valueint;

    p->stats.str_orig = cJSON_GetObjectItem(obj, "str_orig")->valueint;
    p->stats.int_orig = cJSON_GetObjectItem(obj, "int_orig")->valueint;
    p->stats.wis_orig = cJSON_GetObjectItem(obj, "wis_orig")->valueint;
    p->stats.con_orig = cJSON_GetObjectItem(obj, "con_orig")->valueint;
    p->stats.dex_orig = cJSON_GetObjectItem(obj, "dex_orig")->valueint;
    p->stats.cha_orig = cJSON_GetObjectItem(obj, "cha_orig")->valueint;

    return p;
}

void player_make_move(player *p, int turns)
{
    int frequency; /* number of turns between occasions */
    int regen = 0; /* amount of regeneration */
    effect *e; /* temporary var for effect */
    int idx = 0;

    assert(p != NULL);

    /* modifier for frequency */
    frequency = game_difficulty(nlarn) << 3;

    do
    {
        /* if the number of movement points exceeds 100 reduce number
           of turns and handle regeneration and some effects */
        if (p->movement >= SPEED_NORMAL)
        {
            /* reduce the player's movement points */
            p->movement -= SPEED_NORMAL;
        }

        /* player's extra moves have expired - finish a turn */
        if (p->movement < SPEED_NORMAL)
        {
            /* check for time stop */
            if ((e = player_effect_get(p, ET_TIMESTOP)))
            {
                /* time has been stopped - handle player's movement locally */
                p->movement += player_get_speed(p);

                /* expire only time stop */
                if (effect_expire(e) == -1)
                {
                    /* time stop has expired - remove it*/
                    player_effect_del(p, e);
                }
                else
                {
                    /* nothing else happens while the time is stopped */
                    turns--;
                    continue;
                }
            }

            /* move the rest of the world */
            game_spin_the_wheel(nlarn);

            /* expire temporary effects */
            idx = 0; // reset idx for proper expiration during multiturn events
            while (idx < p->effects->len)
            {
                gpointer effect_id = g_ptr_array_index(p->effects, idx);
                e = game_effect_get(nlarn, effect_id);

                if (effect_expire(e) == -1)
                {
                    /* effect has expired */
                    player_effect_del(p, e);
                }
                else
                {
                    idx++;
                }
            }

            /* handle regeneration */
            if (p->regen_counter == 0)
            {
                p->regen_counter = 22 + frequency;

                if (p->hp < player_get_hp_max(p))
                {
                    regen = 1
                            + player_effect(p, ET_INC_HP_REGEN)
                            - player_effect(p, ET_DEC_HP_REGEN);

                    player_hp_gain(p, regen);
                }

                if (p->mp < player_get_mp_max(p))
                {
                    regen = 1
                            + player_effect(p, ET_INC_MP_REGEN)
                            - player_effect(p, ET_DEC_MP_REGEN);

                    player_mp_gain(p, regen);
                }
            }
            else
            {
                p->regen_counter--;
            }

            /* handle poison */
            if ((e = player_effect_get(p, ET_POISON)))
            {
                if ((game_turn(nlarn) - e->start) % (22 - frequency) == 0)
                {
                    damage *dam = damage_new(DAM_POISON, ATT_NONE, e->amount, NULL);

                    player_damage_take(p, dam, PD_EFFECT, e->type);
                }
            }

            /* handle clumsiness */
            if ((e = player_effect_get(p, ET_CLUMSINESS)))
            {
                if (chance(33) && p->eq_weapon)
                {
                    item *it = p->eq_weapon;

                    log_disable(p->log);
                    player_item_unequip(p, NULL, it);
                    log_enable(p->log);

                    log_add_entry(p->log, effect_get_msg_start(e));
                    player_item_drop(p, &p->inventory, it);
                }
            }

            /* handle itching */
            if ((e = player_effect_get(p, ET_ITCHING)))
            {
                item **it;

                if (chance(50) && (it = player_get_random_armour(p)))
                {
                    /* take off armour */
                    it = NULL;

                    log_add_entry(p->log, effect_get_msg_start(e));
                    player_item_drop(p, &p->inventory, *it);
                }
            }
        }

        /* reduce number of turns */
        turns--;
    }
    while (turns > 0);
}

void player_die(player *p, player_cod cause_type, int cause)
{
    GString *text;
    game_score_t *score, *cscore;
    GList *scores, *iterator;
    char *message = NULL;
    char *title = NULL;
    char *tmp = NULL;
    char it_desc[61] = { 0 };
    int count, pos;
    effect *ef = NULL;
    char *pronoun = (p->sex == PS_MALE) ? "He" : "She";

    assert(p != NULL);

    /* check for life protection */
    if ((cause_type < PD_STUCK) && (ef = player_effect_get(p, ET_LIFE_PROTECTION)))
    {
        log_add_entry(p->log, "You feel wiiieeeeerrrrrd all over!");

        if (ef->amount > 1)
        {
            ef->amount--;
        }
        else
        {
            player_effect_del(p, ef);
        }

        if (cause_type == PD_LASTLEVEL)
        {
            /* revert effects of drain level */
            player_level_gain(p, 1);
        }

        p->hp = p->hp_max;

        return;
    }

    switch (cause_type)
    {
    case PD_LASTLEVEL:
        message = "You fade to gray...";
        title = "In Memoriam";
        break;

    case PD_STUCK:
        message = "You are trapped in solid rock.";
        title = "Ouch!";
        break;

    case PD_TOO_LATE:
        message = "You returned home too late!";
        title = "The End";
        break;

    case PD_WON:
        message = "You saved your daughter!";
        title = "Congratulations! You won!";
        break;

    case PD_LOST:
        message = "You didn't manage to save your daughter.";
        title = "You lost";
        break;

    case PD_QUIT:
        message = "You quit.";
        title = "The End";
        break;

    default:
        message = "You die...";
        title = "R. I. P.";
    }

    log_add_entry(p->log, message);

    /* resume game if wizard mode is enabled */
    if (game_wizardmode(nlarn) && (cause_type < PD_TOO_LATE))
    {
        log_add_entry(p->log, "WIZARD MODE. You stay alive.");

        /* return to full power */
        p->hp = p->hp_max;
        p->mp = p->mp_max;

        /* return to level 1 if last level has been lost */
        if (p->level < 1) p->level = 1;

        /* clear some nasty effects */
        effect *e;
        if ((e = player_effect_get(p, ET_PARALYSIS)))
            player_effect_del(p, e);
        if ((e = player_effect_get(p, ET_CONFUSION)))
            player_effect_del(p, e);
        if ((e = player_effect_get(p, ET_BLINDNESS)))
            player_effect_del(p, e);
        if ((e = player_effect_get(p, ET_POISON)))
            player_effect_del(p, e);

        if (player_get_str(p) <= 0)
        {
            if ((e = player_effect_get(p, ET_DEC_STR)))
                player_effect_del(p, e);
        }
        if (player_get_dex(p) <= 0)
        {
            if ((e = player_effect_get(p, ET_DEC_DEX)))
                player_effect_del(p, e);
        }

        /* return to the game */
        return;
    }

    /* do not show scores when in wizardmode */
    if (!game_wizardmode(nlarn))
    {
        /* redraw screen to make sure player can see the cause of his death */
        display_paint_screen(p);

        /* sleep a second */
        usleep(1000000);

        /* flush keyboard input buffer */
        flushinp();

        score = game_score(nlarn, cause_type, cause);
        scores = game_score_add(nlarn, score);

        tmp = player_death_description(score, TRUE);
        text = g_string_new(tmp);
        g_free(tmp);

        /* determine position of score in the score list */
        pos = g_list_index(scores, score);

        /* assemble surrounding scores list */
        g_string_append(text, "\n\n");

        /* get entry three entries up of current/top score in list */
        iterator = g_list_nth(scores, max(pos - 3, 0));

        /* display up to 7 entries */
        for (count = max(pos - 3, 0); iterator && (count < (max(pos, 0) + 4));
                iterator = iterator->next, count++)
        {
            gchar *desc;

            cscore = (game_score_t *)iterator->data;

            desc = player_death_description(cscore, FALSE);
            g_string_append_printf(text, "  %s%2d) %7" G_GINT64_FORMAT " %s [lvl. %d, %d hp]\n",
                                   (cscore == score) ? "*" : " ",
                                   count + 1, cscore->score, desc,
                                   cscore->dlevel, cscore->hp);

            g_free(desc);
        }

        game_scores_destroy(scores);

        /* some statistics */
        g_string_append_printf(text, "\n%s %s after searching the potion for %d mobul%s. ",
                               pronoun, cause_type < PD_TOO_LATE ? "died" : "returned",
                               gtime2mobuls(nlarn->gtime), plural(gtime2mobuls(nlarn->gtime)));

        g_string_append_printf(text, "%s cast %s spell%s, ", pronoun,
                               int2str(p->stats.spells_cast),
                               plural(p->stats.spells_cast));
        g_string_append_printf(text, "quaffed %s potion%s, ",
                               int2str(p->stats.potions_quaffed),
                               plural(p->stats.potions_quaffed));
        g_string_append_printf(text, "nibbled %s cookie%s ",
                               int2str(p->stats.cookies_nibbled),
                               plural(p->stats.cookies_nibbled));
        g_string_append_printf(text, "and read %s book%s ",
                               int2str(p->stats.books_read),
                               plural(p->stats.books_read));
        g_string_append_printf(text, "and %s scroll%s. ",
                               int2str(p->stats.scrolls_read),
                               plural(p->stats.scrolls_read));

        if (p->bank_account > 0)
        {
            g_string_append_printf(text, "%s had %s gp on the bank account.",
                                   pronoun, int2str(p->bank_account));
        }

        /* append map of current level if the player is not in the town */
        if (p->pos.z > 0)
        {
            g_string_append(text, "\n\n-- The current level ------------------\n\n");
            tmp = map_dump(game_map(nlarn, p->pos.z), p->pos);
            g_string_append(text, tmp);
            g_free(tmp);
        }

        /* player's attributes */
        g_string_append(text, "\n\n-- Attributes -------------------------\n\n");
        g_string_append_printf(text, "Strength:     %d (%+2d)\n",
                               p->strength, p->strength - p->stats.str_orig);
        g_string_append_printf(text, "Intelligence: %d (%+2d)\n",
                               p->intelligence, p->intelligence - p->stats.int_orig);
        g_string_append_printf(text, "Wisdom:       %d (%+2d)\n",
                               p->wisdom, p->wisdom - p->stats.wis_orig);
        g_string_append_printf(text, "Constitution: %d (%+2d)\n",
                               p->constitution, p->constitution - p->stats.con_orig);
        g_string_append_printf(text, "Dexterity:    %d (%+2d)\n",
                               p->dexterity, p->dexterity - p->stats.dex_orig);
        g_string_append_printf(text, "Charisma:     %d (%+2d)\n",
                               p->charisma, p->charisma - p->stats.cha_orig);

        /* effects */
        char **effect_desc = player_effect_text(p);

        if (*effect_desc)
        {
            g_string_append(text, "\n\n-- Effects ----------------------------\n\n");

            for (pos = 0; effect_desc[pos]; pos++)
            {
                g_string_append_printf(text, "%s\n", effect_desc[pos]);
            }
        }

        g_strfreev(effect_desc);

        /* append list of known spells */
        if (p->known_spells->len > 0)
        {
            g_string_append(text, "\n\n-- Known Spells -----------------------\n\n");

            for (pos = 0; pos < p->known_spells->len; pos++)
            {
                spell *s = (spell *)g_ptr_array_index(p->known_spells, pos);
                tmp = str_capitalize(g_strdup(spell_name(s)));

                g_string_append_printf(text, "%-24s (lvl. %2d): %3d\n",
                                       tmp, s->knowledge, s->used);

                g_free(tmp);
            }
        }

        /* list monsters killed */
        guint mnum, body_count = 0;
        g_string_append(text, "\n\n-- Creatures vanquished ---------------\n\n");

        for (mnum = MT_NONE + 1; mnum < MT_MAX; mnum++)
        {
            if (p->stats.monsters_killed[mnum] > 0)
            {
                const int count = p->stats.monsters_killed[mnum];
                tmp = str_capitalize(g_strdup(monster_type_plural_name(mnum,
                                              count)));

                g_string_append_printf(text, "%3d %s\n",
                                       p->stats.monsters_killed[mnum], tmp);

                g_free(tmp);
                body_count += p->stats.monsters_killed[mnum];
            }
        }
        g_string_append_printf(text, "\n%3d total\n\n", body_count);

        /* genocided monsters */
        for (pos = 1, count = 0; pos < MT_MAX; pos++)
        {
            if (monster_is_genocided(pos)) count++;
        }

        g_string_append_printf(text, "%s genocided %s monsters.\n",
                               pronoun, int2str(count));


        /* identify entire inventory */
        for (pos = 0; pos < inv_length(p->inventory); pos++)
        {
            item *it = inv_get(p->inventory, pos);
            it->blessed_known = TRUE;
            it->bonus_known = TRUE;
        }

        /* equipped items */
        int equipment_count = 0;
        g_string_append(text, "\n\n-- Equipment --------------------------\n\n");
        if (p->eq_amulet)
        {
            item_describe(p->eq_amulet, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Amulet:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_ring_l)
        {
            item_describe(p->eq_ring_l, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Ring (l): %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_ring_r)
        {
            item_describe(p->eq_ring_r, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Ring (r): %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_weapon)
        {
            item_describe(p->eq_weapon, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Weapon:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_suit)
        {
            item_describe(p->eq_suit, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Armour:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_helmet)
        {
            item_describe(p->eq_helmet, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Helmet:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_shield)
        {
            item_describe(p->eq_shield, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Shield:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_cloak)
        {
            item_describe(p->eq_cloak, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Cloak:    %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_gloves)
        {
            item_describe(p->eq_gloves, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Gloves:   %s\n", it_desc);
            equipment_count++;
        }
        if (p->eq_boots)
        {
            item_describe(p->eq_boots, TRUE, TRUE, FALSE, it_desc, 60);
            g_string_append_printf(text, "Boots:    %s\n", it_desc);
            equipment_count++;
        }

        /* inventory */
        if (equipment_count < inv_length(p->inventory))
        {
            g_string_append(text, "\n\n-- Items in pack ----------------------\n\n");
            for (pos = 0; pos < inv_length(p->inventory); pos++)
            {
                item *it = inv_get(p->inventory, pos);
                if (!player_item_is_equipped(p, it))
                {
                    item_describe(it, TRUE, it->count == 1, FALSE, it_desc, 60);
                    g_string_append_printf(text, "%s\n", it_desc);

                }
            }
        }

        /* messages */
        g_string_append(text, "\n\n-- Last messages ----------------------\n\n");
        count = min(10, log_length(p->log));
        for (pos = log_length(p->log) - count; pos < log_length(p->log); pos++)
        {
            message_log_entry *entry = log_get_entry(p->log, pos);
            g_string_append_printf(text, "%s\n", entry->message);
        }
        /* print uncommited messages */
        if (p->log->buffer->len > 0)
        {
            g_string_append_printf(text, "%s\n", p->log->buffer->str);
        }

        display_show_message(title, text->str, 0);

        if (display_get_yesno("Do you want to save a memorial " \
                              "file for your character?", NULL, NULL))
        {
            char *filename, *proposal;
            GError *error = NULL;

            /* repaint screen onece again */
            display_paint_screen(p);

            proposal = g_strconcat(p->name, ".txt", NULL);
            filename = display_get_string("Enter filename: ", proposal, 40);

            if (filename != NULL)
            {
                /* file name has been provided. try to save file */
                if (!g_file_set_contents(filename, text->str, -1, &error))
                {
                    display_show_message("Error", error->message, 0);
                    g_error_free(error);
                }

                g_free(proposal);
            }

            g_free(filename);
        }

        g_string_free(text, TRUE);
    }

    game_destroy(nlarn);

    exit(EXIT_SUCCESS);
}

gint64 player_calc_score(player *p, int won)
{
    gint64 score = 0;
    guint idx;

    assert (p != NULL);

    /* money */
    score = player_get_gold(p) + p->bank_account - p->outstanding_taxes;

    /* value of equipment */
    for (idx = 0; idx < inv_length(p->inventory); idx++)
    {
        score += item_price(inv_get(p->inventory, idx));
    }

    /* experience */
    score += p->experience;

    /* give points for remaining time if game has been won */
    if (won)
    {
        score += game_remaining_turns(nlarn) * (game_difficulty(nlarn) + 1);
    }

    return score;
}

gboolean player_movement_possible(player *p)
{
    /* no movement if overloaded */
    if (player_effect(p, ET_OVERSTRAINED))
    {
        log_add_entry(p->log, "You cannot move as long you are overstrained.");
        return FALSE;
    }

    /* no movement if paralyzed */
    if (player_effect(p, ET_PARALYSIS))
    {
        log_add_entry(p->log, "You can't move!");
        return FALSE;
    }

    return TRUE;
}

int player_move(player *p, direction dir, gboolean open_door)
{
    int times = 1;      /* how many time ticks it took */
    position target_p;  /* coordinates of target */
    map *map;           /* shortcut to player's current map */
    monster *target_m;  /* monster on target tile (if any) */
    map_sobject_t so;   /* stationary object on target tile (if any) */

    assert(p != NULL && dir > GD_NONE && dir < GD_MAX);

    if (!player_movement_possible(p))
        return 0;

    /* confusion: random movement */
    if (player_effect(p, ET_CONFUSION))
    {
        dir = rand_1n(GD_MAX);
    }

    /* determine target position of move */
    target_p = pos_move(p->pos, dir);

    /* exceeded map limits */
    if (!pos_valid(target_p))
        return FALSE;

    /* make a shortcut to the current map */
    map = game_map(nlarn, p->pos.z);

    /* impassable */
    if (!map_pos_passable(map, target_p))
    {
        /* bump into walls when blinded or confused */
        if (player_effect(p, ET_BLINDNESS) || player_effect(p, ET_CONFUSION))
        {
            player_memory_of(p, target_p).type = map_tiletype_at(map, target_p);
            log_add_entry(p->log, "Ouch! You bump into %s!",
                          lt_get_desc(map_tiletype_at(map, target_p)));
            return times;
        }

        if (open_door && map_sobject_at(map, target_p) == LS_CLOSEDDOOR)
            return player_door_open(nlarn->p, dir);

        /* if it is not a wall, it is definitely not passable */
        if (!map_tiletype_at(map, target_p) == LT_WALL)
            return FALSE;

        /* return if player cannot walk through walls */
        if (!player_effect(p, ET_WALL_WALK))
            return FALSE;
    }

    target_m = map_get_monster_at(map, target_p);

    if (target_m && monster_unknown(target_m))
    {
        /* reveal the mimic */
        log_add_entry(p->log, "Wait! That is a %s!", monster_get_name(target_m));
        monster_unknown_set(target_m, FALSE);
        return times;
    }

    /* attack - no movement */
    if (target_m)
    {
        return player_attack(p, target_m);
    }

    /* reposition player */
    p->pos = target_p;

    /* trigger the trap */
    if (map_trap_at(map, target_p))
    {
        times += player_trap_trigger(p, map_trap_at(map, target_p), FALSE);
    }

    /* auto-pickup */
    if (map_ilist_at(map, p->pos))
    {
        player_autopickup(p);
    }

    /* mention stationary objects at this position */
    if ((so =map_sobject_at(map, p->pos)))
    {
        log_add_entry(p->log, "You see %s here.", ls_get_desc(so));
    }

    return times;
}

int player_attack(player *p, monster *m)
{
    int prop;
    int amount;
    damage *dam;
    int roll;           /* the dice */
    effect *e;

    prop = p->level
           + player_get_dex(p)
           + (p->eq_weapon ? (weapon_wc(p->eq_weapon) / 4) : 0)
           + monster_ac(m) /* FIXME: I don't want those pointless D&D rules */
           - 12
           - game_difficulty(nlarn);

    roll = rand_1n(21);
    if ((roll <= prop) || (roll == 1))
    {
        /* placed a hit */
        log_add_entry(p->log, "You hit the %s.", monster_get_name(m));

        amount = player_get_str(p)
                 + player_get_wc(p)
                 - 12
                 - game_difficulty(nlarn);

        dam = damage_new(DAM_PHYSICAL, ATT_WEAPON, rand_1n(amount + 1), p);

        /* weapon damage due to rust when hitting certain monsters */
        if (p->eq_weapon && monster_is_metallivore(m))
        {
            p->eq_weapon = item_erode(&p->inventory, p->eq_weapon, IET_RUST, TRUE);
        }

        /* hitting a monster breaks stealth condition */
        if ((e = player_effect_get(p, ET_STEALTH)))
        {
            player_effect_del(p, e);
        }

        /* hitting a monster breaks hold monster spell */
        if ((e = monster_effect_get(m, ET_HOLD_MONSTER)))
        {
            monster_effect_del(m, e);
        }

        /* triple damage if hitting a dragon and wearing an amulet of dragon slaying */
        if (monster_is_dragon(m) && (p->eq_amulet && p->eq_amulet->id == AM_DRAGON_SLAYING))
        {
            dam->amount *= 3;
        }

        /* *** SPECIAL WEAPONS *** */
        if (p->eq_weapon)
        {
            /* Vorpal Blade */
            if ((p->eq_weapon->id == WT_VORPALBLADE)
                    && chance(5)
                    && monster_has_head(m)
                    && monster_is_beheadable(m))
            {
                log_add_entry(p->log, "You behead the %s with your Vorpal Blade!",
                              monster_get_name(m));

                dam->amount = monster_hp(m) + monster_ac(m);
            }

            /* Lance of Death */
            if ((p->eq_weapon->id == WT_LANCEOFDEATH))
            {
                /* the lance is pretty deadly for non-demons */
                if (!monster_is_demon(m))
                    dam->amount = 10000;
                else
                    dam->amount = 300;
            }

            /* Slayer */
            if (p->eq_weapon->id == WT_SLAYER)
            {
                if (monster_is_demon(m))
                    dam->amount = 10000;
            }
        }


        /* inflict damage */
        if (!(m = monster_damage_take(m, dam)))
        {
            /* killed the monster */
            return 1;
        }

        /* Lance of Death has not killed */
        if (p->eq_weapon && (p->eq_weapon->id == WT_LANCEOFDEATH))
        {
            log_add_entry(p->log, "Your lance of death tickles the %s!", monster_get_name(m));
        }

        /* if the player is invisible and the monster does not have infravision,
           remember the position where the attack came from
         */
        if (player_effect(p, ET_INVISIBILITY) && !monster_has_infravision(m))
        {
            monster_update_player_pos(m, p->pos);
        }
    }
    else if (monster_in_sight(m))
    {
        /* missed */
        log_add_entry(p->log, "You miss the %s.", monster_name(m));
    }

    return 1; /* i.e. turns used */
}

int player_map_enter(player *p, map *l, gboolean teleported)
{
    position pos;
    monster *m;

    assert(p != NULL && l != NULL);

    /* store the last turn player has been on this map */
    game_map(nlarn, p->pos.z)->visited = game_turn(nlarn);

    if (p->stats.deepest_level < l->nlevel)
    {
        p->stats.deepest_level = l->nlevel;
    }

    /* been teleported here or something like that, need a random spot */
    if (teleported)
        pos = map_find_space(l, LE_MONSTER, FALSE);

    /* beginning of the game */
    else if ((l->nlevel == 0) && (game_turn(nlarn) == 1))
        pos = map_find_sobject(l, LS_HOME);

    /* took the elevator down */
    else if ((p->pos.z == 0) && (l->nlevel == (MAP_DMAX)))
        pos = map_find_sobject(l, LS_ELEVATORUP);

    /* took the elevator up */
    else if ((p->pos.z == (MAP_DMAX)) && (l->nlevel == 0))
        pos = map_find_sobject(l, LS_ELEVATORDOWN);

    /* climbing up */
    else if (p->pos.z > l->nlevel)
    {
        if (l->nlevel == 0)
            pos = map_find_sobject(l, LS_DNGN_ENTRANCE);
        else
            pos = map_find_sobject(l, LS_STAIRSDOWN);
    }
    /* climbing down */
    else if (l->nlevel > p->pos.z)
    {
        if (l->nlevel == 1)
            pos = map_find_sobject(l, LS_DNGN_EXIT);
        else
            pos = map_find_sobject(l, LS_STAIRSUP);
    }

    if (l->nlevel == 0)
    {
        /* do not log this at the start of the game */
        if (p->log->gtime > 1)
            log_add_entry(p->log, "You return to town.");
    }
    else if (l->nlevel == 1 && p->pos.z == 0)
        log_add_entry(p->log, "You enter the caverns of Larn.");

    /* put player into new map */
    p->pos = pos;

    /* remove monster that might be at player's positon */
    if ((m = map_get_monster_at(l, p->pos)))
    {
        position mnpos =  map_find_space(l, LE_MONSTER, FALSE);
        monster_pos_set(m, game_map(nlarn, p->pos.z), mnpos);
    }

    /* recalculate FOV to make ensure correct display after entering a level */
    player_update_fov(p);

    /* call autopickup */
    player_autopickup(p);

    return TRUE;
}

item **player_get_random_armour(player *p)
{
    GPtrArray *armours;
    item **armour = NULL;

    assert (p != NULL);

    armours = g_ptr_array_new();

    /* add each equipped piece of armour to the pool to choose from */
    if (p->eq_boots)  g_ptr_array_add(armours, &p->eq_boots);
    if (p->eq_cloak)  g_ptr_array_add(armours, &p->eq_cloak);
    if (p->eq_gloves) g_ptr_array_add(armours, &p->eq_gloves);
    if (p->eq_helmet) g_ptr_array_add(armours, &p->eq_helmet);
    if (p->eq_shield) g_ptr_array_add(armours, &p->eq_shield);
    if (p->eq_suit)   g_ptr_array_add(armours, &p->eq_suit);

    if (armours->len > 0)
    {
        armour = g_ptr_array_index(armours, rand_0n(armours->len));
    }

    g_ptr_array_free(armours, TRUE);

    return armour;
}

int player_pickup(player *p)
{
    inventory **inv = NULL;
    int time = 0;

    GPtrArray *callbacks = NULL;
    display_inv_callback *callback = NULL;

    assert(p != NULL);

    inv = map_ilist_at(game_map(nlarn, p->pos.z), p->pos);

    if (inv_length(*inv) == 0)
    {
        log_add_entry(p->log, "There is nothing here.");
        return 0;
    }
    else if (inv_length(*inv) == 1)
    {
        return player_item_pickup(p, inv, inv_get(*inv, 0));
    }
    else
    {
        /* define callback functions */
        callbacks = g_ptr_array_new();

        callback = g_malloc0(sizeof(display_inv_callback));
        callback->description = "(g)et";
        callback->key = 'g';
        callback->inv = inv;
        callback->function = &player_item_pickup;
        g_ptr_array_add(callbacks, callback);

        display_inventory("On the floor", p, inv, callbacks, FALSE,
                          TRUE, FALSE, NULL);

        /* clean up callbacks */
        display_inv_callbacks_clean(callbacks);
    }

    return time;
}

void player_autopickup(player *p)
{
    guint idx;
    inventory **floor;

    assert (p != NULL && map_ilist_at(game_map(nlarn, p->pos.z), p->pos));

    floor = map_ilist_at(game_map(nlarn, p->pos.z), p->pos);

    /* set flag to tell player_item_pickup() not to ask about the amount */
    called_by_autopickup = TRUE;

    int other_items_count = 0;
    int other_item_id     = -1;
    gboolean did_pickup   = FALSE;
    for (idx = 0; idx < inv_length(*floor); idx++)
    {
        item *i = inv_get(*floor, idx);

        if (p->settings.auto_pickup[i->type])
        {
            /* try to pick up the item */
            if (player_item_pickup(p, floor, i))
            {
                /* item has been picked up */
                /* go back one item as the following items lowered their number */
                idx--;
                did_pickup = TRUE;
            }
        }
        else
        {
            if (++other_items_count == 1)
                other_item_id = idx;
        }
    }

    /* reset flag */
    called_by_autopickup = FALSE;

    if (did_pickup && other_items_count > 0)
    {
        if (other_items_count == 1)
        {
            item *it = inv_get(*floor, other_item_id);
            char item_desc[81] = { 0 };
            item_describe(it, player_item_known(nlarn->p, it),
                          (it->count == 1), FALSE, item_desc, 80);

            log_add_entry(p->log, "There %s %s here.",
                          it->count == 1 ? "is" : "are", item_desc);
        }
        else
        {
            log_add_entry(p->log, "There are %d more items here.",
                          other_items_count);
        }
    }
}

void player_autopickup_show(player *p)
{
    GString *msg;
    int count = 0;
    item_t it;

    assert (p != NULL);

    msg = g_string_new(NULL);

    for (it = IT_NONE; it < IT_MAX; it++)
    {
        if (p->settings.auto_pickup[it])
        {
            if (count)
                g_string_append(msg, ", ");

            g_string_append(msg, item_name_pl(it));
            count++;
        }
    }

    if (!count)
        g_string_append(msg, "Auto-pickup is not enabled.");
    else
    {
        g_string_prepend(msg, "Auto-pickup is enabled for ");
        g_string_append(msg, ".");
    }


    log_add_entry(p->log, msg->str);

    g_string_free(msg, TRUE);
}

void player_level_gain(player *p, int count)
{
    int base;
    int i;

    char *desc_orig, *desc_new;

    assert(p != NULL && count > 0);

    /* experience level 100 is the end of the career */
    if (p->level == 100)
        return;

    desc_orig = player_get_level_desc(p);

    p->level += count;

    desc_new = player_get_level_desc(p);

    if (g_strcmp0(desc_orig, desc_new) != 0)
    {
        log_add_entry(p->log, "You gain experience and become %s %s!",
                      a_an(desc_new), desc_new);
    }
    else
    {
        log_add_entry(p->log, "You gain experience!");
    }

    if (p->level > p->stats.max_level)
        p->stats.max_level = p->level;

    if (p->experience < player_lvl_exp[p->level - 1])
        /* artificially gained a level, need to adjust XP to match level */
        player_exp_gain(p, player_lvl_exp[p->level - 1] - p->experience);

    for (i = 0; i < count; i++)
    {
        /* increase HP max */
        base = (p->constitution - game_difficulty(nlarn)) >> 1;
        if (p->level < max(7 - game_difficulty(nlarn), 0))
            base += p->constitution >> 2;

        player_hp_max_gain(p, rand_1n(3) + rand_0n(max(base, 1)));

        /* increase MP max */
        base = (p->intelligence - game_difficulty(nlarn)) >> 1;
        if (p->level < max(7 - game_difficulty(nlarn), 0))
            base += p->intelligence >> 2;

        player_mp_max_gain(p, rand_1n(3) + rand_0n(max(base, 1)));
    }
}

void player_level_lose(player *p, int count)
{
    int base;
    int i;

    assert(p != NULL && count > 0);

    p->level -= count;
    log_add_entry(p->log, "You return to experience level %d...", p->level);

    /* die if lost level 1 */
    if (p->level == 0) player_die(p, PD_LASTLEVEL, 0);

    if (p->experience > player_lvl_exp[p->level - 1])
        /* adjust XP to match level */
        player_exp_lose(p, p->experience - player_lvl_exp[p->level - 1]);

    for (i = 0; i < count; i++)
    {
        /* decrease HP max */
        base = (p->constitution - game_difficulty(nlarn)) >> 1;
        if (p->level < max(7 - game_difficulty(nlarn), 0))
            base += p->constitution >> 2;

        player_hp_max_lose(p, rand_1n(3) + rand_0n(max(base, 1)));

        /* decrease MP max */
        base = (p->intelligence - game_difficulty(nlarn)) >> 1;
        if (p->level < max(7 - game_difficulty(nlarn), 0))
            base += p->intelligence >> 2;

        player_mp_max_lose(p, rand_1n(3) + rand_0n(max(base, 1)));
    }
}

void player_exp_gain(player *p, int count)
{
    int numlevels = 0;

    assert(p != NULL && count > 0);
    p->experience += count;

    if (p->stats.max_xp < p->experience)
        p->stats.max_xp = p->experience;

    while (player_lvl_exp[p->level + numlevels] <= p->experience)
        numlevels++;

    if (numlevels)
        player_level_gain(p, numlevels);
}

void player_exp_lose(player *p, int count)
{
    int numlevels = 0;

    assert(p != NULL && count > 0);
    p->experience -= count;

    while ((player_lvl_exp[p->level - 1 - numlevels]) > p->experience)
        numlevels++;

    if (numlevels)
        player_level_lose(p, numlevels);
}


int player_hp_gain(player *p, int count)
{
    assert(p != NULL);

    p->hp += count;
    if (p->hp > player_get_hp_max(p))
        p->hp = player_get_hp_max(p);

    return p->hp;
}

void player_damage_take(player *p, damage *dam, player_cod cause_type, int cause)
{
    monster *m = NULL;
    effect *e = NULL;

    assert(p != NULL && dam != NULL);

    if (dam->originator)
    {
        m = (monster *)dam->originator;

        /* amulet of power cancels demon attacks */
        if (monster_is_demon(m) && chance(75)
                && (p->eq_amulet && p->eq_amulet->id == AM_POWER))
        {
            log_add_entry(p->log, "Your amulet cancels the %s's attack.",
                          monster_get_name(m));

            return;
        }
    }

    /* check resistances */
    switch (dam->type)
    {
    case DAM_PHYSICAL:
        dam->amount -= player_get_ac(p);
        /* taken damage */
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "Ouch!");
        }
        else
        {
            log_add_entry(p->log, "Your armour protects you.");
        }
        break;

    case DAM_MAGICAL:
        dam->amount -= player_effect(p, ET_RESIST_MAGIC);

        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "Ouch!");
        }
        else
        {
            log_add_entry(p->log, "You resist.");
        }

        break;

    case DAM_FIRE:
        dam->amount -= player_effect(p, ET_RESIST_FIRE);
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "You suffer burns.");
        }
        else
        {
            log_add_entry(p->log, "The flames don't phase you.");
        }
        break;

    case DAM_COLD:
        dam->amount -= player_effect(p, ET_RESIST_COLD);
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "You suffer from frostbite.");
        }
        else
        {
            log_add_entry(p->log, "It doesn't seem so cold.");
        }
        break;

    case DAM_ACID:
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "You are splashed with acid.");
        }
        else
        {
            log_add_entry(p->log, "The acid doesn't affect you.");
        }
        break;

    case DAM_WATER:
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "You experience near-drowning.");
        }
        else
        {
            log_add_entry(p->log, "The water doesn't affect you.");
        }
        break;

    case DAM_ELECTRICITY:
        if (dam->amount > 0)
        {
            p->hp -= dam->amount;
            log_add_entry(p->log, "Zapp!");
        }
        else
        {
            log_add_entry(p->log, "As you are grounded nothing happens.");
        }
        break;

    case DAM_POISON:
        /* check if the damage is not caused by the effect that is
           already attached to the player */
        if (cause_type != PD_EFFECT)
        {
            /* check resistance; prevent negative damage amount */
            dam->amount -= max(0, rand_0n(player_get_con(p)));

            if (dam->amount > 0)
            {
                e = effect_new(ET_POISON);
                e->amount = dam->amount;
                e = player_effect_add(p, e);
            }
            else
            {
                log_add_entry(p->log, "You resist the poison.");
            }
        }
        else
        {
            /* damage is caused by the effect of the poison effect () */
            p->hp -= dam->amount;
            log_add_entry(p->log, "You feel poison running through your veins.");
        }

        break;

    case DAM_BLINDNESS:
        if (chance(dam->amount))
        {
            e = player_effect_add(p, effect_new(ET_BLINDNESS));
        }

        if (!e && !(e = player_effect_get(p, ET_BLINDNESS)))
        {
            log_add_entry(p->log, "You are not blinded.");
        }

        break;

    case DAM_CONFUSION:
        if (dam->attack == ATT_GAZE && player_effect_get(p, ET_BLINDNESS))
        {
            /* it is impossible to see a staring monster when blinded */
            return;
        }

        /* check if the player succumbs to the monster's stare */
        if (chance(dam->amount - player_get_int(p)))
        {
            e = player_effect_add(p, effect_new(ET_CONFUSION));
        }

        if (!e && !(e = player_effect_get(p, ET_CONFUSION)))
        {
            log_add_entry(p->log, "You are not confused.");
        }

        break;

    case DAM_PARALYSIS:
        if (dam->attack == ATT_GAZE && player_effect_get(p, ET_BLINDNESS))
        {
            /* it is impossible to see a staring monster when blinded */
            return;
        }

        /* check if the player succumbs to the monster's stare */
        if(chance(dam->amount - player_get_int(p)))
        {
            e = player_effect_add(p, effect_new(ET_PARALYSIS));
        }

        if (!e && !(e = player_effect_get(p, ET_PARALYSIS)))
        {
            log_add_entry(p->log, "You avoid eye contact.");
        }

        break;

    case DAM_DEC_STR:
        if (chance(dam->amount -= player_get_con(p)))
        {
            e = effect_new(ET_DEC_STR);
            /* the default number of turns for ET_DEC_STR is 1 */
            e->turns = dam->amount * 10;
            e = player_effect_add(p, e);

            if (player_get_str(p) < 1)
            {
                player_die(p, PD_EFFECT, ET_DEC_STR);
            }
        }
        else
        {
            log_add_entry(p->log, "You are not affected.");
        }
        break;

    case DAM_DEC_DEX:
        if (chance(dam->amount -= player_get_con(p)))
        {
            e = effect_new(ET_DEC_DEX);
            /* the default number of turns for ET_DEC_DEX is 1 */
            e->turns = dam->amount * 10;
            e = player_effect_add(p, e);

            if (player_get_dex(p) < 1)
            {
                player_die(p, PD_EFFECT, ET_DEC_DEX);
            }
        }
        else
        {
            log_add_entry(p->log, "You are not affected.");
        }
        break;

    case DAM_DRAIN_LIFE:
        if (player_effect(p, ET_UNDEAD_PROTECTION)
                || !chance(dam->amount - player_get_wis(p)))
        {
            /* undead protection cancels drain life attacks */
            log_add_entry(p->log, "You are not affected.");
        }
        else
        {
            log_add_entry(p->log, "Your life energy is drained.");
            player_level_lose(p, 1);
        }
        break;
    default:
        /* the other damage types are not handled here */
        break;
    }

    g_free(dam);

    if (p->hp < 1)
    {
        player_die(p, cause_type, cause);
    }

}

int player_hp_max_gain(player *p, int count)
{
    assert(p != NULL);

    p->hp_max += count;
    /* do _NOT_ increase hp */

    return p->hp_max;
}

int player_hp_max_lose(player *p, int count)
{
    assert(p != NULL);

    p->hp_max -= count;

    if (p->hp_max < 1)
        p->hp_max = 1;

    if (p->hp > (int)p->hp_max)
        p->hp = p->hp_max;

    return p->hp_max;
}

int player_mp_gain(player *p, int count)
{
    assert(p != NULL);

    p->mp += count;
    if (p->mp > player_get_mp_max(p))
        p->mp = player_get_mp_max(p);

    return p->mp;
}

int player_mp_lose(player *p, int count)
{
    assert(p != NULL);

    p->mp -= count;
    if (p->mp < 0)
        p->mp = 0;

    return p->mp;
}

int player_mp_max_gain(player *p, int count)
{
    assert(p != NULL);

    p->mp_max += count;
    /* do _NOT_ increase mp */

    return p->mp_max;
}

int player_mp_max_lose(player *p, int count)
{
    assert(p != NULL);

    p->mp_max -= count;

    if (p->mp_max < 1)
        p->mp_max = 1;

    if (p->mp > (int)p->mp_max)
        p->mp = p->mp_max;

    return p->mp_max;
}

effect *player_effect_add(player *p, effect *e)
{
    assert(p != NULL && e != NULL);

    /* one-time effects are handled here */
    if (e->turns == 1)
    {
        switch (e->type)
        {
        case ET_INC_CHA:
            p->charisma += e->amount;
            break;

        case ET_INC_CON:
            p->constitution += e->amount;
            break;

        case ET_INC_DEX:
            p->dexterity += e->amount;
            break;

        case ET_INC_INT:
            p->intelligence += e->amount;
            break;

        case ET_INC_STR:
            p->strength += e->amount;
            player_inv_weight_recalc(p->inventory, NULL);
            break;

        case ET_INC_WIS:
            p->wisdom += e->amount;
            break;

        case ET_INC_RND:
            player_effect_add(p, effect_new(rand_m_n(ET_INC_CHA, ET_INC_WIS)));
            break;

        case ET_INC_HP_MAX:
            p->hp_max += ((player_get_hp_max(p) / 100) * e->amount);
            break;

        case ET_INC_MP_MAX:
            p->mp_max += ((player_get_mp_max(p) / 100) * e->amount);
            break;

        case ET_INC_LEVEL:
            player_level_gain(p, e->amount);
            break;

        case ET_INC_EXP:
            /* looks like a reasonable amount */
            player_exp_gain(p, rand_1n(player_lvl_exp[p->level - 1] - player_lvl_exp[p->level - 2]));
            break;

        case ET_INC_HP:
            player_hp_gain(p, (int)(((float)p->hp_max / 100) * e->amount));
            break;

        case ET_MAX_HP:
            player_hp_gain(p, player_get_hp_max(p));
            break;

        case ET_INC_MP:
            player_mp_gain(p, (int)(((float)p->mp_max / 100) * e->amount));
            break;

        case ET_MAX_MP:
            player_mp_gain(p, player_get_mp_max(p));
            break;

        case ET_DEC_CHA:
            p->charisma -= e->amount;
            break;

        case ET_DEC_CON:
            p->constitution -= e->amount;
            break;

        case ET_DEC_DEX:
            p->dexterity -= e->amount;
            break;

        case ET_DEC_INT:
            p->intelligence -= e->amount;
            break;

        case ET_DEC_STR:
            p->strength -= e->amount;
            player_inv_weight_recalc(p->inventory, NULL);
            break;

        case ET_DEC_WIS:
            p->wisdom -= e->amount;
            break;

        case ET_DEC_RND:
            player_effect_add(p, effect_new(rand_m_n(ET_DEC_CHA, ET_DEC_WIS)));
            break;

        case ET_DEC_HP_MAX:
            p->hp_max -= ((player_get_hp_max(p) / 100) * e->amount);
            break;

        case ET_DEC_MP_MAX:
            p->mp_max -= ((player_get_mp_max(p) / 100) * e->amount);
            break;

        case ET_DEC_LEVEL:
            player_level_lose(p, e->amount);
            break;

        case ET_DEC_EXP:
            /* looks like a reasonable amount */
            player_exp_lose(p, rand_1n(player_lvl_exp[p->level - 1] - player_lvl_exp[p->level - 2]));
            break;

        default:
            /* nop */
            break;
        }

        if (effect_get_msg_start(e))
            log_add_entry(p->log, "%s", effect_get_msg_start(e));

        effect_destroy(e);
        e = NULL;
    }
    else if (e->type == ET_SLEEP)
    {
        int i;

        if (effect_get_msg_start(e))
            log_add_entry(p->log, "%s", effect_get_msg_start(e));

        for (i = 0; i < e->turns; i++)
        {
            player_make_move(p, 1);
            display_paint_screen(p);
            usleep(50000);
        }

        effect_destroy(e);
        e = NULL;
    }
    else
    {
        int str_orig = player_get_str(p);

        e = effect_add(p->effects, e);

        /* only log a message if the effect has really been added and
           actually has a value */
        if (e && effect_get_amount(e) && effect_get_msg_start(e))
            log_add_entry(p->log, "%s", effect_get_msg_start(e));

        if (str_orig != player_get_str(p))
        {
            /* strength has been modified -> recalc burdened status */
            player_inv_weight_recalc(p->inventory, NULL);
        }
    }

    return e;
}

void player_effects_add(player *p, GPtrArray *effects)
{
    guint idx;

    assert (p != NULL);

    /* if effects is NULL */
    if (!effects) return;

    for (idx = 0; idx < effects->len; idx++)
    {
        gpointer effect_id = g_ptr_array_index(effects, idx);
        g_ptr_array_add(p->effects, effect_id);

        effect *e = game_effect_get(nlarn, effect_id);
        if (effect_get_amount(e) && effect_get_msg_start(e))
        {
            log_add_entry(p->log, effect_get_msg_start(e));
        }
    }
}

int player_effect_del(player *p, effect *e)
{
    int result, str_orig;

    assert(p != NULL && e != NULL && e->type > ET_NONE && e->type < ET_MAX);

    str_orig = player_get_str(p);

    if ((result = effect_del(p->effects, e)))
    {
        if (effect_get_amount(e) && effect_get_msg_stop(e))
        {
            log_add_entry(p->log, effect_get_msg_stop(e));
        }

        if (str_orig != player_get_str(p))
        {
            /* strength has been modified -> recalc burdened status */
            player_inv_weight_recalc(p->inventory, NULL);
        }

        /* finally destroy the effect */
        effect_destroy(e);
    }

    return result;
}

void player_effects_del(player *p, GPtrArray *effects)
{
    guint idx;

    assert (p != NULL);

    /* if effects is NULL */
    if (!effects) return;

    for (idx = 0; idx < effects->len; idx++)
    {
        gpointer effect_id = g_ptr_array_index(effects, idx);
        effect *e = game_effect_get(nlarn, effect_id);
        g_ptr_array_remove_fast(p->effects, effect_id);

        if (effect_get_msg_stop(e))
        {
            log_add_entry(p->log, effect_get_msg_stop(e));
        }
    }
}

effect *player_effect_get(player *p, effect_type et)
{
    assert(p != NULL && et > ET_NONE && et < ET_MAX);
    return effect_get(p->effects, et);
}

int player_effect(player *p, effect_type et)
{
    assert(p != NULL && et > ET_NONE && et < ET_MAX);
    return effect_query(p->effects, et);
}

char **player_effect_text(player *p)
{
    char **text;
    int pos;
    effect *e;

    text = strv_new();

    for (pos = 0; pos < p->effects->len; pos++)
    {
        e = game_effect_get(nlarn, g_ptr_array_index(p->effects, pos));

        if (effect_get_desc(e) != NULL)
        {
            strv_append_unique(&text, effect_get_desc(e));
        }
    }

    return text;
}

int player_inv_display(player *p)
{
    GPtrArray *callbacks;
    display_inv_callback *callback;

    assert(p != NULL);

    if (inv_length(p->inventory) == 0)
    {
        /* don't show empty inventory */
        log_add_entry(p->log, "You do not carry anything.");
        return FALSE;
    }

    /* define callback functions */
    callbacks = g_ptr_array_new();

    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(d)rop";
    callback->key = 'd';
    callback->inv = &p->inventory;
    callback->function = &player_item_drop;
    callback->checkfun = &player_item_is_dropable;
    g_ptr_array_add(callbacks, callback);

    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(e)quip";
    callback->key = 'e';
    callback->function = &player_item_equip;
    callback->checkfun = &player_item_is_equippable;
    g_ptr_array_add(callbacks, callback);

    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(o)pen";
    callback->key = 'o';
    callback->function = &container_open;
    callback->checkfun = &player_item_is_container;
    g_ptr_array_add(callbacks, callback);

    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(p)ut into sth.";
    callback->key = 'p';
    callback->function = &container_item_add;
    callback->checkfun = &player_item_can_be_added_to_container;
    g_ptr_array_add(callbacks, callback);

    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(u)nequip";
    callback->key = 'u';
    callback->function = &player_item_unequip;
    callback->checkfun = &player_item_is_equipped;
    g_ptr_array_add(callbacks, callback);

    /* unequip and use should never appear together */
    callback = g_malloc0(sizeof(display_inv_callback));
    callback->description = "(u)se";
    callback->key = 'u';
    callback->function = &player_item_use;
    callback->checkfun = &player_item_is_usable;
    g_ptr_array_add(callbacks, callback);
    display_inventory("Inventory", p, &p->inventory, callbacks, FALSE,
                      TRUE, FALSE, NULL);

    /* clean up */
    display_inv_callbacks_clean(callbacks);

    return TRUE;
}

static char *player_print_weight(player *p, float weight)
{
    assert (p != NULL);

    static char buf[21] = "";

    char *unit = "g";
    if (weight > 1000)
    {
        weight = weight / 1000;
        unit = "kg";
    }

    g_snprintf(buf, 20, "%g%s", weight, unit);

    return buf;
}

char *player_can_carry(player *p)
{
    static char buf[21] = "";
    g_snprintf(buf, 20, "%s",
               player_print_weight(p, 2000 * 1.3 * (float)player_get_str(p)));
    return buf;
}

char *player_inv_weight(player *p)
{
    static char buf[21] = "";
    g_snprintf(buf, 20, "%s",
               player_print_weight(p, (float)inv_weight(p->inventory)));
    return buf;
}

int player_inv_pre_add(inventory *inv, item *item)
{
    player *p;
    char buf[61];

    int pack_weight;
    float can_carry;

    p = (player *)inv->owner;

    if (player_effect(p, ET_OVERSTRAINED))
    {
        log_add_entry(p->log, "You are already overloaded!");
        return FALSE;
    }

    /* calculate inventory weight */
    pack_weight = inv_weight(inv);

    /* player can carry 2kg per str */
    can_carry = 2000 * (float)player_get_str(p);

    /* check if item weight can be carried */
    if ((pack_weight + item_weight(item)) > (int)(can_carry * 1.3))
    {
        /* get item description */
        item_describe(item, player_item_known(p, item), (item->count == 1), TRUE, buf, 60);
        /* capitalize first letter */
        buf[0] = g_ascii_toupper(buf[0]);

        log_add_entry(p->log, "%s %s too heavy for you.", buf,
                      item->count > 1 ? "are" : "is");

        return FALSE;
    }

    return TRUE;
}

void player_inv_weight_recalc(inventory *inv, item *item)
{
    int pack_weight;
    float can_carry;
    effect *e = NULL;

    player *p;

    assert (inv != NULL);

    p = (player *)inv->owner;       /* make shortcut */
    item = NULL;                    /* don't need that parameter */
    pack_weight = inv_weight(inv);  /* calculate inventory weight */

    /* the player can carry 2kg per str */
    can_carry = 2000 * (float)player_get_str(p);

    if (pack_weight > (int)(can_carry * 1.3))
    {
        /* OVERSTRAINED  */
        if ((e = player_effect_get(p, ET_BURDENED)))
        {
            /* get rid of burden effect (mute log to avoid pointless message) */
            log_disable(p->log);
            player_effect_del(p, e);
            log_enable(p->log);
        }

        /* make overstrained */
        if (!player_effect(p, ET_OVERSTRAINED))
        {
            player_effect_add(p, effect_new(ET_OVERSTRAINED));
        }
    }
    else if (pack_weight < (int)(can_carry * 1.3) && (pack_weight > can_carry))
    {
        /* BURDENED */
        if ((e = player_effect_get(p, ET_OVERSTRAINED)))
        {
            /* get rid of overstrained effect */
            log_disable(p->log);
            player_effect_del(p, e);
            log_enable(p->log);
        }

        if (!player_effect(p, ET_BURDENED))
        {
            player_effect_add(p, effect_new(ET_BURDENED));
        }
    }
    else if (pack_weight < can_carry)
    {
        /* NOT burdened, NOT overstrained */
        if ((e = player_effect_get(p, ET_OVERSTRAINED)))
        {
            player_effect_del(p, e);
        }

        if ((e = player_effect_get(p, ET_BURDENED)))
        {
            player_effect_del(p, e);
        }
    }
}

int player_item_equip(player *p, inventory **inv, item *it)
{
    item **islot = NULL;  /* pointer to chosen item slot */
    int time = 0;         /* time the desired action takes */
    char description[61] = { 0 };

    assert(p != NULL && it != NULL);

    /* don't need that parameter */
    inv = NULL;

    /* the idea behind the time values: one turn to take one item off,
       one turn to get the item out of the pack */

    switch (it->type)
    {
    case IT_AMULET:
        if (p->eq_amulet == NULL)
        {
            p->eq_amulet = it;

            item_describe(it, player_item_known(p, it), TRUE, TRUE, description, 60);
            log_add_entry(p->log, "You put %s on.", description);
            p->identified_amulets[it->id] = TRUE;
            time = 2;
        }
        break;

    case IT_ARMOUR:
        switch (armour_category(it))
        {
        case AC_BOOTS:
            islot = &(p->eq_boots);
            time = 3;
            break;

        case AC_CLOAK:
            islot = &(p->eq_cloak);
            time = 2;
            break;

        case AC_GLOVES:
            islot = &(p->eq_gloves);
            time = 3;
            break;

        case AC_HELMET:
            islot = &(p->eq_helmet);
            time = 2;
            break;

        case AC_SHIELD:
            islot = &(p->eq_shield);
            time = 2;
            break;

        case AC_SUIT:
            islot = &(p->eq_suit);
            time = it->id + 1;
            break;

        case AC_NONE:
        case AC_MAX:
            /* shouldn't happen */
            break;
        }

        if (*islot == NULL)
        {
            it->bonus_known = TRUE;

            item_describe(it, player_item_known(p, it), TRUE, FALSE, description, 60);
            log_add_entry(p->log, "You are now wearing %s.", description);

            *islot = it;
        }
        break;

    case IT_RING:
        if (p->eq_ring_l == NULL)
            islot = &(p->eq_ring_l);
        else if (p->eq_ring_r == NULL)
            islot = &(p->eq_ring_r);

        if (islot != NULL)
        {
            item_describe(it, player_item_known(p, it), TRUE, TRUE, description, 60);
            log_add_entry(p->log, "You put %s on.", description);

            *islot = it;
            p->identified_rings[it->id] = TRUE;

            if (ring_bonus_is_obs(it))
            {
                it->bonus_known = TRUE;
            }

            time = 2;
        }
        break;

    case IT_WEAPON:
        if (p->eq_weapon == NULL)
        {
            item_describe(it, player_item_known(p, it), TRUE, TRUE, description, 60);

            p->eq_weapon = it;
            log_add_entry(p->log, "You now wield %s.", description);

            time = 2 + weapon_is_twohanded(p->eq_weapon);

            if (it->cursed)
            {
                /* capitalize first letter */
                description[0] = g_ascii_toupper(description[0]);
                log_add_entry(p->log, "%s welds itself into your hand.", description);
                it->blessed_known = TRUE;
            }
        }
        break;

    default:
        /* nop */
        break;
    }

    player_effects_add(p, it->effects);

    return time;
}

int player_item_unequip(player *p, inventory **inv, item *it)
{
    int equipment_type;
    item **aslot = NULL;  /* pointer to armour slot */
    item **rslot = NULL;  /* pointer to ring slot */

    int time = 0;         /* time elapsed */
    char desc[61];        /* item description */

    assert(p != NULL && it != NULL);

    /* don't need that parameter */
    inv = NULL;

    equipment_type = player_item_is_equipped(p, it);

    /* the idea behind the time values: one turn to take one item off,
       one turn to get the item out of the pack */

    switch (equipment_type)
    {
    case PE_AMULET:
        if (p->eq_amulet != NULL)
        {
            item_describe(it, player_item_known(p, it), TRUE, TRUE, desc, 60);

            if (!it->cursed)
            {
                log_add_entry(p->log, "You remove %s.", desc);

                player_effects_del(p, p->eq_amulet->effects);
                p->eq_amulet = NULL;

                time = 2;
            }
            else
            {
                log_add_entry(p->log, "You can not remove %s.%s", desc,
                              it->blessed_known ? "" : " It appears to be cursed.");

                it->blessed_known = TRUE;
            }
        }
        break;

    case PE_BOOTS:
        aslot = &(p->eq_boots);
        time = 3;
        break;

    case PE_CLOAK:
        aslot = &(p->eq_cloak);
        time = 2;
        break;

    case PE_GLOVES:
        aslot = &(p->eq_gloves);
        time = 3;
        break;

    case PE_HELMET:
        aslot = &(p->eq_helmet);
        time = 2;
        break;

    case PE_SHIELD:
        aslot = &(p->eq_shield);
        time = 2;
        break;

    case PE_SUIT:
        aslot = &(p->eq_suit);
        /* the better the armour, the longer it takes to get out of it */
        time = (p->eq_suit)->type + 1;
        break;

    case PE_RING_L:
        rslot = &(p->eq_ring_l);
        break;

    case PE_RING_R:
        rslot = &(p->eq_ring_r);
        break;

    case PE_WEAPON:
        if (p->eq_weapon != NULL)
        {
            item_describe(it, player_item_known(p, it), TRUE, TRUE, desc, 60);

            if (!p->eq_weapon->cursed)
            {
                log_add_entry(p->log, "You put away %s.", desc);

                time = 2 + weapon_is_twohanded(p->eq_weapon);
                player_effects_del(p, p->eq_weapon->effects);

                p->eq_weapon = NULL;
            }
            else
            {
                log_add_entry(p->log, "You can't put away %s. " \
                              "It's welded into your hands.", desc);
            }
        }
        break;
    }

    /* take off armour */
    if ((aslot != NULL) && (*aslot != NULL))
    {
        item_describe(it, player_item_known(p, it), TRUE, TRUE, desc, 60);

        if (!it->cursed)
        {
            log_add_entry(p->log, "You finish taking off %s.", desc);

            player_effects_del(p, (*aslot)->effects);

            *aslot = NULL;
        }
        else
        {
            log_add_entry(p->log, "You can't take of %s.%s", desc,
                          it->blessed_known ? "" : " It appears to be cursed.");

            it->blessed_known = TRUE;
        }
    }

    if ((rslot != NULL) && (*rslot != NULL))
    {
        item_describe(it, player_item_known(p, it), TRUE, TRUE, desc, 60);

        if (!it->cursed)
        {
            log_add_entry(p->log, "You remove %s.", desc);

            player_effects_del(p, (*rslot)->effects);

            *rslot = NULL;
            time = 2;
        }
        else
        {
            log_add_entry(p->log, "You can not remove %s.%s", desc,
                          it->blessed_known ? "" : " It appears to be cursed.");

            it->blessed_known = TRUE;
        }
    }

    return time;
}

/* silly filter to get containers */
int player_item_is_container(player *p, item *it)
{
    assert(p != NULL && it != NULL && it->type < IT_MAX);

    return (it->type == IT_CONTAINER);
}

/* silly filter to check if item can be put into a container */
int player_item_can_be_added_to_container(player *p, item *it)
{
    guint idx;
    item *i;

    assert(p != NULL && it != NULL && it->type < IT_MAX);

    if (it->type == IT_CONTAINER)
    {
        return FALSE;
    }

    if (player_item_is_equipped(p, it))
    {
        return FALSE;
    }

    /* check player's inventory for containers */
    for (idx = 0; idx < inv_length(p->inventory); idx++)
    {
        i = inv_get(p->inventory, idx);
        if (i->type == IT_CONTAINER)
        {
            return TRUE;
        }
    }

    /* no match till now, check floor for containers */
    inventory **floor = map_ilist_at(game_map(nlarn, p->pos.z), p->pos);

    for (idx = 0; idx < inv_length(*floor); idx++)
    {
        i = inv_get(*floor, idx);
        if (i->type == IT_CONTAINER)
        {
            return TRUE;
        }
    }

    /* nope */
    return FALSE;
}

int player_item_is_equipped(player *p, item *it)
{
    assert(p != NULL && it != NULL);

    if (!item_is_equippable(it->type))
        return PE_NONE;

    if (it == p->eq_amulet)
        return PE_AMULET;

    if (it == p->eq_boots)
        return PE_BOOTS;

    if (it == p->eq_cloak)
        return PE_CLOAK;

    if (it == p->eq_gloves)
        return PE_GLOVES;

    if (it == p->eq_helmet)
        return PE_HELMET;

    if (it == p->eq_ring_l)
        return PE_RING_L;

    if (it == p->eq_ring_r)
        return PE_RING_R;

    if (it == p->eq_shield)
        return PE_SHIELD;

    if (it == p->eq_suit)
        return PE_SUIT;

    if (it == p->eq_weapon)
        return PE_WEAPON;

    return PE_NONE;
}

int player_item_is_equippable(player *p, item *it)
{
    assert(p != NULL && it != NULL);

    if (!item_is_equippable(it->type))
        return FALSE;

    if (player_item_is_equipped(p, it))
        return FALSE;

    /* amulets */
    if ((it->type == IT_AMULET) && (p->eq_amulet))
        return FALSE;

    /* armour */
    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_BOOTS)
            && (p->eq_boots))
        return FALSE;

    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_CLOAK)
            && (p->eq_cloak))
        return FALSE;

    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_GLOVES)
            && (p->eq_gloves))
        return FALSE;

    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_HELMET)
            && (p->eq_helmet))
        return FALSE;

    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_SHIELD)
            && (p->eq_shield))
        return FALSE;

    /* shield / two-handed weapon combination */
    if (it->type == IT_ARMOUR
            && (armour_category(it) == AC_SHIELD)
            && (p->eq_weapon)
            && weapon_is_twohanded(p->eq_weapon))
        return FALSE;

    if ((it->type == IT_ARMOUR)
            && (armour_category(it) == AC_SUIT)
            && (p->eq_suit))
        return FALSE;

    /* rings */
    if ((it->type == IT_RING) && (p->eq_ring_l) && (p->eq_ring_r))
        return FALSE;

    /* weapons */
    if ((it->type == IT_WEAPON) && (p->eq_weapon))
        return FALSE;

    /* twohanded weapon / shield combinations */
    if ((it->type == IT_WEAPON) && weapon_is_twohanded(it)&& (p->eq_shield))
        return FALSE;

    return TRUE;
}

int player_item_is_usable(player *p, item *it)
{
    assert(p != NULL && it != NULL);
    return item_is_usable(it->type);
}

int player_item_is_dropable(player *p, item *it)
{
    assert(p != NULL && it != NULL);
    return !player_item_is_equipped(p, it);
}

int player_item_is_damaged(player *p, item *it)
{
    assert(p != NULL && it != NULL);

    if (it->corroded) return TRUE;
    if (it->burnt) return TRUE;
    if (it->rusty) return TRUE;

    return FALSE;
}

int player_item_is_affordable(player *p, item *it)
{
    assert(p != NULL && it != NULL);

    return ((item_price(it) <= player_get_gold(p))
            || (item_price(it) <= p->bank_account));
}

int player_item_is_sellable(player *p, item *it)
{
    assert(p != NULL && it != NULL);

    return (!player_item_is_equipped(p, it));
}

int player_item_is_identifiable(player *p, item *it)
{
    return (!player_item_identified(p, it));
}

/* determine if item type is known */
int player_item_known(player *p, item *it)
{
    assert(p != NULL && it != NULL && it->type < IT_MAX);

    switch (it->type)
    {
    case IT_AMULET:
        return p->identified_amulets[it->id];
        break;

    case IT_BOOK:
        return p->identified_books[it->id];
        break;

    case IT_POTION:
        return p->identified_potions[it->id];
        break;

    case IT_RING:
        return p->identified_rings[it->id];
        break;

    case IT_SCROLL:
        return p->identified_scrolls[it->id];
        break;

    default:
        return TRUE;
    }
}

/* determine if a concrete item is fully identified */
int player_item_identified(player *p, item *it)
{
    gboolean known = FALSE;

    assert(p != NULL && it != NULL);

    /* some items are always identified */
    if (!item_is_identifyable(it->type))
        return TRUE;

    known = player_item_known(p, it);

    if (!it->blessed_known)
        known = FALSE;

    if ((it->type == IT_ARMOUR || it->type == IT_RING || it->type == IT_WEAPON)
            && !it->bonus_known)
        known = FALSE;

    return known;
}

char *player_item_identified_list(player *p)
{
    GString *list, *sublist;
    char buf_unidentified[81], buf_identified[81];
    item *it; /* fake pretend item */
    char *heading;

    int type, id, count;

    item_t type_ids[] =
    {
        IT_AMULET,
        IT_BOOK,
        IT_POTION,
        IT_RING,
        IT_SCROLL,
    };

    assert (p != NULL);

    list = g_string_new(NULL);
    it = item_new(type_ids[0], 1);

    for (type = 0; type < 5; type++)
    {
        count = 0;
        sublist = g_string_new(NULL);

        /* item category header */
        heading = g_strdup(item_name_pl(type_ids[type]));
        heading[0] = g_ascii_toupper(heading[0]);

        /* no linefeed before first category */
        g_string_append_printf(sublist, (type == 0) ? "%s\n" : "\n%s\n",
                               heading);

        g_free(heading);

        it->type = type_ids[type];

        for (id = 1; id < item_max_id(type_ids[type]); id++)
        {
            it->id = id;
            if (player_item_known(p, it))
            {

                item_describe(it, FALSE, TRUE, FALSE, buf_unidentified, 80);
                item_describe(it, TRUE, TRUE, FALSE, buf_identified, 80);
                g_string_append_printf(sublist, " %33s - %s \n",
                                       buf_unidentified, buf_identified);
                count++;
            }
        }

        if (count)
        {
            g_string_append(list, sublist->str);
        }

        g_string_free(sublist, TRUE);
    }
    item_destroy(it);

    if (list->len > 0)
    {
        /* append trailing newline */
        g_string_append_c(list, '\n');
        return g_string_free(list, FALSE);
    }
    else
    {
        g_string_free(list, TRUE);
        return NULL;
    }
}

void player_item_identify(player *p, inventory **inv, item *it)
{
    assert(p != NULL && it != NULL);

    /* don't need that parameter */
    inv = NULL;

    switch (it->type)
    {
    case IT_AMULET:
        p->identified_amulets[it->id] = TRUE;
        break;

    case IT_BOOK:
        p->identified_books[it->id] = TRUE;
        break;

    case IT_POTION:
        p->identified_potions[it->id] = TRUE;
        break;

    case IT_RING:
        p->identified_rings[it->id] = TRUE;
        break;

    case IT_SCROLL:
        p->identified_scrolls[it->id] = TRUE;
        break;

    default:
        /* NOP */
        break;
    }

    it->blessed_known = TRUE;
    it->bonus_known = TRUE;
}

int player_item_use(player *p, inventory **inv, item *it)
{
    item_usage_result result;

    assert(p != NULL && it != NULL && it->type > IT_NONE && it->type < IT_MAX);

    /* hide windows */
    display_windows_hide();

    switch (it->type)
    {
        /* read book */
    case IT_BOOK:
        result = book_read(p, it);
        break;

        /* eat food */
    case IT_FOOD:
        result = food_eat(p, it);
        break;

        /* drink potion */
    case IT_POTION:
        result = potion_quaff(p, it);
        break;

        /* read scroll */
    case IT_SCROLL:
        result = scroll_read(p, it);
        break;

    default:
        /* NOP */
        return 0;
        break;
    }

    if (result.identified)
    {
        /* identify the item type, not the entire item */
        switch (it->type)
        {
        case IT_BOOK:
            p->identified_books[it->id] = TRUE;
            break;

        case IT_POTION:
            p->identified_potions[it->id] = TRUE;
            break;

        case IT_SCROLL:
            p->identified_scrolls[it->id] = TRUE;
            break;

        default:
            /* NOP */
            break;
        }
    }

    if (result.used_up)
    {
        if (it->count > 1)
        {
            it->count--;
        }
        else
        {
            inv_del_element(&p->inventory, it);
            item_destroy(it);
        }
    }

    /* show windows */
    display_windows_show();

    return result.time;
}

void player_item_destroy(player *p, item *it)
{
    char desc[81] = { 0 };

    item_describe(it, player_item_known(p, it),
                  (it->count == 1), TRUE, desc, 80);

    desc[0] = g_ascii_toupper(desc[0]);

    if (player_item_is_equipped(p, it))
    {
        log_disable(p->log);
        player_item_unequip(p, &p->inventory, it);
        log_enable(p->log);
    }

    log_add_entry(p->log, "%s %s destroyed!",
                  desc, (it->count == 1) ? "is" : "are");

    inv_del_element(&p->inventory, it);
    item_destroy(it);
}

int player_item_drop(player *p, inventory **inv, item *it)
{
    char desc[61];
    guint count = 0;

    assert(p != NULL && it != NULL && it->type > IT_NONE && it->type < IT_MAX);

    if (player_item_is_equipped(p, it))
        return FALSE;

    if (it->count > 1)
    {
        g_snprintf(desc, 60, "Drop how many %s?", item_name_pl(it->type));

        count = display_get_count(desc, it->count);

        if (!count)
        {
            return 0;
        }
    }

    if (count && count < it->count)
    {
        /* split the item if only a part of it is to be dropped */
        it = item_split(it, count);
        /* otherwise the entire quantity gets dropped */
    }

    /* show the message before dropping the item, as dropping might remove
       the burdened / overloaded effect. If the pre_del callback fails,
       it has to display a message which explains why it has failed. */
    item_describe(it, player_item_known(p, it), FALSE, FALSE, desc, 60);
    log_add_entry(p->log, "You drop %s.", desc);

    if (!inv_del_element(inv, it))
    {
        /* if the callback failed, dropping has failed */
        return FALSE;
    }

    inv_add(map_ilist_at(game_map(nlarn, p->pos.z), p->pos), it);

    /* reveal if item is cursed or blessed when dropping it on an altar */
    map_sobject_t ms = map_sobject_at(game_map(nlarn, p->pos.z), p->pos);

    if (ms == LS_ALTAR
            && (!player_effect(p, ET_BLINDNESS) || game_wizardmode(nlarn)))
    {
        if (it->cursed)
        {
            item_describe(it, player_item_known(p, it), FALSE, TRUE, desc, 60);
            desc[0] = g_ascii_toupper(desc[0]);

            log_add_entry(p->log, "%s is surrounded by a black halo.", desc);
        }

        if (it->blessed)
        {
            item_describe(it, player_item_known(p, it), FALSE, TRUE, desc, 60);
            desc[0] = g_ascii_toupper(desc[0]);

            log_add_entry(p->log, "%s is surrounded by a white halo.", desc);
        }

        it->blessed_known = TRUE;
    }

    return TRUE;
}

int player_item_pickup(player *p, inventory **inv, item *it)
{
    assert(p != NULL && it != NULL && it->type > IT_NONE && it->type < IT_MAX);

    char desc[61];
    guint count = 0;
    gpointer oid = it->oid;

    if ((!called_by_autopickup) && (it->count > 1))
    {
        g_snprintf(desc, 60, "Pick up how many %s?", item_name_pl(it->type));

        count = display_get_count(desc, it->count);

        if (count == 0)
        {
            return 0;
        }

        if (count < it->count)
        {
            it = item_split(it, count);
            /* set oid to NULL to prevent that the original is remove
               from the originating inventory */
            oid = NULL;
        }
    }

    /* prepare item description for logging later.
     * this has to come before adding the item as it may
     * already be freed then (stackable items)
     */
    item_describe(it, player_item_known(p, it), FALSE, FALSE, desc, 60);

    if (inv_add(&p->inventory, it))
    {
        /* adding item to player's inventory succeeded */
        log_add_entry(p->log, "You pick up %s.", desc);
    }
    else
    {
        /* if the item has been split, return it to the originating inventory */
        if (oid == NULL) inv_add(inv, it);

        return FALSE;
    }

    /* if the item has not been split,
       remove it from the originating inventory */
    if (oid != NULL)
        inv_del_oid(inv, oid);

    /* one turn to pick item up, one to stuff it into the pack */
    return 2;
}

int player_read(player *p)
{
    item *it;

    assert (p != NULL);

    if (inv_length_filtered(p->inventory, item_filter_legible) > 0)
    {
        it = display_inventory("Choose an item to read", p, &p->inventory,
                               NULL, FALSE, FALSE, FALSE, item_filter_legible);

        if (it)
        {
            /* repaint screen as it would be plain black
               if a scroll shows a window */
            display_paint_screen(p);
            return player_item_use(p, NULL, it);
        }
    }
    else
    {
        log_add_entry(p->log, "You have nothing to read.");
    }


    return FALSE;
}

int player_quaff(player *p)
{
    item *it;

    assert (p != NULL);

    if (inv_length_filtered(p->inventory, item_filter_potions) > 0)
    {
        it = display_inventory("Choose an item to drink", p, &p->inventory,
                               NULL, FALSE, FALSE, FALSE, item_filter_potions);

        if (it)
        {
            return player_item_use(p, NULL, it);
        }
    }
    else
    {
        log_add_entry(p->log, "You have nothing to drink.");
    }

    return FALSE;
}


int player_equip(player *p)
{
    item *it;

    assert (p != NULL);

    int item_filter_equippable(item *it)
    {
        return player_item_is_equippable(nlarn->p, it);
    }

    if (inv_length_filtered(p->inventory, item_filter_equippable) > 0)
    {
        it = display_inventory("Choose an item to equip", p, &p->inventory,
                               NULL, FALSE, FALSE, FALSE, item_filter_equippable);

        if (it)
        {
            return player_item_equip(p, NULL, it);
        }
    }
    else
    {
        log_add_entry(p->log, "You have nothing you could equip.");
    }

    return FALSE;
}

int player_take_off(player *p)
{
    item *it;

    assert (p != NULL);

    int item_filter_unequippable(item *it)
    {
        return player_item_is_equipped(nlarn->p, it);
    }

    if (inv_length_filtered(p->inventory, item_filter_unequippable) > 0)
    {
        it = display_inventory("Choose an item to take off", p, &p->inventory,
                               NULL, FALSE, FALSE, FALSE, item_filter_unequippable);

        if (it)
        {
            return player_item_unequip(p, NULL, it);
        }
    }
    else
    {
        log_add_entry(p->log, "You have nothing you could take off.");
    }

    return FALSE;
}

int player_drop(player *p)
{
    item *it;

    assert (p != NULL);

    int item_filter_dropable(item *it)
    {
        return player_item_is_dropable(nlarn->p, it);
    }

    if (inv_length_filtered(p->inventory, item_filter_dropable) > 0)
    {
        it = display_inventory("Choose an item to drop", p, &p->inventory,
                               NULL, FALSE, FALSE, FALSE, item_filter_dropable);

        if (it)
        {
             /* repaint the screen as it would be plain black
                if the dialogue to ask for the amount pops up */

            display_paint_screen(p);
            return player_item_drop(p, &p->inventory, it);
        }
    }
    else
    {
        log_add_entry(p->log, "You have nothing you could drop.");
    }

    return FALSE;
}

int player_get_ac(player *p)
{
    int ac = 0;
    assert(p != NULL);

    if (p->eq_boots != NULL)
        ac += armour_ac(p->eq_boots);

    if (p->eq_cloak != NULL)
        ac += armour_ac(p->eq_cloak);

    if (p->eq_gloves != NULL)
        ac += armour_ac(p->eq_gloves);

    if (p->eq_helmet != NULL)
        ac += armour_ac(p->eq_helmet);

    if (p->eq_shield != NULL)
        ac += armour_ac(p->eq_shield);

    if (p->eq_suit != NULL)
        ac += armour_ac(p->eq_suit);

    ac += player_effect(p, ET_PROTECTION);
    ac += player_effect(p, ET_INVULNERABILITY);

    return ac;
}

int player_get_wc(player *p)
{
    int wc = 0;
    assert(p != NULL);

    if (p->eq_weapon != NULL)
        wc += weapon_wc(p->eq_weapon);

    wc += player_effect(p, ET_INC_DAMAGE);
    wc -= player_effect(p, ET_SICKNESS);

    /* minimal damage */
    if (wc < 1)
        wc = 1;

    return wc;
}

int player_get_hp_max(player *p)
{
    assert(p != NULL);
    return p->hp_max
           + player_effect(p, ET_INC_HP_MAX)
           - player_effect(p, ET_DEC_HP_MAX);
}

int player_get_mp_max(player *p)
{
    assert(p != NULL);
    return p->mp_max
           + player_effect(p, ET_INC_MP_MAX)
           - player_effect(p, ET_DEC_MP_MAX);
}

int player_get_str(player *p)
{
    assert(p != NULL);
    return p->strength
           + player_effect(p, ET_INC_STR)
           - player_effect(p, ET_DEC_STR)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_int(player *p)
{
    assert(p != NULL);
    return p->intelligence
           + player_effect(p, ET_INC_INT)
           - player_effect(p, ET_DEC_INT)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_wis(player *p)
{
    assert(p != NULL);
    return p->wisdom
           + player_effect(p, ET_INC_WIS)
           - player_effect(p, ET_DEC_WIS)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_con(player *p)
{
    assert(p != NULL);
    return p->constitution
           + player_effect(p, ET_INC_CON)
           - player_effect(p, ET_DEC_CON)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_dex(player *p)
{
    assert(p != NULL);
    return p->dexterity
           + player_effect(p, ET_INC_DEX)
           - player_effect(p, ET_DEC_DEX)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_cha(player *p)
{
    assert(p != NULL);
    return p->charisma
           + player_effect(p, ET_INC_CHA)
           - player_effect(p, ET_DEC_CHA)
           + player_effect(p, ET_HEROISM)
           - player_effect(p, ET_DIZZINESS);
}

int player_get_speed(player *p)
{
    assert(p != NULL);
    return nlarn->p->speed
           + player_effect(nlarn->p, ET_SPEED)
           - player_effect(nlarn->p, ET_SLOWNESS);
}

guint player_get_gold(player *p)
{
    guint idx;
    item *i;

    assert(p != NULL);

    for (idx = 0; idx < inv_length(p->inventory); idx++)
    {
        i = inv_get(p->inventory, idx);
        if (i->type == IT_GOLD)
            return i->count;
    }

    return 0;
}

guint player_set_gold(player *p, guint amount)
{
    guint idx;
    item *i;

    assert(p != NULL);

    for (idx = 0; idx < inv_length(p->inventory); idx++)
    {
        i = inv_get(p->inventory, idx);
        if (i->type == IT_GOLD)
        {
            if (!amount)
            {
                inv_del_element(&p->inventory, i);
            }
            else
            {
                i->count = amount;
            }

            /* force recalculation of inventory weight */
            player_inv_weight_recalc(p->inventory, NULL);

            return !amount ? amount : i->count;
        }
    }

    /* no gold found -> generate new gold */
    i = item_new(IT_GOLD, amount);
    inv_add(&p->inventory, i);

    return i->count;
}

char *player_get_level_desc(player *p)
{
    assert(p != NULL);
    return (char *) player_level_desc[p->level - 1];
}

void player_list_sobjmem(player *p)
{
    int idx;
    int prevmap = -1;
    GString *sobjlist = g_string_new(NULL);

    if (p->sobjmem == NULL)
    {
        g_string_append(sobjlist, "You have not discovered any landmarks yet.");

    }
    else
    {
        /* sort the array of memorized landmarks */
        g_array_sort(p->sobjmem, player_sobjects_sort);

        /* assemble a list of landmarks per map */
        for (idx = 0; idx < p->sobjmem->len; idx++)
        {
            player_sobject_memory *som;
            som = &g_array_index(p->sobjmem, player_sobject_memory, idx);

            g_string_append_printf(sobjlist, "%-4s %s (%d, %d)\n",
                                   (som->pos.z > prevmap) ? map_names[som->pos.z] : "",
                                   ls_get_desc(som->sobject),
                                   som->pos.y, som->pos.x);

            if (som->pos.z > prevmap) prevmap = som->pos.z;
        }
    }

    display_show_message("Discovered landmarks", sobjlist->str, 5);
    g_string_free(sobjlist, TRUE);
}

/* this and the following function have been
 * ported from python to c using the example at
 * http://roguebasin.roguelikedevelopment.org/index.php?title=Python_shadowcasting_implementation
 */
void player_update_fov(player *p)
{
    int radius;
    int octant;
    position pos;
    map *map;

    area *enlight;

    const int mult[4][8] =
    {
        { 1,  0,  0, -1, -1,  0,  0,  1 },
        { 0,  1, -1,  0,  0, -1,  1,  0 },
        { 0,  1,  1,  0,  0, -1, -1,  0 },
        { 1,  0,  0,  1, -1,  0,  0, -1 }
    };

    int range = (p->pos.z == 0 ? 15 : 6);

    /* calculate range */
    radius = (player_effect(nlarn->p, ET_BLINDNESS) ? 0 : range + player_effect(nlarn->p, ET_AWARENESS));

    /* reset FOV */
    memset(&(p->fov), 0, MAP_SIZE * sizeof(int));

    /* get current map */
    map = game_map(nlarn, p->pos.z);

    /* set level correctly */
    pos.z = p->pos.z;

    /* if player is enlightened, use a circular area around the player
     * otherwise fov algorithm
     */

    if (player_effect(p, ET_ENLIGHTENMENT))
    {
        int x, y;

        enlight = area_new_circle(p->pos, player_effect(p, ET_ENLIGHTENMENT), FALSE);

        /* set visible field according to returned area */
        for (y = 0; y <  enlight->size_y; y++)
        {
            for (x = 0; x < enlight->size_x; x++)
            {
                pos.x = x + enlight->start_x;
                pos.y = y + enlight->start_y;

                if (pos_valid(pos) && area_point_get(enlight, x, y))
                    p->fov[(pos).y][(pos).x] = TRUE;
            }
        }

        area_destroy(enlight);
    }
    else
    {
        /* determine which fields are visible */
        for (octant = 0; octant < 8; octant++)
        {
            player_calculate_octant(p, 1, 1.0, 0.0, radius,
                                    mult[0][octant], mult[1][octant],
                                    mult[2][octant], mult[3][octant]);

        }
        p->fov[p->pos.y][p->pos.x] = TRUE;
    }

    /* update visible fields in player's memory */
    for (pos.y = 0; pos.y < MAP_MAX_Y; pos.y++)
    {
        for (pos.x = 0; pos.x < MAP_MAX_X; pos.x++)
        {
            if (player_pos_visible(p, pos))
            {
                monster *m = map_get_monster_at(map, pos);
                inventory **inv = map_ilist_at(map, pos);

                player_memory_of(p,pos).type = map_tiletype_at(map, pos);
                player_memory_of(p,pos).sobject = map_sobject_at(map, pos);

                /* remember certain stationary objects */
                switch (map_sobject_at(map, pos))
                {
                    case LS_ALTAR:
                    case LS_BANK2:
                    case LS_FOUNTAIN:
                    case LS_MIRROR:
                    case LS_THRONE:
                    case LS_THRONE2:
                    case LS_STATUE:
                        player_sobject_memorize(p, map_sobject_at(map, pos), pos);
                        break;

                    default:
                        player_sobject_forget(p, pos);
                        break;
                }

                if (inv_length(*inv) > 0)
                {
                    item *it;

                    /* memorize the most interesting item on the tile */
                    if (inv_length_filtered(*inv, item_filter_gems) > 0)
                    {
                        /* there's a gem in the stack */
                        it = inv_get_filtered(*inv, 0, item_filter_gems);
                    }
                    else if (inv_length_filtered(*inv, item_filter_gold) > 0)
                    {
                        /* there is gold in the stack */
                        it = inv_get_filtered(*inv, 0, item_filter_gold);
                    }
                    else
                    {
                        /* memorize the topmost item on the stack */
                        it = inv_get(*inv, inv_length(*inv) - 1);
                    }

                    player_memory_of(p,pos).item = it->type;
                    player_memory_of(p,pos).item_colour = item_colour(it);
                }
                else if (m && monster_is_mimic(m) && monster_unknown(m))
                {
                    /* remember the undiscovered mimic as an item */
                    player_memory_of(p,pos).item = monster_item_type(m);
                    player_memory_of(p,pos).item_colour = monster_colour(m);
                }
                else
                {
                    /* no item at that position */
                    player_memory_of(p,pos).item = IT_NONE;
                    player_memory_of(p,pos).item_colour = 0;
                }
            }
        }
    }
}

int player_pos_visible(player *p, position pos)
{
    assert (p != NULL && pos_valid(pos));
    return (p->pos.z == pos.z) && p->fov[pos.y][pos.x];
}

static void player_calculate_octant(player *p, int row, float start,
                                    float end, int radius, int xx,
                                    int xy, int yx, int yy)
{
    int radius_squared;
    int j;
    int dx, dy;
    int X, Y;
    int blocked;
    float l_slope, r_slope;
    float new_start = 0;

    if (start < end)
        return;

    radius_squared = radius * radius;

    for (j  = row; j <= radius + 1; j++)
    {
        dx = -j - 1;
        dy = -j;

        blocked = FALSE;

        while (dx <= 0)
        {
            dx += 1;

            /* Translate the dx, dy coordinates into map coordinates: */
            X = p->pos.x + dx * xx + dy * xy;
            Y = p->pos.y + dx * yx + dy * yy;

            /* check if coordinated are within bounds */
            if ((X < 0) || (X >= MAP_MAX_X))
                continue;

            if ((Y < 0) || (Y >= MAP_MAX_Y))
                continue;

            /* l_slope and r_slope store the slopes of the left and right
             * extremities of the square we're considering: */
            l_slope = (dx - 0.5) / (dy + 0.5);
            r_slope = (dx + 0.5) / (dy - 0.5);

            if (start < r_slope)
            {
                continue;
            }
            else if (end > l_slope)
            {
                break;
            }
            else
            {
                /* Our light beam is touching this square; light it */
                if ((dx * dx + dy * dy) < radius_squared)
                {
                    p->fov[Y][X] = TRUE;
                }

                if (blocked)
                    /* we're scanning a row of blocked squares */
                    if (!map_pos_transparent(game_map(nlarn, p->pos.z), pos_new(X,Y, p->pos.z)))
                    {
                        new_start = r_slope;
                        continue;
                    }
                    else
                    {
                        blocked = FALSE;
                        start = new_start;
                    }
                else
                {
                    if (!map_pos_transparent(game_map(nlarn, p->pos.z), pos_new(X, Y, p->pos.z)) && (j < radius))
                        /* This is a blocking square, start a child scan */
                        blocked = TRUE;

                    player_calculate_octant(p, j+1, start, l_slope, radius, xx, xy, yx, yy);
                    new_start = r_slope;
                }
            }
        }

        /* Row is scanned; do next row unless last square was blocked */
        if (blocked)
        {
            break;
        }
    }
}

static void player_sobject_memorize(player *p, map_sobject_t sobject, position pos)
{
    player_sobject_memory nsom;
    int idx;

    if (p->sobjmem == NULL)
    {
        p->sobjmem = g_array_new(FALSE, FALSE, sizeof(player_sobject_memory));
    }

    /* check if the sobject has already been memorized */
    for (idx = 0; idx < p->sobjmem->len; idx++)
    {
        player_sobject_memory *som;
        som = &g_array_index(p->sobjmem, player_sobject_memory, idx);

        /* memory for this position exists */
        if (pos_identical(som->pos, pos))
        {
            /* update remembered sobject */
            som->sobject = sobject;

            /* the work is done */
            return;
        }
    }

    /* add a new memory entry */
    nsom.pos = pos;
    nsom.sobject = sobject;

    g_array_append_val(p->sobjmem, nsom);
}

static void player_sobject_forget(player *p, position pos)
{
    player_sobject_memory *som;
    int idx;

    /* just return if nothing has been memorized yet */
    if (p->sobjmem == NULL)
    {
        return;
    }

    for (idx = 0; idx < p->sobjmem->len; idx++)
    {
        som = &g_array_index(p->sobjmem, player_sobject_memory, idx);

        /* remove existing entries for this position */
        if (pos_identical(som->pos, pos))
        {
            g_array_remove_index(p->sobjmem, idx);
            break;
        }
    }

    /* free the sobjemt memory if no entry remains */
    if (p->sobjmem->len == 0)
    {
        g_array_free(p->sobjmem, TRUE);
        p->sobjmem = NULL;
    }
}

static int player_sobjects_sort(gconstpointer a, gconstpointer b)
{
    player_sobject_memory *som_a = (player_sobject_memory *)a;
    player_sobject_memory *som_b = (player_sobject_memory *)b;

    if (som_a->pos.z == som_b->pos.z)
    {
        if (som_a->sobject == som_b->sobject)
            return 0;
        if (som_a->sobject < som_b->sobject)
            return -1;
        else
            return 0;
    }
    else if (som_a->pos.z < som_b->pos.z)
        return -1;
    else
        return 1;
}

static cJSON *player_memory_serialize(player *p, position pos)
{
    cJSON *mser;

    mser = cJSON_CreateObject();
    if (player_memory_of(p, pos).type > LT_NONE)
        cJSON_AddNumberToObject(mser, "type",
                                player_memory_of(p, pos).type);

    if (player_memory_of(p, pos).sobject > LS_NONE)
        cJSON_AddNumberToObject(mser, "sobject",
                                player_memory_of(p, pos).sobject);

    if (player_memory_of(p, pos).item > IT_NONE)
        cJSON_AddNumberToObject(mser, "item",
                                player_memory_of(p, pos).item);

    if (player_memory_of(p, pos).item_colour > 0)
        cJSON_AddNumberToObject(mser, "item_colour",
                                player_memory_of(p, pos).item_colour);

    if (player_memory_of(p, pos).trap > TT_NONE)
        cJSON_AddNumberToObject(mser, "trap",
                                player_memory_of(p, pos).trap);

    return mser;
}

static void player_memory_deserialize(player *p, position pos, cJSON *mser)
{
    cJSON *obj;

    obj = cJSON_GetObjectItem(mser, "type");
    if (obj != NULL)
        player_memory_of(p, pos).type = obj->valueint;

    obj = cJSON_GetObjectItem(mser, "sobject");
    if (obj != NULL)
        player_memory_of(p, pos).sobject = obj->valueint;

    obj = cJSON_GetObjectItem(mser, "item");
    if (obj != NULL)
        player_memory_of(p, pos).item = obj->valueint;

    obj = cJSON_GetObjectItem(mser, "item_colour");
    if (obj != NULL)
        player_memory_of(p, pos).item_colour = obj->valueint;

    obj = cJSON_GetObjectItem(mser, "trap");
    if (obj != NULL)
        player_memory_of(p, pos).trap = obj->valueint;
}

static char *player_death_description(game_score_t *score, int verbose)
{
    char *desc;
    GString *text;

    assert(score != NULL);

    switch (score->cod)
    {
    case PD_LASTLEVEL:
        desc = "passed away";
        break;

    case PD_STUCK:
        desc = "got stuck in solid rock";
        break;

    case PD_TOO_LATE:
        desc = "returned with the potion too late";
        break;

    case PD_WON:
        desc = "returned in time with the cure";
        break;

    case PD_LOST:
        desc = "could not find the potion in time";
        break;

    case PD_QUIT:
        desc = "quit the game";
        break;

    default:
        desc = "killed";
    }

    text = g_string_new_len(NULL, 200);

    g_string_append_printf(text, "%s (%c), %s", score->player_name,
                           (score->sex == PS_MALE) ? 'm' : 'f', desc);

    if (verbose)
    {
        g_string_append_printf(text, " on level %d", score->dlevel);

        if (score->dlevel_max > score->dlevel)
        {
            g_string_append_printf(text, " (max. %d)", score->dlevel_max);
        }

        if (score->cod < PD_TOO_LATE)
        {
            g_string_append_printf(text, " with %d and a maximum of %d hp",
                                   score->hp, score->hp_max);
        }
    }

    switch (score->cod)
    {
    case PD_EFFECT:
        switch (score->cause)
        {
        case ET_DEC_STR:
            g_string_append(text, " by enfeeblement.");
            break;

        case ET_DEC_DEX:
            g_string_append(text, " by clumsiness.");
            break;

        case ET_POISON:
            g_string_append(text, " by poison.");
            break;
        }
        break;

    case PD_LASTLEVEL:
        g_string_append_printf(text,". %s left %s body.",
                               (score->sex == PS_MALE) ? "He" : "She",
                               (score->sex == PS_MALE) ? "his" : "her");
        break;

    case PD_MONSTER:
        /* TODO: regard monster's invisibility */
        /* TODO: while sleeping / doing sth. */
        g_string_append_printf(text, " by %s %s.",
                               a_an(monster_type_name(score->cause)),
                               monster_type_name(score->cause));
        break;

    case PD_SPHERE:
        g_string_append(text, " by a sphere of destruction.");
        break;

    case PD_TRAP:
        g_string_append_printf(text, " by %s %s.",
                               a_an(trap_description(score->cause)),
                               trap_description(score->cause));
        break;

    case PD_MAP:
        g_string_append_printf(text, " by %s.", lt_get_desc(score->cause));
        break;

    case PD_SPELL:
        g_string_append_printf(text, " %s with the spell \"%s\".",
                               (score->sex == PS_MALE) ? "himself" : "herself",
                               spell_name_by_id(score->cause));
        break;

    case PD_CURSE:
        g_string_append_printf(text, " by a cursed %s.",
                               item_name_sg(score->cause));
        break;

    case PD_SOBJECT:
        /* currently only the fountain can cause death */
        g_string_append(text, " by toxic water from a fountain.");
        break;

    default:
        /* no further description */
        g_string_append_c(text, '.');
        break;
    }

    if (verbose)
    {
        g_string_append_printf(text, " %s has scored %" G_GINT64_FORMAT " points.",
                               (score->sex == PS_MALE) ? "He" : "She", score->score);
    }

    return g_string_free(text, FALSE);
}

