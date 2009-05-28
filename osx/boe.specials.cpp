#include <stdio.h>
#include <string.h>

//#include "item.h"

#include "boe.global.h"

#include "classes.h"
#include "boe.party.h"
#include "boe.town.h"
#include "boe.text.h"
#include "boe.infodlg.h"
#include "boe.items.h"
#include "boe.itemdata.h"
#include "boe.combat.h"
#include "boe.monster.h"
#include "boe.locutils.h"
#include "boe.fields.h"
#include "soundtool.h"
#include "boe.townspec.h"
#include "boe.graphics.h"
#include "boe.fileio.h"
#include "boe.specials.h"
#include "boe.newgraph.h"
#include "boe.dlgutil.h"
#include "dlgconsts.h"
#include "mathutil.h"
#include "boe.main.h"
#include "dlgutil.h"
#include "fileio.h"

extern WindowPtr mainPtr;
extern eGameMode overall_mode;
//extern party_record_type party;
//extern current_town_type	univ.town;
//extern unsigned char univ.out[96][96],out_e[96][96],sfx[64][64];
extern ter_num_t combat_terrain[64][64];
extern short which_combat_type,current_pc,stat_window;
//extern cOutdoors univ.out.outdoors[2][2];
extern location pc_pos[6],center;
extern bool in_scen_debug,belt_present,processing_fields,monsters_going,suppress_stat_screen,boom_anim_active;
//extern big_tr_type t_d;
//extern pc_record_type ADVEN[6];
extern effect_pat_type current_pat;
//extern town_item_list	univ.town;
extern cOutdoors::cWandering store_wandering_special;
extern short pc_marked_damage[6];
extern short monst_marked_damage[60];
extern DialogPtr modeless_dialogs[18];
extern bool fast_bang,end_scenario;
//extern short town_size[3];
extern short town_type;
extern cScenario scenario;
extern cUniverse univ;
//extern piles_of_stuff_dumping_type *data_store;

bool can_draw_pcs = true;

short boom_gr[8] = {3,0,2,1,1,4,3,3};								
short store_item_spell_level = 10;

// global vbalues for when processing special encounters
short current_pc_picked_in_spec_enc = -1; // pc that's been selected, -1 if none
short skill_max[20] = {20,20,20,20,20,20,20,20,20,7,
						7,20,20,10,20,20,20,20,20};
location store_special_loc;
bool special_in_progress = false;
short spec_str_offset[3] = {160,10,20};

//// Pretty much all of this is new for BoE
							
	// 0 - everywhere 1 - combat only 2 - town only 3 - town & combat only  4 - can't use  5 - outdoor
	// + 10 - mag. inept can use
	short abil_chart[200] = {4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4, // 50
							4,4,4,4,4,4,4,4,4,4,
							13,0,0,0,3, 3,3,0,3,3,
							3,3,3,3,3, 0,0,0,0,3,
							13,3,3,5,0, 0,0,0,0,0,
							4,4,4,4,4,4,4,4,4,4, // 100
							1,1,1,1,1, 1,1,1,1,3,
							3,1,1,1,1, 1,1,1,1,3,
							1,2,2,1,1, 1,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4, // 150
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4,
							4,4,4,4,4,4,4,4,4,4};




bool town_specials(short which,short t_num)
//short which; // number, 0 - 49, of special
{
	bool can_enter = true;
	short spec_id;
	location l;
	
	
	l = univ.town->special_locs[which];
	spec_id = univ.town->spec_id[which];
	if (spec_id < 0)
		return true;
	
	// call special
			
	erase_specials();
		
	return can_enter;
}

bool handle_wandering_specials (short which,short mode)
// which is unused
//short mode; // 0 - pre  1 - end by victory  2 - end by flight
// wanderin spec 99 -> generic spec
{

	short s1 = 0,s2 = 0,s3 = 0;
	
	if ((mode == 0) && (store_wandering_special.spec_on_meet >= 0)) { // When encountering
	run_special(13,1,store_wandering_special.spec_on_meet,loc(),&s1,&s2,&s3);
	if (s1 > 0)
		return false;
	}
		
	if ((mode == 1) && (store_wandering_special.spec_on_win >= 0))  {// After defeating
	run_special(15,1,store_wandering_special.spec_on_win,loc(),&s1,&s2,&s3);
		}
	if ((mode == 2) && (store_wandering_special.spec_on_flee >= 0))  {// After fleeing like a buncha girly men
	run_special(14,1,store_wandering_special.spec_on_flee,loc(),&s1,&s2,&s3);
		}		
return true;
}


bool check_special_terrain(location where_check,short mode,short which_pc,short *spec_num,
	bool *forced)
//short mode; // 0 - out 1 - town 2 - combat
// returns true if can enter this space
// sets forced to true if definitely can enter
{
	ter_num_t ter;
	short r1,i,choice,door_pc,ter_special,ter_flag1,ter_flag2,ter_flag3,pic_type = 0,ter_pic = 0;
	eDamageType dam_type = DAMAGE_WEAPON;
	bool can_enter = true;
	location out_where,from_loc,to_loc;
	short s1 = 0,s2 = 0,s3 = 0;
	
	*spec_num = -1;
	*forced = false;
	
	switch (mode) {
		case 0:
			ter = univ.out[where_check.x][where_check.y];
			from_loc = univ.party.p_loc;
			break;	
		case 1:
			ter = univ.town->terrain(where_check.x,where_check.y);
			from_loc = univ.town.p_loc;
			break;	
		case 2:
			ter = combat_terrain[where_check.x][where_check.y];
			from_loc = pc_pos[current_pc];
			break;	
		}
	ter_special = scenario.ter_types[ter].special;
	ter_flag1 = scenario.ter_types[ter].flag1;
	ter_flag2 = scenario.ter_types[ter].flag2;
	ter_flag3 = scenario.ter_types[ter].flag3;
	ter_pic = scenario.ter_types[ter].picture;

		if ((mode > 0) && (ter_special == TER_SPEC_CONVEYOR)) {
			if (
				((ter_flag3 == DIR_N) && (where_check.y > from_loc.y)) ||
				((ter_flag3 == DIR_E) && (where_check.x < from_loc.x)) ||
				((ter_flag3 == DIR_S) && (where_check.y < from_loc.y)) ||
				((ter_flag3 == DIR_W) && (where_check.x > from_loc.x)) ) {
					ASB("The moving floor prevents you.");
					return false;
					}
			}

			if (mode == 0) {
				out_where = global_to_local(where_check);

				for (i = 0; i < 18; i++) 
					if (out_where == univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_locs[i]) {
						*spec_num = univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_id[i];
						if ((*spec_num >= 0) &&
							(univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].specials[*spec_num].type == 4))
								*forced = true;
						// call special
						run_special(mode,1,univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_id[i],out_where,&s1,&s2,&s3);
						if (s1 > 0)
							can_enter = false;
						erase_out_specials();
						put_pc_screen();	
						put_item_screen(stat_window,0);	
					}
			}

	if ((is_combat()) && (univ.town.is_spot(where_check.x, where_check.y) ||
						  (scenario.ter_types[coord_to_ter(where_check.x, where_check.y)].trim_type == TRIM_CITY))) {
		ASB("Move: Can't trigger this special in combat.");
		return false; // TODO: Maybe replace TRIM_CITY check with a blockage == clear/special && is_special() check?
		}

	if (((mode == 1) || ((mode == 2) && (which_combat_type == 1)))
	&& (univ.town.is_special(where_check.x,where_check.y))) {
			if (univ.town.is_force_barr(where_check.x,where_check.y)) {
				add_string_to_buf("  Magic barrier!               ");	
				return false;
				}
			for (i = 0; i < 50; i++)
				if (where_check == univ.town->special_locs[i]) {
					if (univ.town->specials[univ.town->spec_id[i]].type == 4) {
						*forced = true;
						}
					*spec_num = univ.town->spec_id[i];
					if ((is_blocked(where_check) == false) || (ter_special == TER_SPEC_CHANGE_WHEN_STEP_ON)
	 					|| (ter_special == TER_SPEC_CALL_SPECIAL)) {
						give_help(54,0,0);
						run_special(mode,2,univ.town->spec_id[i],where_check,&s1,&s2,&s3);
						if (s1 > 0)
							can_enter = false;
						}
					}
			put_pc_screen();	
			put_item_screen(stat_window,0);	
	}
	
	if (can_enter == false)
		return false;
	
	if ((!is_out()) && (overall_mode < MODE_TALKING)) {
		check_fields(where_check,mode,which_pc);
		
		if (univ.town.is_web(where_check.x,where_check.y)) {
			add_string_to_buf("  Webs!               ");
			if (mode < 2) {
				suppress_stat_screen = true;
				for (i = 0; i < 6; i++) {
					r1 = get_ran(1,2,3);
					web_pc(i,r1);
				}
				suppress_stat_screen = true;
				put_pc_screen();	
			}
			else web_pc(current_pc,get_ran(1,2,3));
			univ.town.set_web(where_check.x,where_check.y,false);
		}
		if (univ.town.is_force_barr(where_check.x,where_check.y)) {
			add_string_to_buf("  Magic barrier!               ");	
			can_enter = false;
		}
		if (univ.town.is_crate(where_check.x,where_check.y)) {
			add_string_to_buf("  You push the crate.");
			to_loc = push_loc(from_loc,where_check);
			univ.town.set_crate((short) where_check.x,(short) where_check.y,false);
			if (to_loc.x > 0)
				univ.town.set_crate((short) to_loc.x,(short) to_loc.y,true);
			for (i = 0; i < NUM_TOWN_ITEMS; i++)
				if ((univ.town.items[i].variety > 0) && (univ.town.items[i].item_loc == where_check)
					&& (univ.town.items[i].is_contained() == true))
					univ.town.items[i].item_loc = to_loc;
		}
		if (univ.town.is_barrel(where_check.x,where_check.y)) {
			add_string_to_buf("  You push the barrel.");
			to_loc = push_loc(from_loc,where_check);
			univ.town.set_barrel((short) where_check.x,(short) where_check.y,false);
			if (to_loc.x > 0)
				univ.town.set_barrel((short) to_loc.x,(short) to_loc.y,false);
			for (i = 0; i < NUM_TOWN_ITEMS; i++)
				if ((univ.town.items[i].variety > 0) && (univ.town.items[i].item_loc == where_check)
					&& (univ.town.items[i].is_contained()))
					univ.town.items[i].item_loc = to_loc;
		}
	}

	switch (ter_special) {
		case TER_SPEC_CHANGE_WHEN_STEP_ON: 
			alter_space(where_check.x,where_check.y,ter_flag1);
			if (ter_flag2 < 200) {
				play_sound(-1 * ter_flag2);
				}
			give_help(47,65,0);
			if (scenario.ter_types[ter].blockage > 2)
				can_enter = false;
			break;
		case TER_SPEC_DAMAGING:
			//if the party is flying, in a boat, or entering a boat, they cannot be harmed by terrain
			if (flying() || univ.party.in_boat >= 0 || (mode?town_boat_there(where_check):out_boat_there(where_check)) < 30)
				break;
			if(ter_flag3 > 0 && ter_flag3 < 8)
				dam_type = (eDamageType) ter_flag3;
			else dam_type = DAMAGE_WEAPON;
			switch(dam_type){
				case DAMAGE_FIRE:
					add_string_to_buf("  It's hot!");
					pic_type = 0;
					if (PSD[SDF_PARTY_FIREWALK] > 0) {
						add_string_to_buf("  It doesn't affect you.");			
						goto LBL_NO_DAMAGE; //TODO: nix the goto
					}
					break;
				case DAMAGE_COLD:
					add_string_to_buf("  You feel cold!");
					pic_type = 4;
					break;
				case DAMAGE_MAGIC:
				case DAMAGE_UNBLOCKABLE:
					add_string_to_buf("  Something shocks you!");
					pic_type = 1;
					break;
				case DAMAGE_WEAPON:
					add_string_to_buf("  You feel pain!");
					pic_type = 3;
					break;
				case DAMAGE_POISON:
					add_string_to_buf("  You suddenly feel very ill for a moment...");
					pic_type = 2;
					break;
				case DAMAGE_UNDEAD:
				case DAMAGE_DEMON:
					add_string_to_buf("  A dark wind blows through you!");
					pic_type = 1; // TODO: Verify that this is correct
					break;
				default:
					break;
			}
			r1 = get_ran(ter_flag2,dam_type,ter_flag1);
			if (mode < 2)
				hit_party(r1,dam_type);
			fast_bang = 1;
			if (mode == 2)
				damage_pc(which_pc,r1,dam_type,MONSTER_TYPE_UNKNOWN,0);
			if (overall_mode < MODE_COMBAT)
				boom_space(univ.party.p_loc,overall_mode,pic_type,r1,12);
			fast_bang = 0;
			LBL_NO_DAMAGE: break;
		case TER_SPEC_DANGEROUS:
			//if party is flying, in a boat, or entering a boat, they cannot receive statuses from terrain
			if (flying() || univ.party.in_boat >= 0 || (mode?town_boat_there(where_check):out_boat_there(where_check)) < 30)
				break;
			//one_sound(17);
			if (mode == 2) i = which_pc; else i = 0;
			for ( ; i < 6; i++) 
				if (ADVEN[i].main_status == 1) {
					if (get_ran(1,1,100) <= ter_flag2) {
						switch(ter_flag3){
							case STATUS_POISONED_WEAPON: // TODO: Do something here
								break;
							case STATUS_BLESS:
								curse_pc(i,ter_flag1);
								break;
							case STATUS_POISON:
								poison_pc(i,ter_flag1);
								break;
							case STATUS_HASTE:
								slow_pc(i,ter_flag1);
								break;
							case STATUS_INVULNERABLE: // TODO: Do something here
								break;
							case STATUS_MAGIC_RESISTANCE: // TODO: Do something here
								break;
							case STATUS_WEBS:
								web_pc(i,ter_flag1);
								break;
							case STATUS_DISEASE:
								disease_pc(i,ter_flag1);
								break;
							case STATUS_INVISIBLE:
								void_sanctuary(i);
								break;
							case STATUS_DUMB:
								dumbfound_pc(i,ter_flag1);
								break;
							case STATUS_MARTYRS_SHIELD: // TODO: Do something here
								break;
							case STATUS_ASLEEP:
								sleep_pc(i,ter_flag1,STATUS_ASLEEP,ter_flag1);
								break;
							case STATUS_PARALYZED: // TODO: Do something here
								break;
							case STATUS_ACID:
								acid_pc(i,ter_flag1);
								break;
							case 14: // bless
								curse_pc(i,-ter_flag1);
								break;
							case 15: // haste
								slow_pc(i,-ter_flag1);
								break;
						}
						if(mode == 2) break; // only damage once in combat!
					}
				}
			//print_nums(1,which_pc,current_pc);
//			if (mode == 2) {
//						if (get_ran(1,1,100) <= ter_flag2){
//							if (ter_special == 5)
//								poison_pc(which_pc,ter_flag1);
//								else disease_pc(which_pc,ter_flag1);
//							}
//					}
			break;
		case TER_SPEC_CALL_SPECIAL:{
			short spec_type = 0;
			if(ter_flag2 == 3){
				if(mode == 1 || (mode == 2 && which_combat_type == 1)) spec_type = 2; else spec_type = 1;
			}else if(ter_flag2 == 2 && (mode == 0 || (mode == 2 && which_combat_type == 0)))
				spec_type = 1;
			else if(ter_flag2 == 1 && (mode == 1 || (mode == 2 && which_combat_type == 1)))
				spec_type = 2;
			run_special(mode,spec_type,ter_flag1,where_check,&s1,&s2,&s3);
					if (s1 > 0)
						can_enter = false;
			break;
		}
//		case 13: // global special
//			run_special(mode,0,ter_flag1,where_check,&s1,&s2,&s3);
//					if (s1 > 0)
//						can_enter = false;
//			break;

			
		// Locked doors
		case TER_SPEC_UNLOCKABLE:
			if (is_combat()) {  // No lockpicking in combat
				add_string_to_buf("  Can't enter: It's locked.");
				break;
				}
				
			// See what party wants to do.
			choice = fancy_choice_dialog(993,0);
					
			can_enter = false;
			if (choice == 1)
				break;
			if ((door_pc = select_pc(1,0)) < 6) {
				if (choice == 2)
					pick_lock(where_check,door_pc);				
					else bash_door(where_check,door_pc);
				}		
			break;
			

		}
		
	// Action may change terrain, so update what's been seen
	if (is_town())
		update_explored(univ.town.p_loc);
	if (is_combat())
		update_explored(pc_pos[current_pc]);

	return can_enter;
}

// This procedure find the effects of fields that would affect a PC who moves into
// a space or waited in that same space
void check_fields(location where_check,short mode,short which_pc)
//mode; // 0 - out 1 - town 2 - combat
{
	short r1,i;
	
	if (is_out())
		return;
	if (is_town())
		fast_bang = 1;
	if (univ.town.is_fire_wall(where_check.x,where_check.y)) {
			add_string_to_buf("  Fire wall!               ");
			r1 = get_ran(1,1,6) + 1;
//			if (mode < 2)
//				hit_party(r1,1);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_FIRE,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,0,r1,5);	
		}
	if (univ.town.is_force_wall(where_check.x,where_check.y)) {
			add_string_to_buf("  Force wall!               ");
			r1 = get_ran(2,1,6);
//			if (mode < 2)
//				hit_party(r1,3);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_MAGIC,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,1,r1,12);	
		}
	if (univ.town.is_ice_wall(where_check.x,where_check.y)) {
			add_string_to_buf("  Ice wall!               ");
			r1 = get_ran(2,1,6);
//			if (mode < 2)
//				hit_party(r1,5);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_COLD,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,4,r1,7);	
		}
	if (univ.town.is_blade_wall(where_check.x,where_check.y)) {
			add_string_to_buf("  Blade wall!               ");
			r1 = get_ran(4,1,8);
//			if (mode < 2)
//				hit_party(r1,0);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_WEAPON,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,3,r1,2);	
		}
	if (univ.town.is_quickfire(where_check.x,where_check.y)) {
			add_string_to_buf("  Quickfire!               ");
			r1 = get_ran(2,1,8);
//			if (mode < 2)
//				hit_party(r1,1);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_FIRE,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,0,r1,5);	
		}
	if (univ.town.is_scloud(where_check.x,where_check.y)) {
		add_string_to_buf("  Stinking cloud!               ");
		if (mode < 2) {
			suppress_stat_screen = true;
			for (i = 0; i < 6; i++) {
				r1 = get_ran(1,2,3);
				curse_pc(i,r1);
				}
			suppress_stat_screen = false;
			put_pc_screen();
			}
			else curse_pc(current_pc,get_ran(1,2,3));
		}
	if (univ.town.is_sleep_cloud(where_check.x,where_check.y)) {
		add_string_to_buf("  Sleep cloud!               ");
		if (mode < 2) {
			suppress_stat_screen = true;
			for (i = 0; i < 6; i++) {
				sleep_pc(i,3,11,0);
				}
			suppress_stat_screen = false;
			put_pc_screen();
			}
			else sleep_pc(current_pc,3,11,0);
		}
	if (univ.town.is_fire_barr(where_check.x,where_check.y)) {
			add_string_to_buf("  Magic barrier!               ");
			r1 = get_ran(2,1,10);
			if (mode < 2)
				hit_party(r1,DAMAGE_MAGIC);
			if (mode == 2)
				damage_pc(which_pc,r1,DAMAGE_MAGIC,MONSTER_TYPE_UNKNOWN,0);
				if (overall_mode < MODE_COMBAT)
					boom_space(univ.party.p_loc,overall_mode,1,r1,12);		
		}
	fast_bang = 0;
}

void use_spec_item(short item)
{
	short i,j,k;
	location null_loc;
	
	run_special(8,0,scenario.special_item_special[item],loc(),&i,&j,&k);

}


void use_item(short pc,short item)
{
	bool take_charge = true,inept_ok = false;
	short abil,level,i,j,item_use_code,str,type,which_stat,r1;
	char to_draw[60];
	location user_loc;
cCreature *which_m;
effect_pat_type s = {{{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,1,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0},
						{0,0,0,0,0,0,0,0,0}}};	
	abil = ADVEN[pc].items[item].ability;
	level = ADVEN[pc].items[item].item_level;
	
	item_use_code = abil_chart[abil];
	if (item_use_code >= 10) {
		item_use_code -= 10;
		inept_ok = true;
		}

	if (is_out())
		user_loc = univ.party.p_loc;
	if (is_town())
		user_loc = univ.town.p_loc;
	if (is_combat())
		user_loc = pc_pos[current_pc];

	if (item_use_code == 4) {
		add_string_to_buf("Use: Can't use this item.       ");
		take_charge = false;
		}
	if ((ADVEN[pc].traits[11] == true) && (inept_ok == false)){
		add_string_to_buf("Use: Can't - magically inept.       ");
		take_charge = false;
		}
	
	if (take_charge == true) {
			if ((overall_mode == MODE_OUTDOORS) && (item_use_code > 0) && (item_use_code != 5)) {
				add_string_to_buf("Use: Not while univ.out.outdoors.         ");
				take_charge = false;
				}
			if ((overall_mode == MODE_TOWN) && (item_use_code == 1)) {
				add_string_to_buf("Use: Not while in town.         ");
				take_charge = false;
				}
			if ((overall_mode == MODE_COMBAT) && (item_use_code == 2)) {
				add_string_to_buf("Use: Not in combat.         ");
				take_charge = false;
				}
			if ((overall_mode != 0) && (item_use_code == 5)){
				add_string_to_buf("Use: Only univ.out.outdoors.           ");
				take_charge = false;
				}
		}
	if (take_charge == true) {
		if (!ADVEN[pc].items[item].is_ident())
			sprintf((char *) to_draw, "Use: %s",ADVEN[pc].items[item].name.c_str());
			else sprintf((char *) to_draw, "Use: %s",ADVEN[pc].items[item].full_name.c_str());
		add_string_to_buf((char *) to_draw);

		if ((ADVEN[pc].items[item].variety == 7) &&
		      (ADVEN[pc].items[item].graphic_num >= 50) && (ADVEN[pc].items[item].graphic_num <= 52))
		      	play_sound(56);
		
		str = ADVEN[pc].items[item].ability_strength;
		store_item_spell_level = str * 2 + 1;
		type = ADVEN[pc].items[item].magic_use_type;
				
		switch (abil) {
			case 70: // poison weapon
				take_charge = poison_weapon(pc,str,0);
				break;				
			case 71: case 73:  case 74: case 75: case 76: case 78: case 80:
				switch (abil) {
					case 71: 
						play_sound(4);
						which_stat = 1;
						if (type % 2 == 1) {
							ASB("  You feel awkward."); str = str * -1;}
							else ASB("  You feel blessed.");
						break;
					case 73: 
						play_sound(75);
						which_stat = 3;
						if (type % 2 == 1) {
							ASB("  You feel sluggish."); str = str * -1;}
							else ASB("  You feel speedy.");
						break;
					case 74:
						play_sound(68);
						which_stat = 4;
						if (type % 2 == 1) {
							ASB("  You feel odd."); str = str * -1;}
							else ASB("  You feel protected.");
						break;
					case 75: 
						play_sound(51);
						which_stat = 5;
						if (type % 2 == 1) {
							ASB("  You feel odd."); str = str * -1;}
							else ASB("  You feel protected.");
						break;
					case 76: 
						which_stat = 6;
						if (type % 2 == 1)
							ASB("  You feel sticky.");
							else {
								ASB("  Your skin tingles."); str = str * -1;}
						break;
					case 78: 
						play_sound(43);
						which_stat = 8;
						if (type % 2 == 1) {
							ASB("  You feel exposed."); str = str * -1;}
							else ASB("  You feel obscure.");
						break;
					case 80: 
						play_sound(43);
						which_stat = 10;
						if (type % 2 == 1) {
							ASB("  You feel dull."); str = str * -1;}
							else ASB("  You start to glow slightly.");
						break;
					}
				if (type > 1)
					affect_party(which_stat,str);
					else affect_pc(pc,which_stat,str);
				break;
			case 72:
				switch (type) {
					case 0: ASB("  You feel better."); cure_pc(pc,str); break;
					case 1: ASB("  You feel ill."); poison_pc(pc,str); break;
					case 2: ASB("  You all feel better."); cure_party(str); break;
					case 3: ASB("  You all feel ill."); poison_party(str); break;
					}
				break;
			case 77:
				switch (type) {
					case 0: ASB("  You feel healthy."); affect_pc(pc,7,-1 * str); break;
					case 1: ASB("  You feel sick."); disease_pc(pc,str); break;
					case 2: ASB("  You all feel healthy."); affect_party(7,-1 * str); break;
					case 3: ASB("  You all feel sick."); for (i = 0; i < 6; i++) disease_pc(i,str); break;
					}
				break;
			case 79:
				switch (type) {
					case 0: ASB("  You feel clear headed."); affect_pc(pc,9,-1 * str); break;
					case 1: ASB("  You feel confused."); dumbfound_pc(pc,str); break;
					case 2: ASB("  You all feel clear headed."); affect_party(9,-1 * str); break;
					case 3: ASB("  You all feel confused."); for (i = 0; i < 6; i++) dumbfound_pc(i,str); break;
					}
				break;
			case 81:
				switch (type) {
					case 0: ASB("  You feel alert."); affect_pc(pc,11,-1 * str); break;
					case 1: ASB("  You feel very tired."); sleep_pc(pc,str + 1,11,200); break;
					case 2: ASB("  You all feel alert."); affect_party(11,-1 * str); break;
					case 3: ASB("  You all feel very tired."); for (i = 0; i < 6; i++) sleep_pc(i,str + 1,11,200); break;
					}
				break;
			case 82:
				switch (type) {
					case 0: ASB("  You find it easier to move."); affect_pc(pc,12,-1 * str * 100); break;
					case 1: ASB("  You feel very stiff."); sleep_pc(pc,str * 20 + 10,12,200); break;
					case 2: ASB("  You all find it easier to move."); affect_party(12,-1 * str * 100); break;
					case 3: ASB("  You all feel very stiff."); for (i = 0; i < 6; i++) sleep_pc(i,str * 20 + 10,12,200); break;
					}
				break;
			case 83:
				switch (type) {
					case 0: ASB("  Your skin tingles pleasantly."); affect_pc(pc,13,-1 * str); break;
					case 1: ASB("  Your skin burns!"); acid_pc(pc,str); break;
					case 2: ASB("  You all tingle pleasantly."); affect_party(13,-1 * str); break;
					case 3: ASB("  Everyone's skin burns!"); for (i = 0; i < 6; i++) acid_pc(i,str); break;
					}
				break;
			case 84:
				switch (type) {
					case 0: 
					case 1: ASB("  You feel wonderful!"); heal_pc(pc,str * 20); affect_pc(pc,1,str); break;
					case 2:
					case 3: ASB("  Everyone feels wonderful!"); for (i = 0; i < 6; i++) {
								heal_pc(i,str * 20); affect_pc(i,1,str); } break;
					}
				break;
			case 85:
				switch (type) {
					case 0: ASB("  You feel much smarter."); award_xp(pc,str * 5); break;
					case 1: ASB("  You feel forgetful."); drain_pc(pc,str * 5); break;
					case 2: ASB("  You all feel much smarter."); award_party_xp(str * 5); break;
					case 3: ASB("  You all feel forgetful."); for (i = 0; i < 6; i++) drain_pc(i,str * 5); break;
					}
				break;
			case 86:
				play_sound(68);
				switch (type) {
					case 0: ASB("  You feel much smarter."); ADVEN[pc].skill_pts += str; break;
					case 1: ASB("  You feel forgetful."); ADVEN[pc].skill_pts = max(0,ADVEN[pc].skill_pts - str); break;
					case 2: ASB("  You all feel much smarter."); for (i = 0; i < 6; i++) ADVEN[i].skill_pts += str; break;
					case 3: ASB("  You all feel forgetful."); for (i = 0; i < 6; i++) ADVEN[i].skill_pts = max(0,ADVEN[i].skill_pts - str); break;
					}
				break;
			case 87:
				switch (type) {
					case 0: ASB("  You feel better."); heal_pc(pc,str * 20); break;
					case 1: ASB("  You feel sick."); damage_pc(pc,20 * str,DAMAGE_UNBLOCKABLE,MONSTER_TYPE_HUMAN,0); break;
					case 2: ASB("  You all feel better."); heal_party(str * 20); break;
					case 3: ASB("  You all feel sick."); hit_party(20 * str,DAMAGE_UNBLOCKABLE); break;
					}
				break;
			case 88:
				switch (type) {
					case 0: ASB("  You feel energized."); restore_sp_pc(pc,str * 5); break;
					case 1: ASB("  You feel drained."); ADVEN[pc].cur_sp = max(0,ADVEN[pc].cur_sp - str * 5); break;
					case 2: ASB("  You all feel energized."); restore_sp_party(str * 5); break;
					case 3: ASB("  You all feel drained."); for (i = 0; i < 6; i++) ADVEN[i].cur_sp = max(0,ADVEN[i].cur_sp - str * 5); break;
					}
				break;
			case 89:
				switch (type) {
					case 0: 
					case 1: ASB("  You feel terrible."); drain_pc(pc,str * 5); damage_pc(pc,20 * str,DAMAGE_UNBLOCKABLE,MONSTER_TYPE_HUMAN,0); disease_pc(pc,2 * str); dumbfound_pc(pc,2 * str); break;
					case 2: 
					case 3: ASB("  You all feel terrible."); for (i = 0; i < 6; i++) {
					drain_pc(i,str * 5); damage_pc(i,20 * str,DAMAGE_UNBLOCKABLE,MONSTER_TYPE_HUMAN,0); disease_pc(i,2 * str); dumbfound_pc(i,2 * str);} break;
					}
				break;
			case 90:
				ASB("  You have more light."); increase_light(50 * str);
				break;
			case 91:
				ASB("  Your footsteps become quieter."); PSD[SDF_PARTY_STEALTHY] += 5 * str;
				break;
			case 92:
				ASB("  You feel chilly."); PSD[SDF_PARTY_FIREWALK] += 2 * str;
				break;
			case 93:
				if (PSD[SDF_PARTY_FLIGHT] > 0) {
					add_string_to_buf("  Not while already flying.          ");
					break;
					}
				if (univ.party.in_boat >= 0)
					add_string_to_buf("  Leave boat first.             "); 
				else if (univ.party.in_horse >= 0)////
					add_string_to_buf("  Leave horse first.             "); 
				else {
					ASB("  You rise into the air!"); PSD[SDF_PARTY_FLIGHT] += str;
					}
				break;
			case 94:
				switch (type) {
					case 0: 
					case 1: ASB("  You feel wonderful."); heal_pc(pc,200); cure_pc(pc,8); break;
					case 2:
					case 3: ASB("  You all feel wonderful."); heal_party(200); cure_party(8); break;
					}
				break;
				
			// spell effects
			case 110:
				add_string_to_buf("  It fires a bolt of flame.");
				start_spell_targeting(1011);
				break;
			case 111:
				add_string_to_buf("  It shoots a fireball.         ");
				start_spell_targeting(1022);
				break;
			case 112:
				add_string_to_buf("  It shoots a huge fireball. ");
				start_spell_targeting(1040);
				break;
			case 113:
				add_string_to_buf("  It shoots a black ray.  ");
				start_spell_targeting(1048);
				break;
			case 114:
				add_string_to_buf("  It fires a ball of ice.   ");
				start_spell_targeting(1031);
				break;
			case 115:
				add_string_to_buf("  It fires a purple ray.   ");
				start_spell_targeting(1012);
				break;
			case 116:
				add_string_to_buf("  The ground shakes!        ");
				do_shockwave(pc_pos[current_pc]);
				break;
			case 117:
				add_string_to_buf("  It shoots a white ray.   ");
				start_spell_targeting(1132);
				break;		
			case 118:
				add_string_to_buf("  It shoots a golden ray.   ");
				start_spell_targeting(1155);
				break;	
			case 119:
				if (summon_monster(str,user_loc,50,2) == false)
					add_string_to_buf("  Summon failed.");
				break;
			case 120:
				r1 = get_ran(6,1,4);
				for (i = 0; i < get_ran(1,3,5); i++)
					if (summon_monster(str,user_loc,r1,2) == false)
						add_string_to_buf("  Summon failed.");
				break;
			case 121:
				add_string_to_buf("  Acid sprays from the tip!   ");
				start_spell_targeting(1068);
				break;
			case 122:
				add_string_to_buf("  It creates a cloud of gas.   ");
				start_spell_targeting(1066);
				break;
			case 123:
				add_string_to_buf("  It creates a shimmering cloud.   ");
				start_spell_targeting(1019);
				break;
			case 124:
				add_string_to_buf("  A green ray emerges.        ");
				start_spell_targeting(1030);
				break;
			case 125:
				add_string_to_buf("  Sparks fly.");
				start_spell_targeting(1044);
				break;
			case 126:
				add_string_to_buf("  It shoots a silvery beam.   ");
				start_spell_targeting(1069);
				break;
			case 127:
				add_string_to_buf("  It explodes!");
				start_spell_targeting(1065);
				break;
			case 128:
				add_string_to_buf("  It shoots a fiery red ray.   ");
				start_spell_targeting(1062);
				break;
			case 129:
				add_string_to_buf("Fire pours out!");
				make_quickfire(user_loc.x,user_loc.y);
				break;
			case 130: 
				ASB("It throbs, and emits odd rays.");
				for (i = 0; i < univ.town->max_monst(); i++) {
						if ((univ.town.monst.dudes[i].active != 0) && (univ.town.monst.dudes[i].attitude % 2 == 1) 
						 && (dist(pc_pos[current_pc],univ.town.monst.dudes[i].cur_loc) <= 8)
						 && (can_see(pc_pos[current_pc],univ.town.monst.dudes[i].cur_loc,0) < 5)) {
								which_m = &univ.town.monst.dudes[i];
								charm_monst(which_m,0,0,8);
							}
						}
				break;
			case 131:
				if (univ.town->specials2 & 1) {
					add_string_to_buf("  It doesn't work.");
					break;
					}
				add_string_to_buf("  You have a vision.            ");
				for (i = 0; i < univ.town->max_dim(); i++)
					for (j = 0; j < univ.town->max_dim(); j++)
						make_explored(i,j);
				clear_map();
				break;
			case 132:
				add_string_to_buf("  It fires a blinding ray.");
				add_string_to_buf("  Target spell.    ");
				overall_mode = MODE_TOWN_TARGET;
				current_pat = s;
				set_town_spell(1041,current_pc);
			break;
			case 133:
				add_string_to_buf("  It shoots a blue sphere.   ");
				start_spell_targeting(1064);
				break;
			case 134:
				add_string_to_buf("  It fires a lovely, sparkling beam.");
				start_spell_targeting(1117);
				break;
			case 135:
				add_string_to_buf("  Your hair stands on end.   ");
				start_spell_targeting(1051);
				break;
			}
// Special spells:
//   62 - Carrunos
//	 63 - Summon Rat
//	 64 - Ice Wall Balls
//	 65 - Goo Bomb
//   66 - Foul Vapors			
//   67 - Sleep cloud
//	 68 - spray acid
//	 69 - paralyze
//   70 - mass sleep
		}
	
	put_pc_screen();
	if ((take_charge == true) && (ADVEN[pc].items[item].charges > 0))
		remove_charge(pc,item);
	if (take_charge == false) {
		draw_terrain(0);
		put_item_screen(stat_window,0);
		}
}

// Returns true if an action is actually carried out. This can only be reached in town.
bool use_space(location where)
{
	ter_num_t ter;
	short i;
	location from_loc,to_loc;
	

	ter = univ.town->terrain(where.x,where.y);
	from_loc = univ.town.p_loc;

	add_string_to_buf("Use...");
	
	if (univ.town.is_web(where.x,where.y)) {
		add_string_to_buf("  You clear the webs.");
		univ.town.set_web(where.x,where.y,false);
		return true;
		}
	if (univ.town.is_crate(where.x,where.y)) {
		to_loc = push_loc(from_loc,where);
		if (from_loc == to_loc) {
			add_string_to_buf("  Can't push crate.");
			return false;
			}
		add_string_to_buf("  You push the crate.");
		univ.town.set_crate((short) where.x,(short) where.y,false);
		univ.town.set_crate((short) to_loc.x,(short) to_loc.y,true);
		for (i = 0; i < NUM_TOWN_ITEMS; i++)
			if ((univ.town.items[i].variety > 0) && (univ.town.items[i].item_loc == where)
			 && (univ.town.items[i].is_contained()))
			 	univ.town.items[i].item_loc = to_loc;
		}
	if (univ.town.is_barrel(where.x,where.y)) {
		to_loc = push_loc(from_loc,where);
		if (from_loc == to_loc) {
			add_string_to_buf("  Can't push barrel.");
			return false;
			}
		add_string_to_buf("  You push the barrel.");
		univ.town.set_barrel((short) where.x,(short) where.y,false);
		univ.town.set_barrel((short) to_loc.x,(short) to_loc.y,true);
		for (i = 0; i < NUM_TOWN_ITEMS; i++)
			if ((univ.town.items[i].variety > 0) && (univ.town.items[i].item_loc == where)
			 && (univ.town.items[i].is_contained()))
			 	univ.town.items[i].item_loc = to_loc;
		}
		
	if (scenario.ter_types[ter].special == TER_SPEC_CHANGE_WHEN_USED) {
			if (where == from_loc) {
				add_string_to_buf("  Not while on space.");
				return false;
				}
			add_string_to_buf("  OK.");
			alter_space(where.x,where.y,scenario.ter_types[ter].flag1);
			play_sound(scenario.ter_types[ter].flag2);
			return true;
	}else if (scenario.ter_types[ter].special == TER_SPEC_CALL_SPECIAL_WHEN_USED){ // call special
		short spec_type = 0;
		if(scenario.ter_types[ter].flag2 == 3){
			if((is_town() || (is_combat() && which_combat_type == 1))) spec_type = 2; else spec_type = 1;
		}else if(scenario.ter_types[ter].flag2 == 1 && (is_town() || (is_combat() && which_combat_type == 1)))
			spec_type = 2;
		else if(scenario.ter_types[ter].flag2 == 2 && (is_out() || (is_combat() && which_combat_type == 1)))
			spec_type = 1;
		run_special(17,spec_type,scenario.ter_types[ter].flag1,where,&i,&i,&i);
	}
	add_string_to_buf("  Nothing to use.");

	return false;
}

// Note ... if this is a container, the code must first process any specials. If
//specials return false, can't get items inside. If true, can get items inside.
// Can't get items out in combat.
bool adj_town_look(location where)
{
	char terrain;
	bool can_open = true,item_there = false,got_special = false;
	short i = 0,s1 = 0, s2 = 0, s3 = 0;

	for (i = 0; i < NUM_TOWN_ITEMS; i++) 
		if ((univ.town.items[i].variety > 0) && (univ.town.items[i].is_contained()) &&
			(where == univ.town.items[i].item_loc))
				item_there = true;

	terrain = univ.town->terrain(where.x,where.y);
	if (univ.town.is_special(where.x,where.y)) {// && (get_blockage(terrain) > 0)) {
		if (adjacent(univ.town.p_loc,where) == false)
			add_string_to_buf("  Not close enough to search.");
			else {
				for (i = 0; i < 50; i++)
					if (where == univ.town->special_locs[i]) {
						if (get_blockage(univ.town->terrain(where.x,where.y)) > 0) { 
						// tell party you find something, if looking at a space they can't step in
							//if (univ.town.town.special_id[i] >= 10) 
							//	add_string_to_buf("  Search: You find something!          ");
							//	else if (PSD[univ.town.town_num][univ.town.town.special_id[i]] < 20)
							//		add_string_to_buf("  Search: You find something!          ");
							}
						//call special can_open = town_specials(i,univ.town.town_num);
						
						run_special(4,2,univ.town->spec_id[i],where,&s1,&s2,&s3);
						if (s1 > 0)
							can_open = false;
						got_special = true;
						}
				put_item_screen(stat_window,0);	
				}
		}
	if ((is_container(where)) && (item_there == true) && (can_open == true)) {
		get_item(where,6,true);
		}
	else switch (terrain) {
		case 22: case 23:
			add_string_to_buf("  (Use this space to do something");
			add_string_to_buf("  with it.)");
			break;
		
		default:
			if (got_special == false)
				add_string_to_buf("  Search: You don't find anything.          ");
			return false;
		}
	return false;
}

// PSOE - place_special_outdoor_encounter
void PSOE(short str1a,short str1b,short str2a,short str2b,
	short which_special,unsigned char *stuff_done_val,short where_put)
// if always, stuff_done_val is NULL
{
	short i,j,graphic_num = 0;
	
	if (stuff_done_val != NULL) {
		if (*stuff_done_val > 0)
			return;
			else *stuff_done_val = 20;
		}
	for (i = 0; i < 18; i++)
		if (univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_id[i] == where_put) {
			for (j = 0; j < 7; j++) 
				if (univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[which_special].monst[j] > 0) {
					graphic_num = 400 + get_monst_picnum(univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[which_special].monst[j]);
					j = 7;
					}
			//display_strings( str1a, str1b, str2a, str2b,
			////	"Encounter!",57, graphic_num, 0);
			draw_terrain(0);
			pause(15);
			//if (univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[which_special].spec_code == 0)
			//	univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[which_special].spec_code = 1;
			//place_outd_wand_monst(univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_locs[i],
			//	univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[which_special]);
			
			i = 18;
			}
	draw_terrain(0);
	play_sound(61);
	//play_sound(0);
}

void out_move_party(char x,char y) 
{
	location l;
	
	l.x = x;l.y = y;
	l = local_to_global(l);
	univ.party.p_loc = l;
	center = l;
	update_explored(l);
}

void teleport_party(short x,short y,short mode)
// mode - 0 full teleport flash 1 no teleport flash 2 only fade flash
{
	short i;
	location l;
	
	if (is_combat())
		mode = 1;
	
	l = univ.town.p_loc;
	update_explored(l);
	
	if (mode != 1) {
		start_missile_anim();
		for (i = 0; i < 9; i++)
			add_explosion(l,-1,1,1,0,0);
		do_explosion_anim(5,1);
		}
	if (mode != 1)
		can_draw_pcs = false;
	if (mode != 1) {
		do_explosion_anim(5,2);
		end_missile_anim();	
		}
	center.x = x; center.y = y;
	if (is_combat()) {
		pc_pos[current_pc].x = x;pc_pos[current_pc].y = y;
		}
	l.x = x; l.y = y;
	univ.town.p_loc.x = x;
	univ.town.p_loc.y = y;
	update_explored(l);
	draw_terrain(0);
	
	if (mode == 0) {	
		start_missile_anim();
		for (i = 0; i < 14; i++)
			add_explosion(center,-1,1,1,0,0);
		do_explosion_anim(5,1);
		}
	can_draw_pcs = true;
	if (mode == 0) {	
		do_explosion_anim(5,2);
		end_missile_anim();
		}	
	draw_map(modeless_dialogs[5],5);
}


void fade_party()
{
	short i;
	location l;
	
	l = univ.town.p_loc;	
	start_missile_anim();
	for (i = 0; i < 14; i++)
		add_explosion(l,-1,1,1,0,0);
	do_explosion_anim(5,1);
	univ.town.p_loc.x = 100;
	univ.town.p_loc.y = 100;
	do_explosion_anim(5,2);
	end_missile_anim();	
}

void change_level(short town_num,short x,short y)
{
	location l;
	
	if ((town_num < 0) || (town_num >= scenario.num_towns)) {
		give_error("The scenario special encounter tried to put you into a town that doesn't exist.","",0);
		return;
		}
	l.x = x; l.y = y;

	force_town_enter(town_num,l);
	end_town_mode(1,l);
	start_town_mode(town_num,9);
}


// Damaging and killing monsters needs to be here because several have specials attached to them.
bool damage_monst(short which_m, short who_hit, short how_much, short how_much_spec, eDamageType dam_type, short sound_type)
//short which_m, who_hit, how_much, how_much_spec;  // 6 for who_hit means dist. xp evenly  7 for no xp
//short dam_type;  // 0 - weapon   1 - fire   2 - poison   3 - general magic   4 - unblockable  5 - cold 
 				 // 6 - demon 7 - undead  
 				 // 9 - marked damage, from during anim mode
 				 //+10 = no_print
 				 // 100s digit - damage sound for boom space
{
	cCreature *victim;
	short r1,which_spot;
	location where_put;
	
	bool do_print = true;
	char resist;

	//print_num(which_m,(short)univ.town.monst.dudes[which_m].m_loc.x,(short)univ.town.monst.dudes[which_m].m_loc.y);

	if (univ.town.monst.dudes[which_m].active == 0) return false;
	
	//sound_type = dam_type / 100;
	//dam_type = dam_type % 100;

	if (dam_type >= DAMAGE_MARKED) { // note: MARKED here actually means NO_PRINT
		do_print = false;
		dam_type -= DAMAGE_MARKED;
		}

	if (sound_type == 0) {
		if ((dam_type == 1) || (dam_type == 4) )
			sound_type = 5;		
		if 	(dam_type == 5)
			sound_type = 7;	
		if 	(dam_type == 3)
			sound_type = 12;	
		if 	(dam_type == 2)
			sound_type = 11;	
		}

		
	victim = &univ.town.monst.dudes[which_m];	
	resist = victim->m_d.immunities;

	if (dam_type == 3) {
		if (resist & 1)
			how_much = how_much / 2;
		if (resist & 2)
			how_much = 0;		
		}
	if (dam_type == 1) {
		if (resist & 4)
			how_much = how_much / 2;
		if (resist & 8)
			how_much = 0;		
		}
	if (dam_type == 5) {
		if (resist & 16)
			how_much = how_much / 2;
		if (resist & 32)
			how_much = 0;		
		}
	if (dam_type == 2) {
		if (resist & 64)
			how_much = how_much / 2;
		if (resist & 128)
			how_much = 0;		
		}
	
	// Absorb damage?
	if (((dam_type == 1) || (dam_type == 3) || (dam_type == 5))
	 && (victim->m_d.spec_skill == 26)) {
		victim->m_d.health += how_much;
		ASB("  Magic absorbed.");
		return false;
		}
		
	// Saving throw
	if (((dam_type == 1) || (dam_type == 5)) && (get_ran(1,0,20) <= victim->m_d.level))
		how_much = how_much / 2;
	if ((dam_type == 3) && (get_ran(1,0,24) <= victim->m_d.level))
		how_much = how_much / 2;

	// Rentar-Ihrno?
	if (victim->m_d.spec_skill == 36)
		how_much = how_much / 10;

		
	r1 = get_ran(1,0,(victim->m_d.armor * 5) / 4);
	r1 += victim->m_d.level / 4;
	if (dam_type == 0)
		how_much -= r1;
		
	if (boom_anim_active == true) {
		if (how_much < 0)
			how_much = 0;
		monst_marked_damage[which_m] += how_much;
		add_explosion(victim->cur_loc,how_much,0,(dam_type > 2) ? 2 : 0,14 * (victim->m_d.x_width - 1),18 * (victim->m_d.y_width - 1));
		if (how_much == 0)
			return false;
			else return true;
		}

	if (how_much <= 0) {
		if (is_combat())
			monst_spell_note(victim->number,7);
		if ((how_much <= 0) && ((dam_type == 0) || (dam_type == 6) || (dam_type == 7))) {
			draw_terrain(2);
			play_sound(2);
			}
//		sprintf ((char *) create_line, "  No damage.              ");
//		add_string_to_buf((char *) create_line);		
		return false;	
		}

	if (do_print == true)
		monst_damaged_mes(which_m,how_much,how_much_spec);
	victim->m_d.health = victim->m_d.health - how_much - how_much_spec;
	
	if (in_scen_debug == true)
		victim->m_d.health = -1;
		
	// splitting monsters
	if ((victim->m_d.spec_skill == 12) && (victim->m_d.health > 0)){
		where_put = find_clear_spot(victim->cur_loc,1);
		if (where_put.x > 0) 
			if ((which_spot = place_monster(victim->number,where_put)) < 90) {
				univ.town.monst.dudes[which_spot].m_d.health = victim->m_d.health;
				univ.town.monst.dudes[which_spot].number = victim->number;
				univ.town.monst.dudes[which_spot].start_attitude = victim->start_attitude;
				univ.town.monst.dudes[which_spot].start_loc = victim->start_loc;
				univ.town.monst.dudes[which_spot].mobility = victim->mobility;
				univ.town.monst.dudes[which_spot].time_flag = victim->time_flag;
				univ.town.monst.dudes[which_spot].extra1 = victim->extra1;
				univ.town.monst.dudes[which_spot].extra2 = victim->extra2;
				univ.town.monst.dudes[which_spot].spec1 = victim->spec1;
				univ.town.monst.dudes[which_spot].spec2 = victim->spec2;
				univ.town.monst.dudes[which_spot].spec_enc_code = victim->spec_enc_code;
				univ.town.monst.dudes[which_spot].time_code = victim->time_code;
				univ.town.monst.dudes[which_spot].monster_time = victim->monster_time;
				univ.town.monst.dudes[which_spot].personality = victim->personality;
				univ.town.monst.dudes[which_spot].special_on_kill = victim->special_on_kill;
				univ.town.monst.dudes[which_spot].facial_pic = victim->facial_pic;
				monst_spell_note(victim->number,27);
				}
		}
	if (who_hit < 7)
		univ.party.total_dam_done += how_much + how_much_spec;
		
	// Monster damages. Make it hostile.
	victim->active = 2;
	

	if (dam_type != 9) { // note special damage only gamed in hand-to-hand, not during animation
		if (party_can_see_monst(which_m) == true) {
			boom_space(victim->cur_loc,100,boom_gr[dam_type],how_much,sound_type);
			if (how_much_spec > 0)
				boom_space(victim->cur_loc,100,51,how_much_spec,5);
			}
			else {
				boom_space(victim->cur_loc,overall_mode, boom_gr[dam_type],how_much,sound_type);
				if (how_much_spec > 0)
					boom_space(victim->cur_loc,overall_mode,51,how_much_spec,5);			
				}
		}
		
	if (victim->m_d.health < 0) {
		monst_killed_mes(which_m);
		kill_monst(victim,who_hit);		
		}
		else {	
		if (how_much > 0)
			victim->m_d.morale = victim->m_d.morale - 1;
		if (how_much > 5)
			victim->m_d.morale = victim->m_d.morale - 1;
		if (how_much > 10)
			victim->m_d.morale = victim->m_d.morale - 1;
		if (how_much > 20)
			victim->m_d.morale = victim->m_d.morale - 2;
		}
		
	if ((victim->attitude % 2 != 1) && (who_hit < 7) && 
	 (processing_fields == false) && (monsters_going == false)) {
		add_string_to_buf("Damaged an innocent.           ");
		victim->attitude = 1;
		make_town_hostile();
		}
	if ((victim->attitude % 2 != 1) && (who_hit < 7) && 
	 ((processing_fields == true) && (PSD[SDF_HOSTILES_PRESENT] == 0))) {
		add_string_to_buf("Damaged an innocent.");
		victim->attitude = 1;
		make_town_hostile();
		}

	return true;
}

void kill_monst(cCreature *which_m,short who_killed)
{
	short xp,i,j,s1,s2,s3;	
	location l;

	switch (which_m->m_d.m_type) {
		case 0: case 3: case 4: case 5: case 6:  
			if (( which_m->number == 38) || 
				( which_m->number == 39))
				i = 4;
				else if ( which_m->number == 45)
					i = 0;
					else i = get_ran(1,0,1); 
			play_sound(29 + i); break;
		 case 9: play_sound(29); break;
		 case 1: case 2: case 7: case 8: case 11:
			i = get_ran(1,0,1); play_sound(31 + i); break;
		default: play_sound(33); break;
		}
	
	// Special killing effects
	if (sd_legit(which_m->spec1,which_m->spec2) == true)
		PSD[which_m->spec1][which_m->spec2] = 1;
		
	run_special(12,2,which_m->special_on_kill,which_m->cur_loc,&s1,&s2,&s3);
	if (which_m->m_d.radiate_1 == 15)
		run_special(12,0,which_m->m_d.radiate_2,which_m->cur_loc,&s1,&s2,&s3);
	
	if ((in_scen_debug == false) && ((which_m->summoned >= 100) || (which_m->summoned == 0))) { // no xp for party-summoned monsters
		xp = which_m->m_d.level * 2;
		if (who_killed < 6)
			award_xp(who_killed,xp);
			else if (who_killed == 6)
				award_party_xp(xp / 6 + 1);
		if (who_killed < 7) {
			univ.party.total_m_killed++;
			i = max((xp / 6),1);
			award_party_xp(i);
			}
		l = which_m->cur_loc;
		place_glands(l,which_m->number);
		
		}
	if ((in_scen_debug == false) && (which_m->summoned == 0))
		place_treasure(which_m->cur_loc, which_m->m_d.level / 2, which_m->m_d.treasure, 0);
	
	i = which_m->cur_loc.x;
	j = which_m->cur_loc.y;
	switch (which_m->m_d.m_type) {
		case 7:	make_sfx(i,j,6); break;
		case 8:	if (which_m->number <= 59) make_sfx(i,j,7); break;
		case 10: case 12: make_sfx(i,j,4); break;
		case 11: make_sfx(i,j,8); break;
		default: make_sfx(i,j,1); break;
		}
	


	if (((is_town()) || (which_combat_type == 1)) && (which_m->summoned == 0)) {
		univ.party.m_killed[univ.town.num]++;
		}

	univ.party.total_m_killed++;
	
	which_m->spec1 = 0; // make sure, if this is a spec. activated monster, it won't come back

	which_m->active = 0;
}

// Pushes party and monsters around by moving walls and conveyor belts. 
//This is very fragils, and only hands a few cases. 
void push_things()////
{
	bool redraw = false;
	short i,k;
	ter_num_t ter;
	location l;
	
	if (is_out()) // TODO: Make these work outdoors
		return;
	if (belt_present == false)
		return;
	
	for (i = 0; i < univ.town->max_monst(); i++)
		if (univ.town.monst.dudes[i].active > 0) {
			l = univ.town.monst.dudes[i].cur_loc;
			ter = univ.town->terrain(l.x,l.y);
			switch (scenario.ter_types[ter].flag1) { // TODO: Implement the other 4 possible directions
				case DIR_N: l.y--; break;
				case DIR_E: l.x++; break;
				case DIR_S: l.y++; break;
				case DIR_W: l.x--; break;
				}
			if (l != univ.town.monst.dudes[i].cur_loc) {
				univ.town.monst.dudes[i].cur_loc = l;
				if ((point_onscreen(center,univ.town.monst.dudes[i].cur_loc) == true) || 
					(point_onscreen(center,l) == true))
						redraw = true;
				}
			}
	for (i = 0; i < NUM_TOWN_ITEMS; i++)
		if (univ.town.items[i].variety > 0) {
			l = univ.town.items[i].item_loc;
			ter = univ.town->terrain(l.x,l.y);
			switch (scenario.ter_types[ter].flag1) { // TODO: Implement the other 4 possible directions
				case DIR_N: l.y--; break;
				case DIR_E: l.x++; break;
				case DIR_S: l.y++; break;
				case DIR_W: l.x--; break;
				}
			if (l != univ.town.items[i].item_loc) {
				univ.town.items[i].item_loc = l;
				if ((point_onscreen(center,univ.town.items[i].item_loc) == true) || 
					(point_onscreen(center,l) == true))
						redraw = true;
				}
			}
	
	if (is_town()) {
		ter = univ.town->terrain(univ.town.p_loc.x,univ.town.p_loc.y);
		l = univ.town.p_loc;
		switch (scenario.ter_types[ter].flag1) { // TODO: Implement the other 4 possible directions
			case DIR_N: l.y--; break;
			case DIR_E: l.x++; break;
			case DIR_S: l.y++; break;
			case DIR_W: l.x--; break;
			}
		if (l != univ.town.p_loc) {
			ASB("You get pushed.");
			if (scenario.ter_types[ter].special == TER_SPEC_CONVEYOR)	
				draw_terrain(0);
			center = l;
			univ.town.p_loc = l;
			update_explored(l);
			ter = univ.town->terrain(univ.town.p_loc.x,univ.town.p_loc.y);
			draw_map(modeless_dialogs[5],5);
			if (univ.town.is_barrel(univ.town.p_loc.x,univ.town.p_loc.y)) {
				univ.town.set_barrel(univ.town.p_loc.x,univ.town.p_loc.y,false);
				ASB("You smash the barrel.");			
				}
			if (univ.town.is_crate(univ.town.p_loc.x,univ.town.p_loc.y)) {
				univ.town.set_crate(univ.town.p_loc.x,univ.town.p_loc.y,false);
				ASB("You smash the crate.");			
				}
			for (k = 0; k < NUM_TOWN_ITEMS; k++)
				if ((univ.town.items[k].variety > 0) && (univ.town.items[k].is_contained() == true)
				&& (univ.town.items[k].item_loc == univ.town.p_loc))
					univ.town.items[k].item_properties = univ.town.items[k].item_properties & 247;				
			redraw = true;
			}	
		}
	if (is_combat()) {
		for (i = 0; i < 6; i++)
			if (ADVEN[i].main_status == 1) {
				ter = univ.town->terrain(pc_pos[i].x,pc_pos[i].y);
				l = pc_pos[i];
				switch (scenario.ter_types[ter].flag1) { // TODO: Implement the other 4 possible directions
					case DIR_N: l.y--; break;
					case DIR_E: l.x++; break;
					case DIR_S: l.y++; break;
					case DIR_W: l.x--; break;
					}
				if (l != pc_pos[i]) {
					ASB("Someone gets pushed.");	
					ter = univ.town->terrain(l.x,l.y);
					if (scenario.ter_types[ter].special == TER_SPEC_CONVEYOR)	
						draw_terrain(0);
					pc_pos[i] = l;
					update_explored(l);
					draw_map(modeless_dialogs[5],5);
					if (univ.town.is_barrel(pc_pos[i].x,pc_pos[i].y)) {
						univ.town.set_barrel(pc_pos[i].x,pc_pos[i].y,false);
						ASB("You smash the barrel.");			
						}
					if (univ.town.is_crate(pc_pos[i].x,pc_pos[i].y)) {
						univ.town.set_crate(pc_pos[i].x,pc_pos[i].y,false);
						ASB("You smash the crate.");			
						}
					for (k = 0; k < NUM_TOWN_ITEMS; k++)
						if ((univ.town.items[k].variety > 0) && (univ.town.items[k].is_contained() == true)
						&& (univ.town.items[k].item_loc == pc_pos[i]))
							univ.town.items[k].item_properties = univ.town.items[k].item_properties & 247;		
					redraw = true;
					}				
				}
		}
	if (redraw == true) {
		print_buf();
		draw_terrain(0);
		}
}

void special_increase_age()
{
	unsigned short i;
	short s1,s2,s3;
	bool redraw = false,stat_area = false;
	location null_loc;
	
	for (i = 0; i < 8; i++)
		if ((univ.town->timer_spec_times[i] > 0) && (univ.party.age % univ.town->timer_spec_times[i] == 0)
			&& ((is_town() == true) || ((is_combat() == true) && (which_combat_type == 1)))) {
			run_special(9,2,univ.town->timer_specs[i],null_loc,&s1,&s2,&s3);
			stat_area = true;
			if (s3 > 0)
				redraw = true;
			}
	for (i = 0; i < 20; i++)
		if ((scenario.scenario_timer_times[i] > 0) && (univ.party.age % scenario.scenario_timer_times[i] == 0)) {
			run_special(10,0,scenario.scenario_timer_specs[i],null_loc,&s1,&s2,&s3);
			stat_area = true;
			if (s3 > 0)
				redraw = true;
			}
	for (i = 0; i < univ.party.party_event_timers.size(); i++) {
			if (univ.party.party_event_timers[i].time == 1) {
				if (univ.party.party_event_timers[i].global_or_town == 0)
					run_special(11,0,univ.party.party_event_timers[i].node_to_call,null_loc,&s1,&s2,&s3);
				else run_special(11,2,univ.party.party_event_timers[i].node_to_call,null_loc,&s1,&s2,&s3);
				univ.party.party_event_timers[i].time = 0;
				stat_area = true;
				if (s3 > 0)
					redraw = true;
				}
				else univ.party.party_event_timers[i].time--;
		}
	if (stat_area == true) {
		put_pc_screen();
		put_item_screen(stat_window,0);
		}
	if (redraw == true)
		draw_terrain(0);

}

// This is the big painful one, the main special engine
// which_mode - says when it was called
// 0 - out moving (a - 1 if blocked)
// 1 - town moving (a - 1 if blocked)
// 2 - combat moving (a - 1 if blocked)
// 3 - out looking (a - 1 if don't get items inside)  NOT USED!!!
// 4 - town looking (a - 1 if don't get items inside)
// 5 - entering town
// 6 - leaving town
// 7 - talking (a,b - numbers of strings to respond)
// 8 - using a special item
// 9 - town timer
// 10 - scen timer
// 11 - party countdown timer
// 12 - killed a monst
// 13 - encountering outdoor enc (a - 1 if no fight)
// 14 - winning outdoor enc
// 15 - fleeing outdoor enc
// 16 - ritual of sanct
// 17 - using space
// which_type - 0 - scen 1 - out 2 - town
// start spec - the number of the first spec to call
// a,b - 2 values that can be returned
// redraw - 1 if now need redraw
void run_special(short which_mode,short which_type,short start_spec,location spec_loc,short *a,short *b,short *redraw)
{
	short cur_spec,cur_spec_type,next_spec,next_spec_type;
	cSpecial cur_node;
	short num_nodes = 0;
	
	if (special_in_progress == true) {
		give_error("The scenario called a special node while processing another special encounter. The second special will be ignored.","",0);
		return;
		}
	special_in_progress = true;
	next_spec = start_spec;
	next_spec_type = which_type;
	current_pc_picked_in_spec_enc = -1;
	store_special_loc = spec_loc;
	if (end_scenario == true) {
		special_in_progress = false;
		return;
		}
	
	while (next_spec >= 0) {
	
		cur_spec = next_spec;
		cur_spec_type = next_spec_type;
		next_spec = -1;
		cur_node = get_node(cur_spec,cur_spec_type);

		//print_nums(1111,cur_spec_type,cur_node.type);

		if (cur_node.type == -1) { /// got an error 
			special_in_progress = false;
			return;
			}
		if ((cur_node.type >= 0) && (cur_node.type <= 27)) {
			general_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 50) && (cur_node.type <= 63)) {
			oneshot_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 80) && (cur_node.type <= 106)) {
			affect_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 130) && (cur_node.type <= 155)) {
			ifthen_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 170) && (cur_node.type <= 195)) {
			townmode_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 200) && (cur_node.type <= 218)) {
			rect_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
		if ((cur_node.type >= 225) && (cur_node.type <= 229)) {
			outdoor_spec(which_mode,cur_node,cur_spec_type,&next_spec,&next_spec_type,a,b,redraw);
			}
			
		num_nodes++;
		if (num_nodes >= 50) {
			give_error("A special encounter can be at most 50 nodes long. The 50th node was just processed. The encounter will now end.","",0);
			next_spec = -1;
			}
		}
	if (is_out())
		erase_out_specials();
		else erase_specials();
	special_in_progress = false;
}

cSpecial get_node(short cur_spec,short cur_spec_type)
{
	cSpecial dummy_node;
	
	dummy_node = scenario.scen_specials[0];
	dummy_node.type = -1;
	if (cur_spec_type == 0) {
		if (cur_spec != minmax(0,255,cur_spec)) {
			give_error("The scenario called a scenario special node out of range.","",0);
			return dummy_node;
			}
		return scenario.scen_specials[cur_spec];
		}
	if (cur_spec_type == 1) {
		if (cur_spec != minmax(0,59,cur_spec)) {
			give_error("The scenario called an outdoor special node out of range.","",0);
			return dummy_node;
			}
		return univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].specials[cur_spec];
		}
	if (cur_spec_type == 2) {
		if (cur_spec != minmax(0,99,cur_spec)) {
			give_error("The scenario called a town special node out of range.","",0);
			return dummy_node;
			}
		return univ.town->specials[cur_spec];
		}
	return dummy_node;
}

void general_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = false;
	Str255 str1 = "",str2 = "";
	short store_val = 0,i;
	cSpecial spec;
	short mess_adj[3] = {160,10,20};
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	switch (cur_node.type) {
		case 0: break; // null spec
		case 1:
			check_mess = true; setsd(cur_node.sd1,cur_node.sd2,cur_node.ex1a);
			break;	
		case 2:
			check_mess = true;
			setsd(cur_node.sd1,cur_node.sd2,
				PSD[cur_node.sd1][cur_node.sd2] + ((cur_node.ex1b == 0) ? 1 : -1) * cur_node.ex1a);
			break;	
		case 3:
			check_mess = true;break;	
		case 5:
			get_strs((char *) str1,(char *) str2, cur_spec_type,cur_node.m1 + mess_adj[cur_spec_type],
				cur_node.m2 + mess_adj[cur_spec_type]);
			if (cur_node.m1 >= 0)
				ASB((char *) str1);
			if (cur_node.m2 >= 0)
				ASB((char *) str2);
			break;	
		case 6:
			setsd(cur_node.sd1,cur_node.sd2,
				((PSD[cur_node.sd1][cur_node.sd2] == 0) ? 1 : 0) );
			check_mess = true;break;	
		case 7:
			if (is_out()) *next_spec = -1;
			if ((is_out()) && (spec.ex1a != 0) && (which_mode == 0)) {
				ASB("Can't go here while univ.out.outdoors.");
				*a = 1;
				}
			break;	
		case 8:
			if (is_town()) *next_spec = -1;
			if ((is_town()) && (spec.ex1a != 0) && (which_mode == 1)) {
				ASB("Can't go here while in town mode.");
				*a = 1;
				}
			break;	
		case 9:
			if (is_combat()) *next_spec = -1;
			if ((is_combat()) && (spec.ex1a != 0) && (which_mode == 2)) {
				ASB("Can't go here during combat.");
				*a = 1;
				}
			break;	
		case 10:
			if ((which_mode == 3) || (which_mode == 4)) *next_spec = -1;
			break;
		case 11:
			check_mess = true;
			if (which_mode < 3) {
				if (spec.ex1a != 0)
					*a = 1;
					else *a = 0;
				}
			break;
		case 12:
			check_mess = true; univ.party.age += spec.ex1a;
			break;
		case 13:
			check_mess = true;
			univ.party.start_timer(spec.ex1a, spec.ex1b, 0);
			break;
		case 14:
			play_sound(spec.ex1a);
			break;
		case 15:
			check_mess = true;
			if (spec.ex1a != minmax(0,29,spec.ex1a))
				give_error("Horse out of range.","",0);
				else univ.party.horses[spec.ex1a].property = (spec.ex2a == 0) ? 1 : 0;
			break;
		case 16:
			check_mess = true;
			if (spec.ex1a != minmax(0,29,spec.ex1a))
				give_error("Boat out of range.","",0);
				else univ.party.boats[spec.ex1a].property = (spec.ex2a == 0) ? 1 : 0;
			break;
		case 17:
			check_mess = true;
			if (spec.ex1a != minmax(0,scenario.num_towns - 1,spec.ex1a))
				give_error("Town out of range.","",0);
				else univ.party.can_find_town[spec.ex1a] = (spec.ex1b == 0) ? 0 : 1;
			*redraw = true;
			break;
		case 18:
			check_mess = true;
			if (spec.ex1a != minmax(1,10,spec.ex1a))
				give_error("Event code out of range.","",0);
				else if (univ.party.key_times[spec.ex1a] == 30000)
					univ.party.key_times[spec.ex1a] = calc_day();
			break;
		case 19:
			check_mess = true;
			if ((forced_give(spec.ex1a,ITEM_NO_ABILITY) == false) && ( spec.ex1b >= 0))
				*next_spec = spec.ex1b;
			break;
		case 20:
			for (i = 0; i < 144; i++)
				if (party_check_class(spec.ex1a,0) == true)
					store_val++;
			if (store_val == 0) {
				if ( spec.ex1b >= 0)
					*next_spec = spec.ex1b;
				}
				else {
					check_mess = true;
					give_gold(store_val * spec.ex2a,true);
					}
			break;
		case 21:
			*next_spec_type = 0;
			break;
		case 22:
			if (spec.sd1 != minmax(0,299,spec.sd1))
				give_error("Stuff Done flag out of range.","",0);
				else for (i = 0; i < 10; i++) PSD[spec.sd1][i] = spec.ex1a;
			break;
		case 23:
			if ((sd_legit(spec.sd1,spec.sd2) == false) || (sd_legit(spec.ex1a,spec.ex1b) == false))
				give_error("Stuff Done flag out of range.","",0);
				else PSD[spec.sd1][spec.sd2] = PSD[spec.ex1a][spec.ex1b];
			break;
		case 24:
			if (which_mode != 16)
				*next_spec = spec.ex1b;
			break;
		case 25:
					check_mess = true;
					univ.party.age += spec.ex1a;
					heal_party(spec.ex1b); restore_sp_party(spec.ex1b);
			break;
		case 26:
			if (which_mode != 13)
				break;
			*a = (spec.ex1a == 0) ? 1 : 0;
			break;
		case 27:
			end_scenario = true;
			break;
		}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
}

/*
void general_spec(short which_mode,special_node_type cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = false;
	Str255 str1 = "",str2 = "";
	short store_val = 0,i,j;
	special_node_type spec;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	switch (cur_node.type) {
		case :
			break;
		}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
}
*/

void oneshot_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = true,set_sd = true;
	Str255 strs[6] = {"","","","","",""};
	short i,j,buttons[3] = {-1,-1,-1};
	cSpecial spec;
	cItemRec store_i;
	location l;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	if ((sd_legit(spec.sd1,spec.sd2) == true) && (PSD[spec.sd1][spec.sd2] == 250)) {
		*next_spec = -1;
		return;
		}
	switch (cur_node.type) {
		case 50:
			if (forced_give(spec.ex1a,ITEM_NO_ABILITY) == false) {
				set_sd = false;
				if ( spec.ex2b >= 0)
					*next_spec = spec.ex2b;
				}
				else {
					give_gold(spec.ex1b,true);
					give_food(spec.ex2a,true);
					}
			break;
		case 51:
			if (spec.ex1a != minmax(0,49,spec.ex1a)) {
				give_error("Special item is out of range.","",0);
				set_sd = false;
				}
				else {
					univ.party.spec_items[spec.ex1a] = (spec.ex1b == 0) ? 1 : 0;
					}
			if (stat_window == 6)
				set_stat_window(6);
			*redraw = 1;
			break;
		case 52:
			set_sd = false;
			check_mess = false;
			break;
		case 53:
			check_mess = false;
			break;
		case 55: case 56: case 57:
			check_mess = false;
			if (spec.m1 < 0)
				break;
			for (i = 0; i < 3; i++)
				get_strs((char *) strs[i * 2],(char *) strs[i * 2 + 1],cur_spec_type,
					spec.m1 + i * 2 + spec_str_offset[cur_spec_type],spec.m1 + i * 2 + 1 + spec_str_offset[cur_spec_type]);
			if (spec.m2 > 0) 
				{buttons[0] = 1; buttons[1] = spec.ex1a; buttons[2] = spec.ex2a;
				if ((spec.ex1a >= 0) || (spec.ex2a >= 0)) buttons[0] = 20; }
			if (spec.m2 <= 0) {buttons[0] = spec.ex1a;buttons[1] = spec.ex2a;}
			if ((buttons[0] < 0) && (buttons[1] < 0)) {
				give_error("Dialog box ended up with no buttons.","",0);
				break;
				}
			switch (cur_node.type) {
				case 55: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2400,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic ,buttons) ; break;
				case 56: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2000,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic,buttons) ; break;
				case 57: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2000,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic ,buttons) ; break;
				}
			if (spec.m2 > 0) {
				if (i == 1) {
					if ((spec.ex1a >= 0) || (spec.ex2a >= 0)) {
						set_sd = false; 
						}
					}
				if (i == 2) *next_spec = spec.ex1b;
				if (i == 3) *next_spec = spec.ex2b;
				}
				else {
					if (i == 1) *next_spec = spec.ex1b;
					if (i == 2) *next_spec = spec.ex2b;
					}
			break;
		case 58: case 59: case 60:
			check_mess = false;
			if (spec.m1 < 0)
				break;
			for (i = 0; i < 3; i++)
				get_strs((char *) strs[i * 2],(char *) strs[i * 2 + 1],cur_spec_type,
					spec.m1 + i * 2 + spec_str_offset[cur_spec_type],spec.m1 + i * 2 + 1  + spec_str_offset[cur_spec_type]);
			buttons[0] = 20; buttons[1] = 19;
			//i = custom_choice_dialog(strs,spec.pic,buttons) ;
			switch (cur_node.type) {
				case 58: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2400,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic,buttons) ; break;
				case 59: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2000,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic,buttons) ; break;
				case 60: if (spec.pic >= 1000) i = custom_choice_dialog(strs,(spec.pic % 1000) + 2000,buttons) ;
					else i = custom_choice_dialog(strs,spec.pic,buttons) ; break;
				}
			if (i == 1) {set_sd = false; *next_spec = -1;}
				else {
					store_i = get_stored_item(spec.ex1a);
					if ((spec.ex1a >= 0) && (!give_to_party(store_i,true))) {
						set_sd = false; *next_spec = -1;
						}
						else {
							give_gold(spec.ex1b,true);
							give_food(spec.ex2a,true);
							if ((spec.m2 >= 0) && (spec.m2 < 50)) {
								if (univ.party.spec_items[spec.m2] == 0)
									ASB("You get a special item.");
								univ.party.spec_items[spec.m2] = 1;
								*redraw = true;
								if (stat_window == 6)
									set_stat_window(6);
								}
							if (spec.ex2b >= 0) *next_spec = spec.ex2b;
							}
					}
			break;
		case 61:
			if (spec.ex1a != minmax(0,3,spec.ex1a)) {
				give_error("Special outdoor enc. is out of range. Must be 0-3.","",0);
				set_sd = false;
				}
				else {
					l = global_to_local(univ.party.p_loc);			
					place_outd_wand_monst(l,
					univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[spec.ex1a],true);			
					}
			break;
		case 62:
			activate_monsters(spec.ex1a,0);
			break;
		case 63:
			check_mess = false;
			if ((spec.m1 >= 0) || (spec.m2 >= 0)) {
				get_strs((char *) strs[0],(char *) strs[1],
					cur_spec_type,
					spec.m1 + ((spec.m1 >= 0) ? spec_str_offset[cur_spec_type] : 0),
					spec.m2 + ((spec.m2 >= 0) ? spec_str_offset[cur_spec_type] : 0));
				buttons[0] = 3; buttons[1] = 2;
				i = custom_choice_dialog(strs,727,buttons);
				}
				else i = FCD(872,0); 
			if (i == 1) {set_sd = false; *next_spec = -1; *a = 1;}
				else {
					if (is_combat() == true)
						j = run_trap(current_pc,(eTrapType)spec.ex1a,spec.ex1b,spec.ex2a);
						else j = run_trap(7,(eTrapType)spec.ex1a,spec.ex1b,spec.ex2a);
					if (j == 0) {
						*a = 1; set_sd = false;
						}
					}
			break;
		}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
	if ((set_sd == true) && (sd_legit(spec.sd1,spec.sd2) == true)) 
		PSD[spec.sd1][spec.sd2] = 250;

}

void affect_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = true;
	short i,pc,r1;
	cSpecial spec;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	pc = current_pc_picked_in_spec_enc;


	if ((check_mess == true) && (cur_node.type != 80)) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}


	switch (cur_node.type) {
		case 80:
			if (spec.ex1a == 2)
				current_pc_picked_in_spec_enc = -1;
				else if (spec.ex1a == 1) {
					i = select_pc(0,0);
					if (i != 6)
						current_pc_picked_in_spec_enc = i;
					}
				else if (spec.ex1a == 0) {
					i = select_pc(1,0);
					if (i != 6)
						current_pc_picked_in_spec_enc = i;
					}
			if (i == 6)// && (spec.ex1b >= 0))
				*next_spec = spec.ex1b;
			break;
		case 81:{
			r1 = get_ran(spec.ex1a,1,spec.ex1b) + spec.ex2a;
			eDamageType dam_type = (eDamageType) spec.ex2b;
			if (pc < 0) {
				if(spec.pic == 1 && overall_mode == MODE_COMBAT)
					damage_pc(current_pc,r1,dam_type,MONSTER_TYPE_HUMAN,0);
				else hit_party(r1,dam_type);
			}
			else damage_pc(pc,r1,dam_type,MONSTER_TYPE_HUMAN,0);
			break;}
		case 82:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					ADVEN[i].cur_health = minmax(0,	ADVEN[i].max_health,
						ADVEN[i].cur_health + spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 83:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					ADVEN[i].cur_sp = minmax(0,	ADVEN[i].max_sp,
						ADVEN[i].cur_sp + spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 84:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) award_xp(i,spec.ex1a); else drain_pc(i,spec.ex1a);
					}
			break;
		case 85:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					ADVEN[i].skill_pts = minmax(0,	100,
						ADVEN[i].skill_pts + spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 86:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) {
						if ((ADVEN[i].main_status > MAIN_STATUS_ABSENT) && (ADVEN[i].main_status < MAIN_STATUS_SPLIT))
							ADVEN[i].main_status = MAIN_STATUS_ALIVE;
						}
						else switch(spec.ex1a){
								// When passed to kill_pc, the SPLIT party status actually means "no saving throw".
							case 0:
								kill_pc(i,MAIN_STATUS_SPLIT_DEAD);
							case 1:
								kill_pc(i,MAIN_STATUS_SPLIT_DUST);
							case 2:
								kill_pc(i,MAIN_STATUS_SPLIT_STONE);
						}
					}
			*redraw = 1;
			break;
		case 87:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) {
						cure_pc(i,spec.ex1a);
						}
						else poison_pc(i,spec.ex1a);
					}
			break;
		case 88:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) {
						slow_pc(i,-1 * spec.ex1a);
						}
						else slow_pc(i,spec.ex1a);
					}
			break;
		case 89:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,4,spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 90:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,5,spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 91:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,6,spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 92:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,7,spec.ex1a * ((spec.ex1b != 0) ? 1: -1));
			break;
		case 93:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,8,spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 94:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,1,spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 95:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i))
					affect_pc(i,9,spec.ex1a * ((spec.ex1b == 0) ? -1: 1));
			break;
		case 96:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) {
						affect_pc(i,11,-1 * spec.ex1a);
						}
						else sleep_pc(i,spec.ex1a,11,10);
					}
			break;
		case 97:
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) {
					if (spec.ex1b == 0) {
						affect_pc(i,12,-1 * spec.ex1a);
						}
						else sleep_pc(i,spec.ex1a,12,10);
					}
			break;
		case 98:
			if (spec.ex2a != minmax(0,18,spec.ex2a)) {
				give_error("Skill is out of range.","",0);
				break;
				}
			for (i = 0; i < 6; i++)
				if (((pc < 0) || (pc == i)) && (get_ran(1,1,100) < spec.pic))
					ADVEN[i].skills[spec.ex2a] = minmax(0, skill_max[spec.ex2a],
						ADVEN[i].skills[spec.ex2a] + spec.ex1a * ((spec.ex1b != 0) ? -1: 1));
			break;
		case 99:
			if (spec.ex1a != minmax(0,31,spec.ex1a)) {
				give_error("Mage spell is out of range (0 - 31). See docs.","",0);
				break;
				}
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) 
					ADVEN[i].mage_spells[spec.ex1a + 30] = true;
			break;
		case 100:
			if (spec.ex1a != minmax(0,31,spec.ex1a)) {
				give_error("Priest spell is out of range (0 - 31). See docs.","",0);
				break;
				}
			for (i = 0; i < 6; i++)
				if ((pc < 0) || (pc == i)) 
					ADVEN[i].priest_spells[spec.ex1a + 30] = true;
			break;
		case 101:
			if (spec.ex1b == 0)
				give_gold(spec.ex1a,true);
				else if (univ.party.gold < spec.ex1a)
					univ.party.gold = 0;
					else take_gold(spec.ex1a,false);
			break;
		case 102:
			if (spec.ex1b == 0)
				give_food(spec.ex1a,true);
				else if (univ.party.food < spec.ex1a)
					univ.party.food = 0;
					else take_food(spec.ex1a,false);
			break;
		case 103:
			if (spec.ex1a != minmax(0,19,spec.ex1a)) {
				give_error("Alchemy is out of range.","",0);
				break;
				}
			univ.party.alchemy[spec.ex1a] = true;
			break;
		case 104:
			r1 = (short) PSD[SDF_PARTY_STEALTHY];
			r1 = minmax(0,250,r1 + spec.ex1a);
			PSD[SDF_PARTY_STEALTHY] = r1;
			break;
		case 105:
			r1 = (short) PSD[SDF_PARTY_FIREWALK];
			r1 = minmax(0,250,r1 + spec.ex1a);
			PSD[SDF_PARTY_FIREWALK] = r1;
			break;
		case 106:
			if (univ.party.in_boat >= 0)
				add_string_to_buf("  Can't fly when on a boat. "); 
			else if (univ.party.in_horse >= 0)////
				add_string_to_buf("  Can't fly when on a horse.  "); 
			else {
				r1 = (short) PSD[SDF_PARTY_FLIGHT];
				r1 = minmax(0,250,r1 + spec.ex1a);
				PSD[SDF_PARTY_FLIGHT] = r1;
				}
			break;
		}
}

void ifthen_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = false;
	Str255 str1 = "",str2 = "",str3 = "";
	short i,j,k;
	cSpecial spec;
	location l;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	switch (cur_node.type) {
		case 130:
			if (sd_legit(spec.sd1,spec.sd2) == true) {
				if ((spec.ex1a >= 0) && (PSD[spec.sd1][spec.sd2] >= spec.ex1a))
					*next_spec = spec.ex1b;
					else if ((spec.ex2a >= 0) && (PSD[spec.sd1][spec.sd2] < spec.ex2a))
						*next_spec = spec.ex2b;
				}
			break;
		case 131:
			if (((is_town()) || (is_combat())) && (univ.town.num == spec.ex1a))
				*next_spec = spec.ex1b;
			break;
		case 132:
			if (get_ran(1,1,100) < spec.ex1a)
				*next_spec = spec.ex1b;
			break;
		case 133:
			if (spec.ex1a != minmax(0,49,spec.ex1a)) {
				give_error("Special item is out of range.","",0);		
				}
				else if (univ.party.spec_items[spec.ex1a] > 0)
					*next_spec = spec.ex1b;
			break;
		case 134:
			if ((sd_legit(spec.sd1,spec.sd2) == true) && (sd_legit(spec.ex1a,spec.ex1b) == true)) {
				if (PSD[spec.ex1a][spec.ex1b] < PSD[spec.sd1][spec.sd2])
					*next_spec = spec.ex2b;
				}
				else give_error("A Stuff Done flag is out of range.","",0);	
			break;
		case 135:
			if (((is_town()) || (is_combat())) && (univ.town->terrain(spec.ex1a,spec.ex1b) == spec.ex2a))
				*next_spec = spec.ex2b;
			break;
		case 136:
			l.x = spec.ex1a; l.y = spec.ex1b;
			l = local_to_global(l);
			if ((is_out()) && (univ.out[l.x][l.y] == spec.ex2a))
				*next_spec = spec.ex2b;
			break;
		case 137:
			if (univ.party.gold >= spec.ex1a)
				*next_spec = spec.ex1b;
			break;
		case 138:
			if (univ.party.food >= spec.ex1a)
				*next_spec = spec.ex1b;
			break;
		case 139:
			if (is_out())
				break;
			l.x = spec.ex1a; l.y = spec.ex1b;
			for (i = 0; i < NUM_TOWN_ITEMS; i++)
				if ((univ.town.items[i].variety > 0) && (univ.town.items[i].special_class == spec.ex2a)
					&& (l == univ.town.items[i].item_loc))
						*next_spec = spec.ex2b;
			break;
		case 140:
			if (party_check_class(spec.ex1a,1) == true)
				*next_spec = spec.ex1b;
			break;
		case 141:
			for (i = 0; i < 6; i++)
				if (ADVEN[i].main_status == 1)
					for (j = 0; j < 24; j++)
						if ((ADVEN[i].items[j].variety > 0) && (ADVEN[i].items[j].special_class == spec.ex1a)
							&& (ADVEN[i].equip[j] == true)) 
							*next_spec = spec.ex1b;			
			break;
		case 142:
			if (univ.party.gold >= spec.ex1a) {
				take_gold(spec.ex1a,true);
				*next_spec = spec.ex1b;
				}
			break;
		case 143:
			if (univ.party.food >= spec.ex1a) {
				take_food(spec.ex1a,true);
				*next_spec = spec.ex1b;
				}
			break;
		case 144:
			if (is_out())
				break;
			l.x = spec.ex1a; l.y = spec.ex1b;
			for (i = 0; i < NUM_TOWN_ITEMS; i++)
				if ((univ.town.items[i].variety > 0) && (univ.town.items[i].special_class == spec.ex2a)
					&& (l == univ.town.items[i].item_loc)) {
						*next_spec = spec.ex2b;
						*redraw = 1;
						univ.town.items[i].variety = ITEM_TYPE_NO_ITEM;
						}
			break;
		case 145:
			if (party_check_class(spec.ex1a,0) == true)
				*next_spec = spec.ex1b;
			break;
		case 146:
			for (i = 0; i < 6; i++)
				if (ADVEN[i].main_status == 1)
					for (j = 0; j < 24; j++)
						if ((ADVEN[i].items[j].variety > 0) && (ADVEN[i].items[j].special_class == spec.ex1a)
							&& (ADVEN[i].equip[j] == true)) {
							*next_spec = spec.ex1b;			
							*redraw = 1;
							take_item(i,j);
							}
			break;
		case 147:
			if (calc_day() >= spec.ex1a)
				*next_spec = spec.ex1b;
			break;
		case 148:
			for (j = 0; j < univ.town->max_dim(); j++)
				for (k = 0; k < univ.town->max_dim(); k++) 
					if (univ.town.is_barrel(j,k))
						*next_spec = spec.ex1b;								
			break;
		case 149:
			for (j = 0; j < univ.town->max_dim(); j++)
				for (k = 0; k < univ.town->max_dim(); k++) 
					if (univ.town.is_crate(j,k))
						*next_spec = spec.ex1b;								
			break;
		case 150:
			if (day_reached(spec.ex1a,spec.ex1b) == true)
				*next_spec = spec.ex2b;
			break;
		case 151:
			for (i = 0; i < 6; i++)
				if ((ADVEN[i].main_status == 1) && (ADVEN[i].traits[4] > 0))
					*next_spec = spec.ex1b;
			break;
		case 152:
			for (i = 0; i < 6; i++)
				if ((ADVEN[i].main_status == 1) && (ADVEN[i].traits[5] > 0))
					*next_spec = spec.ex1b;
			break;
		case 153:
			if (mage_lore_total() >= spec.ex1a)
					*next_spec = spec.ex1b;
			break;
		case 154: // text response
			check_mess = false;
			get_text_response(873,str3,0);
			j = 1; k = 1;
			spec.pic = minmax(0,8,spec.pic);
			get_strs((char *) str1,(char *) str2,0,spec.ex1a,spec.ex2a);
			for (i = 0; i < spec.pic;i++) {
				if ((spec.ex1a < 0) || (str3[i] != str1[i]))
					j = 0;
				if ((spec.ex2a < 0) || (str3[i] != str2[i]))
					k = 0;
				}
			if (j == 1)
				*next_spec = spec.ex1b;
				else if (k == 1)
					*next_spec = spec.ex2b;
			break;
		case 155:
			if (sd_legit(spec.sd1,spec.sd2) == true) {
				if (PSD[spec.sd1][spec.sd2] == spec.ex1a)
					*next_spec = spec.ex1b;
				}
			break;
		}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
}

void townmode_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw)
{
	bool check_mess = true;
	Str255 strs[6] = {"","","","","",""};
	short i,buttons[3] = {-1,-1,-1},r1;
	cSpecial spec;
	location l;
	ter_num_t ter;
	cItemRec store_i;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	l.x = spec.ex1a; l.y = spec.ex1b;
	
	if (is_out())
		return;
	switch (cur_node.type) {
		case 170:
			make_town_hostile();
			break;
		case 171:
			set_terrain(l,spec.ex2a);
			if(scenario.ter_types[spec.ex2a].special == TER_SPEC_CONVEYOR)
				belt_present = true;
			*redraw = true;
			draw_map(modeless_dialogs[5],10);
			break;
		case 172:
			if (coord_to_ter(spec.ex1a,spec.ex1b) == spec.ex2a){
				set_terrain(l,spec.ex2b);
				if(scenario.ter_types[spec.ex2a].special == TER_SPEC_CONVEYOR)
					belt_present = true;
			}
			else if (coord_to_ter(spec.ex1a,spec.ex1b) == spec.ex2b){
				set_terrain(l,spec.ex2a);
				if(scenario.ter_types[spec.ex2a].special == TER_SPEC_CONVEYOR)
					belt_present = true;
			}
			*redraw = 1;
			draw_map(modeless_dialogs[5],10);
			break;
		case 173:
			ter = coord_to_ter(spec.ex1a,spec.ex1b);
			set_terrain(l,scenario.ter_types[ter].trans_to_what);
			if(scenario.ter_types[spec.ex2a].special == TER_SPEC_CONVEYOR)
				belt_present = true;
			*redraw = 1;
			draw_map(modeless_dialogs[5],10);
			break;
		case 174:
			if (is_combat()) {
				ASB("Not while in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
				else { // 1 no
					*a = 1;
					if ((which_mode == 7) || (spec.ex2a == 0))
						teleport_party(spec.ex1a,spec.ex1b,1);
					else teleport_party(spec.ex1a,spec.ex1b,0);
				}
			*redraw = 1;
			break;
		case 175:
			if (which_mode == 7)
				break;
			hit_space(l,spec.ex2a,(eDamageType) spec.ex2b,1,1);
			*redraw = 1;
			break;
		case 176:
			if (which_mode == 7)
				break;
			radius_damage(l,spec.pic, spec.ex2a, (eDamageType) spec.ex2b);
			*redraw = 1;
			break;
		case 177:
			ter = coord_to_ter(spec.ex1a,spec.ex1b);
			if (scenario.ter_types[ter].special == TER_SPEC_LOCKABLE)
				set_terrain(l,scenario.ter_types[ter].flag1);
			*redraw = 1;
			break;
		case 178:
			ter = coord_to_ter(spec.ex1a,spec.ex1b);
			if (scenario.ter_types[ter].special == TER_SPEC_UNLOCKABLE)
				set_terrain(l,scenario.ter_types[ter].flag1);
			*redraw = 1;
			break;
		case 179:
			if (which_mode == 7)
				break;
			run_a_boom(l,spec.ex2a,0,0);
			break;
		case 180:
			create_wand_monst();
			*redraw = 1;
			break;
		case 181:
			if (spec.ex2a > 0)
				forced_place_monster(spec.ex2a,l);
				else place_monster(spec.ex2a,l);
			*redraw = 1;
			break;
		case 182:
			for (i = 0; i < univ.town->max_monst(); i++)
				if (univ.town.monst.dudes[i].number == spec.ex1a) {
					univ.town.monst.dudes[i].active = 0;
					}			
			*redraw = 1;
			break;
		case 183:
			for (i = 0; i < univ.town->max_monst(); i++)
				if ((univ.town.monst.dudes[i].active > 0) &&
					(((spec.ex1a == 0) && (1 == 1)) || 
					((spec.ex1a == 1) && (univ.town.monst.dudes[i].attitude % 2 == 0)) || 
					((spec.ex1a == 2) && (univ.town.monst.dudes[i].attitude % 2 == 1)))){
					univ.town.monst.dudes[i].active = 0;
					}
			*redraw = 1;
			break;
		case 184:
			if (which_mode > 4) {
				ASB("Can't use lever now.");
				check_mess = false;
				*next_spec = -1;
				}
				else {
					if (handle_lever(store_special_loc) > 0)
						*next_spec = spec.ex1b;
					}
			break;
		case 185:
			if (is_combat()) {
				ASB("Not while in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
				else if ((which_mode != 1) && (which_mode != 4)) {
					ASB("Can't teleport now.");
					if (which_mode < 3)
						*a = 1;
					*next_spec = -1;
					check_mess = false;
					}
				else if (FCD(870,0) == 2) {			
					*a = 1;
					if ((which_mode == 7) || (spec.ex2a == 0))
						teleport_party(spec.ex1a,spec.ex1b,1);
						else teleport_party(spec.ex1a,spec.ex1b,0);
					}
			break;
		case 186:
			if (FCD(871,0) == 2) 		
				*next_spec = spec.ex1b;
			break;
		case 187:
			if (is_combat()) {
				ASB("Can't change level in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
				else if (which_mode != 1) {
					ASB("Can't change level now.");
					if (which_mode < 3)
						*a = 1;
					*next_spec = -1;
					check_mess = false;
					}
				else {
					*a = 1;
					if (spec.ex2b < 0) spec.ex2b = 0;
					if ((spec.ex2b >= 8) || (FCD(880 + spec.ex2b,0) == 2))
						change_level(spec.ex2a,l.x,l.y);
					}
			break;
		case 188:
			check_mess = false;
			if (spec.m1 < 0)
				break;
			if (which_mode > 4) {
				ASB("Can't use lever now.");
				check_mess = false;
				*next_spec = -1;
				}
				else {
					for (i = 0; i < 3; i++)
						get_strs((char *) strs[i * 2],(char *) strs[i * 2 + 1],cur_spec_type,
						spec.m1 + i * 2 + spec_str_offset[cur_spec_type],spec.m1 + i * 2 + 1 + spec_str_offset[cur_spec_type]);
					buttons[0] = 9; buttons[1] = 35;
					i = custom_choice_dialog(strs,spec.pic,buttons); 
					if (i == 1) {*next_spec = -1;}
						else {
							ter = coord_to_ter(store_special_loc.x,store_special_loc.y);
							set_terrain(store_special_loc,scenario.ter_types[ter].trans_to_what);
							*next_spec = spec.ex1b;
							}
					}
			break;
		case 189:
			check_mess = false;
			if (spec.m1 < 0)
				break;
			if (is_combat()) {
				ASB("Not while in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
				else if ((which_mode != 1) && (which_mode != 4)) {
					ASB("Can't teleport now.");
					if (which_mode < 3)
						*a = 1;
					*next_spec = -1;
					check_mess = false;
					}
				else {
					for (i = 0; i < 3; i++)
						get_strs((char *) strs[i * 2],(char *) strs[i * 2 + 1]
						,cur_spec_type,spec.m1 + i * 2 + spec_str_offset[cur_spec_type],spec.m1 + i * 2 + 1 + spec_str_offset[cur_spec_type]);
					buttons[0] = 9; buttons[1] = 8;
					i = custom_choice_dialog(strs,722,buttons); 
					if (i == 1) { *next_spec = -1; if (which_mode < 3) *a = 1;}
						else {
							*a = 1;
							if (which_mode == 7) 
								teleport_party(spec.ex1a,spec.ex1b,1);
								else teleport_party(spec.ex1a,spec.ex1b,0);							
							}
					}
			break;
		case 190:
			check_mess = false;
			if ((spec.m1 < 0) && (spec.ex2b != 1))
				break;
			if (is_combat()) {
				ASB("Can't change level in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
				else if (which_mode != 1) {
					ASB("Can't change level now.");
					if (which_mode < 3)
						*a = 1;
					*next_spec = -1;
					check_mess = false;
					}
				else {
					if (spec.m1 >= 0) {
						for (i = 0; i < 3; i++)
							get_strs((char *) strs[i * 2],(char *) strs[i * 2 + 1],cur_spec_type,
							spec.m1 + i * 2 + spec_str_offset[cur_spec_type],spec.m1 + i * 2 + 1 + spec_str_offset[cur_spec_type]);
						buttons[0] = 20; buttons[1] = 24;
						}
					if (spec.ex2b == 1)
						i = 2;
						else i = custom_choice_dialog(strs,719,buttons) ;
					*a = 1;
					if (i == 1) { *next_spec = -1;}
						else {
							*a = 1;
							change_level(spec.ex2a,l.x,l.y);
							}
					}
			break;
		case 191:
			position_party(spec.ex1a,spec.ex1b,spec.ex2a,spec.ex2b);
			break;
		case 192:
			store_i = get_stored_item(spec.ex2a);
			place_item(store_i,l,true);
			break;
		case 193:
			if (which_mode == 7)
				break;
			if (is_combat()) {
				ASB("Not while in combat.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
			if (PSD[SDF_IS_PARTY_SPLIT] > 0) {
				ASB("Party is already split.");
				if (which_mode < 3)
					*a = 1;
				*next_spec = -1;
				check_mess = false;
				}
			r1 = char_select_pc(1,0,"Which character goes?");
			if (which_mode < 3)
				*a = 1;
			if (r1 != 6) {
				PSD[SDF_PARTY_SPLIT_PC] = r1;
				*next_spec = -1;
				start_split(spec.ex1a,spec.ex1b,spec.ex2a);
				}
				else check_mess = false;				
			break;
		case 194:
			if (is_combat()) {
				ASB("Not while in combat.");
				break;
				}
			if (which_mode < 3)
				*a = 1;
			*next_spec = -1;
			check_mess = false;		
			end_split(spec.ex1a);
			break;
		case 195:
			univ.party.start_timer(spec.ex1a, spec.ex1b, 1);			break;
		}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
}

void rect_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw){
	bool check_mess = true;
	short i,j,k;
	cSpecial spec;
	location l;
	ter_num_t ter;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	if (is_out())
		return;
		
	*redraw = 1;
	for (i = spec.ex1b;i <= spec.ex2b;i++)
		for (j = spec.ex1a; j <= spec.ex2a; j++) {
	
	l.x = i; l.y = j;
	switch (cur_node.type) {
		case 200:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_fire_wall(i,j);
			break;
		case 201:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_force_wall(i,j);
			break;
		case 202:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_ice_wall(i,j);
			break;
		case 203:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_blade_wall(i,j);
			break;
		case 204:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_scloud(i,j);
			break;
		case 205:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_sleep_cloud(i,j);
			break;
		case 206:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_quickfire(i,j);
			break;
		case 207:
			if (get_ran(1,1,100) <= spec.sd1 )
				univ.town.set_fire_barr(i,j,true);
			break;
		case 208:
			if (get_ran(1,1,100) <= spec.sd1 )
				univ.town.set_force_barr(i,j,true);
			break;
		case 209:
			if (spec.sd1 == 0)
				dispel_fields(i,j,1);
			else dispel_fields(i,j,2);
			break;
		case 210:
			if (get_ran(1,1,100) <= spec.sd1 )
				make_sfx(i,j,spec.sd2 + 1);
			break;
		case 211:
			if (get_ran(1,1,100) <= spec.sd1 ) {
				if (spec.sd2 == 0)
					make_web(i,j);
				if (spec.sd2 == 1)
					univ.town.set_barrel(i,j,true);
				if (spec.sd2 == 2)
					univ.town.set_crate(i,j,true);
			}
			break;
		case 212:
			for (k = 0; k < NUM_TOWN_ITEMS; k++)
				if ((univ.town.items[k].variety > ITEM_TYPE_NO_ITEM) && (univ.town.items[k].item_loc == l)) {
					univ.town.items[k].item_loc.x = spec.sd1;
					univ.town.items[k].item_loc.y = spec.sd2;
				}
			break;
		case 213:
			for (k = 0; k < NUM_TOWN_ITEMS; k++)
				if ((univ.town.items[k].variety > ITEM_TYPE_NO_ITEM) && (univ.town.items[k].item_loc == l)) {
					univ.town.items[k].variety = ITEM_TYPE_NO_ITEM;
				}
			break;
		case 214:
			if (get_ran(1,1,100) <= spec.sd2){
				set_terrain(l,spec.sd1);
				if(scenario.ter_types[spec.sd1].special == TER_SPEC_CONVEYOR)
					belt_present = true;
				*redraw = true;
				draw_map(modeless_dialogs[5],10);
			}
			break;
		case 215:
			if (coord_to_ter(i,j) == spec.sd1){
				set_terrain(l,spec.sd2);
				if(scenario.ter_types[spec.sd2].special == TER_SPEC_CONVEYOR)
					belt_present = true;
				*redraw = true;
				draw_map(modeless_dialogs[5],10);
			}
			else if (coord_to_ter(i,j) == spec.sd2){
				set_terrain(l,spec.sd1);
				if(scenario.ter_types[spec.sd1].special == TER_SPEC_CONVEYOR)
					belt_present = true;
				*redraw = true;
				draw_map(modeless_dialogs[5],10);
			}
			break;
		case 216:
			ter = coord_to_ter(i,j);
			set_terrain(l,scenario.ter_types[ter].trans_to_what);
			if(scenario.ter_types[scenario.ter_types[ter].trans_to_what].special == TER_SPEC_CONVEYOR)
				belt_present = true;
			*redraw = true;
			draw_map(modeless_dialogs[5],10);
			break;
		case 217:
			ter = coord_to_ter(i,j);
			if (scenario.ter_types[ter].special == 8){
				set_terrain(l,scenario.ter_types[ter].flag1);
				if(scenario.ter_types[scenario.ter_types[ter].trans_to_what].special == TER_SPEC_CONVEYOR)
					belt_present = true;
				*redraw = true;
				draw_map(modeless_dialogs[5],10);
			}
			break;
		case 218:
			ter = coord_to_ter(i,j);
			if (scenario.ter_types[ter].special == TER_SPEC_UNLOCKABLE){
				set_terrain(l,scenario.ter_types[ter].flag1);
				if(scenario.ter_types[scenario.ter_types[ter].trans_to_what].special == TER_SPEC_CONVEYOR)
					belt_present = true;
				*redraw = true;
				draw_map(modeless_dialogs[5],10);
			break;
			}
		}
	}
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
	}
}

void outdoor_spec(short which_mode,cSpecial cur_node,short cur_spec_type,
	short *next_spec,short *next_spec_type,short *a,short *b,short *redraw){
	bool check_mess = false;
	Str255 str1 = "",str2 = "";
	cSpecial spec;
	location l;
	
	spec = cur_node;
	*next_spec = cur_node.jumpto;
	
	if (is_out() == false) return;
	
	switch (cur_node.type) {
		case 225: create_wand_monst();
			*redraw = 1;
			break;
		case 226:
			univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].terrain[spec.ex1a][spec.ex1b] = spec.ex2a;
			l.x = spec.ex1a;
			l.y = spec.ex1b;
			l = local_to_global(l);
			univ.out[l.x][l.y] = spec.ex2a;
			*redraw = 1;
			check_mess = true;
			break;		
		case 227:
			if (spec.ex1a != minmax(0,3,spec.ex1a)) {
				give_error("Special outdoor enc. is out of range. Must be 0-3.","",0);
				//set_sd = false;
				}
				else {
					l = global_to_local(univ.party.p_loc);
					place_outd_wand_monst(l,
					univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].special_enc[spec.ex1a],true);			
					check_mess = true;
					}
			break;		
		case 228:
			check_mess = true;
			out_move_party(spec.ex1a,spec.ex1b);
			*redraw = 1;
			*a = 1;
			break;		
		case 229:
			get_strs((char *) str1,(char *) str2,1,spec.m1 + 10,-1);
			if (spec.ex2a >= 40)
				spec.ex2a = 39;
			if (spec.ex2a < 1)
				spec.ex2a = 1;
			spec.ex2b = minmax(0,6,spec.ex2b);
			switch (spec.ex1b) {
				case 0: start_shop_mode(0,spec.ex1a,spec.ex1a + spec.ex2a - 1,spec.ex2b,(char *) str1); break;
				case 1: start_shop_mode(10,spec.ex1a,spec.ex1a + spec.ex2a - 1,spec.ex2b,(char *) str1); break;
				case 2: start_shop_mode(11,spec.ex1a,spec.ex1a + spec.ex2a - 1 ,spec.ex2b,(char *) str1); break;
				case 3: start_shop_mode(12,spec.ex1a,spec.ex1a + spec.ex2a - 1,spec.ex2b,(char *) str1); break;
				case 4: start_shop_mode(3,spec.ex1a,spec.ex1a + spec.ex2a - 1,spec.ex2b,(char *) str1); break;
				}
			*next_spec = -1;
			break;		
		}
		
	if (check_mess == true) {
		handle_message(which_mode,cur_spec_type,cur_node.m1,cur_node.m2,a,b);
		}
}

void setsd(short a,short b,short val)
{
	if (sd_legit(a,b) == false) {
		give_error("The scenario attempted to change an out of range Stuff Done flag.","",0);
		return;
		}
	PSD[a][b] = val;
}

void handle_message(short which_mode,short cur_type,short mess1,short mess2,short *a,short *b)
{
	Str255 str1 = "",str2 = "";
	short label1 = -1,label2 = -1,label1b = -1,label2b = -1;
	short mess_adj[3] = {160,10,20};
	
	if ((mess1 < 0) && (mess2 < 0))
		return;
	if (which_mode == 7) { // talking
		*a = mess1 + ((mess1 >= 0) ? mess_adj[cur_type] : 0); 
		*b = mess2 + ((mess2 >= 0) ? mess_adj[cur_type] : 0);
		return;
		}
	get_strs((char *) str1,(char *) str2, cur_type, mess1 + ((mess1 >= 0) ? mess_adj[cur_type] : 0), 
		mess2 + ((mess2 >= 0) ? mess_adj[cur_type] : 0)) ;
	if (mess1 >= 0) {
		label1 = 1000 * cur_type + mess1 + mess_adj[cur_type];
		label1b = (is_out()) ? (univ.party.outdoor_corner.x + univ.party.i_w_c.x) +
			scenario.out_width * (univ.party.outdoor_corner.y + univ.party.i_w_c.y) : univ.town.num;
		}
	if (mess2 >= 0) {
		label2 = 1000 * cur_type + mess2 + mess_adj[cur_type];
		label2b = (is_out()) ? (univ.party.outdoor_corner.x + univ.party.i_w_c.x) +
			scenario.out_width * (univ.party.outdoor_corner.y + univ.party.i_w_c.y) : univ.town.num;
		}
	display_strings((char *) str1, (char *) str2,label1,label2, label1b,label2b, 
		"",57,scenario.intro_pic,PICT_SCEN,0);
}
 
void get_strs(char *str1,char *str2,short cur_type,short which_str1,short which_str2) 
{
	short num_strs[3] = {260,108,135};
		
	if (((which_str1 >= 0) && (which_str1 != minmax(0,num_strs[cur_type],which_str1))) ||
		((which_str2 >= 0) && (which_str2 != minmax(0,num_strs[cur_type],which_str2)))) {
		give_error("The scenario attempted to access a message out of range.","",0);
		return;
		}
	switch (cur_type) {
		case 0:
			if (which_str1 >= 0)
				strcpy((char *) str1,scenario.scen_strs(which_str1));
			if (which_str2 >= 0)
				strcpy((char *) str2,scenario.scen_strs(which_str2));
			break;
		case 1:
			if (which_str1 >= 0)
				//load_outdoor_str(loc(univ.party.outdoor_corner.x + univ.party.i_w_c.x,univ.party.outdoor_corner.y + univ.party.i_w_c.y),which_str1,(char *) str1);
				strcpy((char*)str1,univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].out_strs(which_str1));
			if (which_str2 >= 0)
				//load_outdoor_str(loc(univ.party.outdoor_corner.x + univ.party.i_w_c.x,univ.party.outdoor_corner.y + univ.party.i_w_c.y),which_str2,(char *) str2);
				strcpy((char*)str2,univ.out.outdoors[univ.party.i_w_c.x][univ.party.i_w_c.y].out_strs(which_str2));
			break;
		case 2:
			if (which_str1 >= 0)
				strcpy((char *) str1,univ.town->town_strs(which_str1));
			if (which_str2 >= 0)
				strcpy((char *) str2,univ.town->town_strs(which_str2));
			break;
		}

}
