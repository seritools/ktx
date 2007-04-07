/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  $Id: match.c,v 1.115 2007/04/07 17:52:37 qqshka Exp $
 */

#include "g_local.h"

void NextLevel ();
void IdlebotForceStart ();
void StartMatch ();
void remove_specs_wizards ();
void lastscore_add ();
void OnePlayerMidairStats();

float CountPlayers()
{
	gedict_t	*p;
	float		num = 0;

	for ( p = world; (p = find_plr( p )); )
		num++;

	return num;
}

float CountRPlayers()
{
	gedict_t	*p;
	float		num = 0;

	for ( p = world; (p = find_plr( p )); )
		if ( p->ready )
			num++;

	return num;
}

float CountTeams()
{
	gedict_t	*p, *p2;
	float		num = 0;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for ( p = world; (p = find_plr( p )); ) {
		if( p->k_flag || strnull( s = getteam( p ) ) )
			continue;

		p->k_flag = 1;
		num++;

		for( p2 = p; (p2 = find_plr( p2 )); )
			if( streq( s, getteam( p2 ) ) )
				p2->k_flag = 1;
	}
	return num;
}

// return number of teams where at least one member is ready?
float CountRTeams()
{
	gedict_t	*p, *p2;
	float		num = 0;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for ( p = world; (p = find_plr( p )); ) {
		if( p->k_flag || !p->ready || strnull( s = getteam( p ) ) )
			continue;

		p->k_flag = 1;
		num++;

		for( p2 = p; (p2 = find_plr( p2 )); )
			if( streq( s, getteam( p2 ) ) )
				p2->k_flag = 1;
	}
	return num;
}

// check count of members in each team i'm guess
// and return 0 if at least one team has less members than 'memcnt'
// else return 1 (even we have more mebers than memcnt, dunno is this bug <- FIXME)

float CheckMembers ( float memcnt )
{
	gedict_t	*p, *p2;
	float		f1;
	char		*s = "";

	for ( p = world; (p = find_plr( p )); )
		p->k_flag = 0;

	for( p = world; (p = find_plr( p )); ) {
		if ( p->k_flag )
			continue;

		p->k_flag = 1;
		f1 = 1;

		if( !strnull ( s = getteam( p ) ) )
			for( p2 = p; (p2 = find_plr( p2 )); )
				if( streq( s, getteam( p2 ) ) ) {
					p2->k_flag = 1;
					f1++;
				}

		if ( f1 < memcnt )
			return 0;
	}
	return 1;
}

#define MAX_TM_STATS (MAX_CLIENTS)

char tmStats_names[MAX_TM_STATS][MAX_TEAM_NAME]; // u can't put this in struct in QVM

typedef struct teamStats_s {
	char *name; // team name
	int gfrags; // frags from ghosts
	int frags, deaths, tkills;
	float dmg_t, dmg_g, dmg_team;
// { ctf
	float res, str, rgn, hst;
	int caps, pickups, returns, f_defends, c_defends;
// }
	wpType_t wpn[wpMAX];
	itType_t itm[itMAX];
} teamStats_t;

teamStats_t tmStats[MAX_TM_STATS];

int tmStats_cnt = 0;

void CollectTpStats()
{
	gedict_t	*p, *p2;
	int from1, from2;
	int i, *frags;
	char *tmp = "";

	for( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); )
		p->ready = 0; // clear mark

//	get one player and search all his mates, mark served players via ->ready field 
//  ghosts is served too

	for( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); ) {
		if( p->ready || strnull( tmp = getteam( p ) ) )
			continue; // served or wrong team

		if ( tmStats_cnt < 0 || tmStats_cnt >= MAX_TM_STATS )
			return; // all slots busy

		for( from2 = 0, p2 = world; (p2 = find_plrghst ( p2, &from2 )); ) {
			if( p2->ready || strneq( tmp, getteam( p2 ) ))
				continue; // served or on different team

			if ( strnull(tmStats[tmStats_cnt].name) ) // we not yet done that, do that once
				strlcpy(tmStats[tmStats_cnt].name, tmp, MAX_TEAM_NAME);

			frags = (p2->ct == ctPlayer ? &tmStats[tmStats_cnt].frags : &tmStats[tmStats_cnt].gfrags);
			frags[0]					  += p2->s.v.frags;
			tmStats[tmStats_cnt].deaths   += p2->deaths;
			tmStats[tmStats_cnt].tkills   += p2->friendly;

			tmStats[tmStats_cnt].dmg_t    += p2->ps.dmg_t;
			tmStats[tmStats_cnt].dmg_g    += p2->ps.dmg_g;
			tmStats[tmStats_cnt].dmg_team += p2->ps.dmg_team;

			for ( i = itNONE; i < itMAX; i++ ) { // summ each field of items
				tmStats[tmStats_cnt].itm[i].tooks += p2->ps.itm[i].tooks;
				tmStats[tmStats_cnt].itm[i].time  += p2->ps.itm[i].time;
			}

			for ( i = wpNONE; i < wpMAX; i++ ) { // summ each field of weapons
				tmStats[tmStats_cnt].wpn[i].hits    += p2->ps.wpn[i].hits;
				tmStats[tmStats_cnt].wpn[i].attacks += p2->ps.wpn[i].attacks;

				tmStats[tmStats_cnt].wpn[i].kills   += p2->ps.wpn[i].kills;
				tmStats[tmStats_cnt].wpn[i].deaths  += p2->ps.wpn[i].deaths;
				tmStats[tmStats_cnt].wpn[i].tkills  += p2->ps.wpn[i].tkills;
				tmStats[tmStats_cnt].wpn[i].ekills  += p2->ps.wpn[i].ekills;
				tmStats[tmStats_cnt].wpn[i].drops   += p2->ps.wpn[i].drops;
				tmStats[tmStats_cnt].wpn[i].tooks   += p2->ps.wpn[i].tooks;
				tmStats[tmStats_cnt].wpn[i].ttooks  += p2->ps.wpn[i].ttooks;
			}

// { ctf related
			tmStats[tmStats_cnt].res += p2->ps.res_time;
			tmStats[tmStats_cnt].str += p2->ps.str_time;
			tmStats[tmStats_cnt].hst += p2->ps.hst_time;
			tmStats[tmStats_cnt].rgn += p2->ps.rgn_time;

			tmStats[tmStats_cnt].caps      += p2->ps.caps;
			tmStats[tmStats_cnt].pickups   += p2->ps.pickups;
			tmStats[tmStats_cnt].returns   += p2->ps.returns;
			tmStats[tmStats_cnt].f_defends += p2->ps.f_defends;
			tmStats[tmStats_cnt].c_defends += p2->ps.c_defends;
// }

			p2->ready = 1; // set mark
		}

		if ( strnull(tmStats[tmStats_cnt].name) )
			continue; // wtf, empty team?

		if ( isCTF() && g_globalvars.time - match_start_time > 0 )
		{
			tmStats[tmStats_cnt].res = ( tmStats[tmStats_cnt].res / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].str = ( tmStats[tmStats_cnt].str / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].hst = ( tmStats[tmStats_cnt].hst / ( g_globalvars.time - match_start_time )) * 100;
			tmStats[tmStats_cnt].rgn = ( tmStats[tmStats_cnt].rgn / ( g_globalvars.time - match_start_time )) * 100;
		}

		tmStats_cnt++;
	}
}

void ShowTeamsBanner ( )
{
	int i;

	G_bprint(2, "�����������������������������������\n");


//	for( i = 666 + 1; i <= k_teamid ; i++ )
//		G_bprint(2, "%s�%s�", (i != (666+1) ? " vs " : ""), ezinfokey(world, va("%d", i)));
	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ )
		G_bprint(2, "%s�%s�", (i ? " vs " : ""), tmStats[i].name);

	G_bprint(2, " %s:\n", redtext("match statistics"));

	G_bprint(2, "�����������������������������������\n");
}

void SummaryTPStats()
{
	int i;
	float h_sg, h_ssg, h_gl, h_rl, h_lg;
	
	ShowTeamsBanner ();
	
	G_bprint(2, "\n%s, %s, %s, %s\n", redtext("weapons"), redtext("powerups"),
									  redtext("armors&mhs"), redtext("damage"));
	G_bprint(2, "�����������������������������������\n");

	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {

		h_sg  = 100.0 * tmStats[i].wpn[wpSG].hits  / max(1, tmStats[i].wpn[wpSG].attacks);
		h_ssg = 100.0 * tmStats[i].wpn[wpSSG].hits / max(1, tmStats[i].wpn[wpSSG].attacks);
		h_gl  = tmStats[i].wpn[wpGL].hits;
		h_rl  = tmStats[i].wpn[wpRL].hits;
		h_lg  = 100.0 * tmStats[i].wpn[wpLG].hits  / max(1, tmStats[i].wpn[wpLG].attacks);

		// weapons
		G_bprint(2, "�%s�: %s:%s%s%s%s%s\n", tmStats[i].name, redtext("Wp"),
					(h_lg  ? va(" %s%.0f%%", redtext("lg"),   h_lg) : ""),
					(h_rl  ? va(" %s%.0f",   redtext("rl"),   h_rl) : ""), 
					(h_gl  ? va(" %s%.0f",   redtext("gl"),   h_gl) : ""), 
					(h_sg  ? va(" %s%.0f%%", redtext("sg"),   h_sg) : ""),
					(h_ssg ? va(" %s%.0f%%", redtext("ssg"), h_ssg) : ""));

		// powerups
		G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("Powerups"),
				redtext("Q"), tmStats[i].itm[itQUAD].tooks, redtext("P"), tmStats[i].itm[itPENT].tooks, 
				redtext("R"), tmStats[i].itm[itRING].tooks);

		// armors + megahealths
		G_bprint(2, "%s: %s:%d %s:%d %s:%d %s:%d\n", redtext("Armr&mhs"),
				redtext("ga"), tmStats[i].itm[itGA].tooks, redtext("ya"), tmStats[i].itm[itYA].tooks, 
				redtext("ra"), tmStats[i].itm[itRA].tooks, redtext("mh"), tmStats[i].itm[itHEALTH_100].tooks);

		if ( isCTF() ) 
		{
			G_bprint(2, "%s: %s:%.0f%% %s:%.0f%% %s:%.0f%% %s:%.0f%%\n", redtext("RuneTime"),
				redtext("res"), tmStats[i].res, redtext("str"), tmStats[i].str,
				redtext("hst"), tmStats[i].hst, redtext("rgn"), tmStats[i].rgn);
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("     CTF"),
				redtext("pickups"), tmStats[i].pickups, redtext("caps"), tmStats[i].caps, redtext("returns"), tmStats[i].returns );
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Defends"),
				redtext("flag"), tmStats[i].f_defends, redtext("carrier"), tmStats[i].c_defends );
		}

		// rl
		if ( isTeam() ) // rl stats pointless in other modes?
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("      RL"),
					redtext("Took"), tmStats[i].wpn[wpRL].tooks, redtext("Killed"), tmStats[i].wpn[wpRL].ekills,
					redtext("Dropped"), tmStats[i].wpn[wpRL].drops);

		// damage
		G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
					redtext("Tkn"), tmStats[i].dmg_t, redtext("Gvn"), tmStats[i].dmg_g, redtext("Tm"), tmStats[i].dmg_team);
	}

	G_bprint(2, "�����������������������������������\n");
}


void TeamsStats ( )
{
	int	i, sumfrags = 0, wasPrint = 0;

	// Summing up the frags to calculate team percentages
	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ )
		sumfrags += max(0, tmStats[i].frags + tmStats[i].gfrags);
	// End of summing

	G_bprint(2, "\n%s: %s\n"
				"�����������������������������������\n", redtext("Team scores"),
													     redtext("frags � percentage"));

	for( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		G_bprint(2, "�%s�: %d", tmStats[i].name, tmStats[i].frags );

		if( tmStats[i].gfrags )
			G_bprint( 2, " + (%d) = %d", tmStats[i].gfrags, tmStats[i].frags + tmStats[i].gfrags );

		// effi
		G_bprint(2, " � %.1f%%\n", (sumfrags > 0 ? ((float)(tmStats[i].frags + tmStats[i].gfrags))/sumfrags * 100 : 0.0));

		wasPrint = 1;
	}

	if ( wasPrint )
		G_bprint(2, "�����������������������������������\n");
}

float maxfrags, maxdeaths, maxfriend, maxeffi, maxcaps, maxdefends;
int maxspree, maxspree_q;

void OnePlayerStats(gedict_t *p, int tp)
{
	float	dmg_g, dmg_t, dmg_team;
	int   ra, ya, ga;
	int   mh, d_rl, k_rl, t_rl;
	int   quad, pent, ring;
	float h_rl, a_rl, h_gl, a_gl, h_lg, a_lg, h_sg, a_sg, h_ssg, a_ssg;
	int res, str, hst, rgn;

	dmg_g = p->ps.dmg_g;
	dmg_t = p->ps.dmg_t;
	dmg_team = p->ps.dmg_team;
	ra    = p->ps.itm[itRA].tooks;
	ya    = p->ps.itm[itYA].tooks;
	ga    = p->ps.itm[itGA].tooks;
	mh    = p->ps.itm[itHEALTH_100].tooks;
	quad  = p->ps.itm[itQUAD].tooks;
	pent  = p->ps.itm[itPENT].tooks;
	ring  = p->ps.itm[itRING].tooks;

	h_rl  = p->ps.wpn[wpRL].hits;
	a_rl  = p->ps.wpn[wpRL].attacks;
	h_gl  = p->ps.wpn[wpGL].hits;
	a_gl  = p->ps.wpn[wpGL].attacks;
	h_lg  = p->ps.wpn[wpLG].hits;
	a_lg  = p->ps.wpn[wpLG].attacks;
	h_sg  = p->ps.wpn[wpSG].hits;
	a_sg  = p->ps.wpn[wpSG].attacks;
	h_ssg = p->ps.wpn[wpSSG].hits;
	a_ssg = p->ps.wpn[wpSSG].attacks;

	h_sg  = 100.0 * h_sg  / max(1, a_sg);
	h_ssg = 100.0 * h_ssg / max(1, a_ssg);
#if 0 /* percentage */
	h_gl  = 100.0 * h_gl  / max(1, a_gl);
	h_rl  = 100.0 * h_rl  / max(1, a_rl);
#else /* just count of direct hits */
	h_gl  = h_gl;
	h_rl  = h_rl;
#endif
	h_lg  = 100.0 * h_lg  / max(1, a_lg);

	d_rl = p->ps.wpn[wpRL].drops;
	k_rl = p->ps.wpn[wpRL].ekills;
	t_rl = p->ps.wpn[wpRL].tooks;

	if ( isCTF() && g_globalvars.time - match_start_time > 0 )
	{
		res = ( p->ps.res_time / ( g_globalvars.time - match_start_time )) * 100;
		str = ( p->ps.str_time / ( g_globalvars.time - match_start_time )) * 100;
		hst = ( p->ps.hst_time / ( g_globalvars.time - match_start_time )) * 100;
		rgn = ( p->ps.rgn_time / ( g_globalvars.time - match_start_time )) * 100;
	}
	else
	 res = str = hst = rgn = 0;

	if ( tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );

	G_bprint(2, "\x87 %s%s:\n"
			"  %d (%d) %s%.1f%%\n", ( isghost( p ) ? "\x83" : "" ), getname(p),
			( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points) : (int)p->s.v.frags), 
			( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points - p->deaths) : (int)(p->s.v.frags - p->deaths)),
			( tp ? va("%d ", (int)p->friendly ) : "" ),
			p->efficiency);

// qqshka - force show this always
//	if ( !tp || cvar( "tp_players_stats" ) ) {
		// weapons
		G_bprint(2, "%s:%s%s%s%s%s\n", redtext("Wp"),
				(h_lg  ? va(" %s%.1f%%", redtext("lg"),   h_lg) : ""),
				(h_rl  ? va(" %s%.0f",   redtext("rl"),   h_rl) : ""),
				(h_gl  ? va(" %s%.0f",   redtext("gl"),   h_gl) : ""),
				(h_sg  ? va(" %s%.1f%%", redtext("sg"),   h_sg) : ""),
				(h_ssg ? va(" %s%.1f%%", redtext("ssg"), h_ssg) : ""));

		// velocity
		if ( isDuel() )
		G_bprint(2, "%s: %s:%.1f %s:%.1f\n", redtext("   Speed"),
				redtext("max"), p->ps.velocity_max,
				redtext("average"), p->ps.vel_frames > 0 ? p->ps.velocity_sum / p->ps.vel_frames : 0.);

		// armors + megahealths
		G_bprint(2, "%s: %s:%d %s:%d %s:%d %s:%d\n", redtext("Armr&mhs"),
				redtext("ga"), ga, redtext("ya"), ya, redtext("ra"), ra, redtext("mh"), mh);

		// powerups
		if ( isTeam() || isCTF() )
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("Powerups"),
				redtext("Q"), quad, redtext("P"), pent, redtext("R"), ring);

		if ( isCTF() )
		{
			G_bprint(2, "%s: %s:%d%% %s:%d%% %s:%d%% %s:%d%%\n", redtext("RuneTime"),
				redtext("res"), res, redtext("str"), str, redtext("hst"), hst, redtext("rgn"), rgn );
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("     CTF"),
				redtext("pickups"), p->ps.pickups, redtext("caps"), p->ps.caps, redtext("returns"), p->ps.returns );
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Defends"),
				redtext("flag"), p->ps.f_defends, redtext("carrier"), p->ps.c_defends );
		}

		// rl
		if ( isTeam() )
			G_bprint(2, "%s: %s:%d %s:%d %s:%d\n", redtext("      RL"),
				redtext("Took"), t_rl, redtext("Killed"), k_rl, redtext("Dropped"), d_rl);

		// damage
		G_bprint(2, "%s: %s:%.0f %s:%.0f %s:%.0f\n", redtext("  Damage"),
				redtext("Tkn"), dmg_t, redtext("Gvn"), dmg_g, redtext("Tm"), dmg_team);

		if ( isDuel() )
		{
			//  endgame h & a
			G_bprint(2, "  %s  H&A: \220H:%d\221\217", redtext("EndGame"), (int)p->s.v.health);
			if ( (int)p->s.v.armorvalue )
				G_bprint(2, "\220A:%s:%d\221\n", armor_type(p->s.v.items), (int)p->s.v.armorvalue);
			else
				G_bprint(2, "\220A:0\221\n");

			// overtime h & a
			if ( k_overtime ) {
				G_bprint(2, "  %s H&A: \220H:%d\221\217", redtext("OverTime"), (int)p->ps.ot_h);
				if ( (int)p->ps.ot_a )
					G_bprint(2, "\220A:%s:%d\221\n", armor_type(p->ps.ot_items), (int)p->ps.ot_a);
				else
					G_bprint(2, "\220A:0\221\n");
			}
		}
		else
			G_bprint(2, "%s: %s:%d %s:%d\n", redtext(" Streaks"),
				redtext("Frags"), p->ps.spree_max, redtext("QuadRun"), p->ps.spree_max_q);

		// spawnfrags
		if ( !isCTF() )
			G_bprint(2, "  %s: \220%d\221\n", redtext("SpawnFrags"), p->ps.spawn_frags);
//	}

	if ( !tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );

	maxfrags   = max((isCTF() ? p->s.v.frags - p->ps.ctf_points : p->s.v.frags), maxfrags);
	maxdeaths  = max(p->deaths, maxdeaths);
	maxfriend  = max(p->friendly, maxfriend);
	maxeffi    = max(p->efficiency, maxeffi);
	maxcaps    = max(p->ps.caps, maxcaps);
	maxdefends = max(p->ps.f_defends, maxdefends);
	maxspree   = max(p->ps.spree_max, maxspree);
	maxspree_q = max(p->ps.spree_max_q, maxspree_q);
}

// Players statistics printout here
void PlayersStats ()
{
	gedict_t	*p, *p2;
	char		*tmp, *tmp2;
	int			tp, first, from1, from2;

	from1 = 0;
	p = find_plrghst ( world, &from1 );
	while( p ) {
		p->ready = 0; // clear mark
		p = find_plrghst ( p, &from1 );
	}

	// Probably low enough for a start value :)
	maxfrags = -999999;

	maxeffi = maxfriend = maxdeaths = maxcaps = maxdefends = 0;
	maxspree = maxspree_q = 0;

	tp = isTeam() || isCTF();

	G_bprint(2, "\n%s:\n"
				"%s (%s) %s� %s\n"
				"�����������������������������������\n", redtext("Player statistics"),
				redtext( "Frags"), redtext( "rank"), ( tp ? redtext("friendkills "): "" ), redtext( "efficiency" ));

	from1 = 0;
	p = find_plrghst ( world, &from1 );
	while( p ) {
		if( !p->ready ) {

			first = 1;

			from2 = 0;
			p2 = find_plrghst ( world, &from2 );
			while ( p2 ) {
				if( !p2->ready ) {
					// sort by team
					tmp = getteam(p);
					tmp2 = getteam(p2);

					if( streq ( tmp, tmp2 ) ) {
						p2->ready = 1; // set mark

						if ( first ) {
							first = 0;
							if ( tp )
								G_bprint(2, "Team �%s�:\n", tmp );
						}

						if ( isCTF() )
						{
							if ( p2->s.v.frags - p2->ps.ctf_points < 1 )
								p2->efficiency = 0;
							else
								p2->efficiency = (p2->s.v.frags - p2->ps.ctf_points) / (p2->s.v.frags - p2->ps.ctf_points + p2->deaths) * 100;
						}
						else if ( isRA() )
						{
							p2->efficiency = ( ( p2->ps.loses + p2->ps.wins ) ? ( p2->ps.wins * 100.0f ) / ( p2->ps.loses + p2->ps.wins ) : 0 );
						}
						else
						{
							if( p2->s.v.frags < 1 )
								p2->efficiency = 0;
							else
								p2->efficiency = p2->s.v.frags / (p2->s.v.frags + p2->deaths) * 100;
						}

						if ( cvar("k_midair") )
							OnePlayerMidairStats(p2, tp);
						else
							OnePlayerStats(p2, tp);
					}
				}

				p2 = find_plrghst ( p2, &from2 );

				if ( !p2 )
					G_bprint(2, "\n"); // split players from different teams via \n
			}
		}

		p = find_plrghst ( p, &from1 );
	}
}

// Print the high score table
void TopStats ( )
{
	gedict_t	*p;
	float		f1;
	int			from;

	G_bprint(2, "�%s� %s:\n"
				"�����������������������������������\n"
				"      Frags: ", g_globalvars.mapname, redtext("top scorers"));

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( (!isCTF() && p->s.v.frags == maxfrags) || (isCTF() &&  p->s.v.frags - p->ps.ctf_points == maxfrags)) {
			G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxfrags);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}


	G_bprint(2, "     Deaths: ");

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( p->deaths == maxdeaths ) {
			G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), (int)maxdeaths);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}

	if( maxfriend ) {
		G_bprint(2, "Friendkills: ");

		from = f1 = 0;
		p = find_plrghst ( world, &from );
		while( p ) {
			if( p->friendly == maxfriend ) {
				G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), (int)maxfriend);
				f1 = 1;
			}

			p = find_plrghst ( p, &from );
		}
	}

	G_bprint(2, " Efficiency: ");

	from = f1 = 0;
	p = find_plrghst ( world, &from );
	while( p ) {
		if( p->efficiency == maxeffi ) {
			G_bprint(2, "%s%s%s �%.1f%%�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ),	getname( p ), maxeffi);
			f1 = 1;
		}

		p = find_plrghst ( p, &from );
	}

	if ( maxspree )
	{
		G_bprint( 2, " FragStreak: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.spree_max == maxspree ) {
				G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
					( isghost( p ) ? "\x83" : "" ), getname( p ), maxspree );
				f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
	}

	if ( maxspree_q )
	{
		G_bprint( 2, "    QuadRun: ");
		from = f1 = 0;
		p = find_plrghst( world, &from );
		while( p ) {
			if ( p->ps.spree_max_q == maxspree_q ) {
					G_bprint(2, "%s%s%s \220%d\221\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), maxspree_q );
					f1 = 1;
			}
			p = find_plrghst( p, &from );
		}
	}

	if ( isCTF() )
	{
		if ( maxcaps > 0 )
		{
			G_bprint(2, "   Captures: ");
			from = f1 = 0;
			p = find_plrghst ( world, &from );
			while( p ) {
				if ( p->ps.caps == maxcaps ) {
					G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
								( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxcaps );
					f1 = 1;
				}
				p = find_plrghst ( p, &from );
			}
		}

		if ( maxdefends > 0 )
		{
			G_bprint(2, "FlagDefends: ");
			from = f1 = 0;
			p = find_plrghst ( world, &from );
			while( p ) {
				if ( p->ps.f_defends == maxdefends ) {
					G_bprint(2, "%s%s%s �%d�\n", (f1 ? "             " : ""),
						( isghost( p ) ? "\x83" : "" ), getname( p ), (int)maxdefends );
					f1 = 1;
				}
				p = find_plrghst ( p, &from );
			}
		}
	}

	G_bprint(2, "�����������������������������������\n");
}

void OnePlayerMidairStats( gedict_t *p, int tp )
{
	int midairs, silver, gold, diamond;

	midairs = p->ps.midairs;
	silver  = p->ps.midairs_s;
	gold    = p->ps.midairs_g;
	diamond = p->ps.midairs_d;

	if ( tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );

	// need to fix stats in midair similar to ctf

	G_bprint(2, "\x87 %s%s:\n"
		 "  %d (%d) %s%.1f%%\n", ( isghost( p ) ? "\x83" : "" ), getname(p),
		 ( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points) : (int)p->s.v.frags),
		 ( isCTF() ? (int)(p->s.v.frags - p->ps.ctf_points - p->deaths) : (int)(p->s.v.frags - p->deaths)),
		 ( tp ? va("%d ", (int)p->friendly ) : "" ),
		 p->efficiency);

	G_bprint(2, "%s: %d\n", redtext("Midairs"), midairs);
	G_bprint(2, "%s: %d\n", redtext(" Silver"), silver);
	G_bprint(2, "%s: %d\n", redtext("   G0ld"), gold);
	G_bprint(2, "%s: %d\n", redtext("Diam0nd"), diamond);

	if ( !tp )
		G_bprint(2,"\235\236\236\236\236\236\236\236\236\237\n" );
}

char *GetMode() {

	if ( isRA() )
		return "RA";
	else if ( isDuel() )
		return "duel";
	else if ( isTeam() )
		return "team";
	else if ( isCTF() )
		return "CTF";
	else if ( isFFA() )
		return "FFA";
	else
		return "unknown";
}

char *WpName( weaponName_t wp )
{
	switch ( wp ) {
		case wpAXE: return "axe";
		case wpSG:  return "sg";
		case wpSSG: return "ssg";
		case wpNG:  return "ng";
		case wpSNG: return "sng";
		case wpGL:  return "gl";
		case wpRL:  return "rl";
		case wpLG:  return "lg";

		// shut up gcc
		case wpNONE:
		case wpMAX: return "unknown";
	}

	return "unknown";
}

char *ItName( itemName_t it )
{
	switch ( it ) {
		case itHEALTH_15:  return "health_15";
		case itHEALTH_25:  return "health_25";
		case itHEALTH_100: return "health_100";
		case itGA:		   return "ga";
		case itYA:		   return "ya";
		case itRA:		   return "ra";
		case itQUAD:	   return "q";
		case itPENT:	   return "p";
		case itRING:	   return "r";

		// shut up gcc
		case itNONE:
		case itMAX: return "unknown";
	}        
	         
	return "unknown";
}            
             
             
qboolean itPowerup( itemName_t it )
{            
	return (it == itQUAD || it == itPENT || it == itRING);
}

fileHandle_t di_handle;

void s2di( const char *fmt, ... )
{
	va_list argptr;
	char    text[1024];

	va_start( argptr, fmt );
	vsnprintf( text, sizeof(text), fmt, argptr );
	va_end( argptr );

	text[sizeof(text)-1] = 0;

	trap_FS_WriteFile( text, strlen(text), di_handle );
}

void StatsToFile()
{
	gedict_t	*p, *p2;
	int from1, from2;
	char *team = "";

	char date[32], name[256], tmp[1024], buf[1024], *ip = "", *port = "";
	int i = 0, j;

	if ( strnull( ip = cvar_string( "sv_local_addr" ) ) || strnull( port = strchr(ip, ':') ) || !(i = atoi(port + 1)) )
		return;

	port[0] = 0;
	port++;

	if ( strnull( cvar_string( "serverdemo" ) ) || cvar("sv_demotxt") != 2 )
		return; // does't record demo or does't want stats to be put in file

	snprintf(name, sizeof(name), "demoinfo_%s_%d.txt", ip, i);
	if ( trap_FS_OpenFile( name, &di_handle, FS_WRITE_BIN ) < 0 )
		return; // OpenFile is last check, so we does't need CloseFile each "return" above

	if ( !QVMstrftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", 0) )
		date[0] = 0; // bad date

	s2di("%s", "<?xml version=\"1.0\"?>\n");
	s2di("<match version=\"2\" date=\"%s\" map=\"%s\" hostname=\"%s\" ip=\"%s\" port=\"%d\" mode=\"%s\">\n", 
		date, g_globalvars.mapname, striphigh(cvar_string("hostname")), ip, i, GetMode());

// { TEAMS

	for ( tmp[0] = i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		snprintf(buf, sizeof(buf), " team%d=\"%s\"", i + 1, tmStats[i].name);
		strlcat(tmp, buf, sizeof(tmp));
	}

	if ( i )
		s2di("\t<teams%s>\n", striphigh(tmp));

	for ( i = 0; i < min(tmStats_cnt, MAX_TM_STATS); i++ ) {
		s2di("\t\t<team name=\"%s\" frags=\"%d\" deaths=\"%d\" tkills=\"%d\" dmg_tkn=\"%d\" dmg_gvn=\"%d\" dmg_tm=\"%d\">\n", 
			striphigh(tmStats[i].name), tmStats[i].frags + tmStats[i].gfrags, tmStats[i].deaths, tmStats[i].tkills,
			(int)tmStats[i].dmg_t, (int)tmStats[i].dmg_g, (int)tmStats[i].dmg_team);

		s2di("\t\t\t<weapons>\n");
		for ( j = 1; j < wpMAX; j++ )
			s2di("\t\t\t\t<weapon name=\"%s\" hits=\"%d\" attacks=\"%d\""
					" kills=\"%d\" deaths=\"%d\" tkills=\"%d\" ekills=\"%d\""
					" drops=\"%d\" tooks=\"%d\" ttooks=\"%d\"/>\n",
					WpName(j), tmStats[i].wpn[j].hits, tmStats[i].wpn[j].attacks,
					tmStats[i].wpn[j].kills, tmStats[i].wpn[j].deaths, tmStats[i].wpn[j].tkills, tmStats[i].wpn[j].ekills,
					tmStats[i].wpn[j].drops, tmStats[i].wpn[j].tooks, tmStats[i].wpn[j].ttooks);
		s2di("\t\t\t</weapons>\n");

		s2di("\t\t\t<items>\n");
		for ( j = 1; j < itMAX; j++) {
			if ( itPowerup( j ) )
				snprintf(buf, sizeof(buf), " time=\"%d\"", (int)tmStats[i].itm[j].time);
			else
				buf[0] = 0;
			s2di("\t\t\t\t<item name=\"%s\" tooks=\"%d\"%s/>\n", ItName(j), tmStats[i].itm[j].tooks, buf);
		}
		s2di("\t\t\t</items>\n");
		s2di("\t\t</team>\n");
	}

	if ( i )
		s2di("\t</teams>\n");

// } TEAMS

// { PLAYERS

	for ( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); )
		p->ready = 0; // clear mark

	s2di("\t<players>\n");

//	get one player and search all his mates, mark served players via ->ready field 
//  ghosts is served too

	for ( from1 = 0, p = world; (p = find_plrghst ( p, &from1 )); ) {
		team = getteam( p );
		if ( p->ready /* || strnull( team ) */ )
			continue; // served or wrong team

		for ( from2 = 0, p2 = world; (p2 = find_plrghst ( p2, &from2 )); ) {
			if ( p2->ready || strneq( team, getteam( p2 ) ))
				continue; // served or on different team

			s2di("\t\t<player name=\"%s\" team=\"%s\" frags=\"%d\" deaths=\"%d\" tkills=\"%d\""
						" dmg_tkn=\"%d\" dmg_gvn=\"%d\" dmg_tm=\"%d\">\n",
					striphigh(getname(p2)), striphigh(team), (int)p2->s.v.frags, (int)p2->deaths, (int)p2->friendly,
					(int)p2->ps.dmg_t, (int)p2->ps.dmg_g, (int)p2->ps.dmg_team);

			s2di("\t\t\t<weapons>\n");
			for ( j = 1; j < wpMAX; j++ )
			s2di("\t\t\t\t<weapon name=\"%s\" hits=\"%d\" attacks=\"%d\""
					" kills=\"%d\" deaths=\"%d\" tkills=\"%d\" ekills=\"%d\""
					" drops=\"%d\" tooks=\"%d\" ttooks=\"%d\"/>\n",
					WpName(j), p2->ps.wpn[j].hits, p2->ps.wpn[j].attacks,
					p2->ps.wpn[j].kills, p2->ps.wpn[j].deaths, p2->ps.wpn[j].tkills, p2->ps.wpn[j].ekills,
					p2->ps.wpn[j].drops, p2->ps.wpn[j].tooks, p2->ps.wpn[j].ttooks);
			s2di("\t\t\t</weapons>\n");

			s2di("\t\t\t<items>\n");
			for ( j = 1; j < itMAX; j++) {
				if ( itPowerup( j ) )
					snprintf(buf, sizeof(buf), " time=\"%d\"", (int)p2->ps.itm[j].time);
				else
					buf[0] = 0;
				s2di("\t\t\t\t<item name=\"%s\" tooks=\"%d\"%s/>\n", ItName(j), p2->ps.itm[j].tooks, buf);
			}
			s2di("\t\t\t</items>\n");

			s2di("\t\t</player>\n");
			p2->ready = 1; // set mark
		}
	}

	s2di("\t</players>\n");
// } PLAYERS

	s2di("</match>\n");

	trap_FS_CloseFile( di_handle );

	localcmd("\ndemoinfoadd ** %s\n", name);
	trap_executecmd();
}

void EM_on_MatchEndBreak( int isBreak )
{
	gedict_t *p;

	for( p = world; (p = find_client( p )); )
		if ( isBreak )
			on_match_break( p );
		else
			on_match_end( p );
}

void EM_CorrectStats()
{
	gedict_t	*p;

	for( p = world; (p = find_plr( p )); ) 
	{
		// take away powerups so scoreboard looks normal
		p->s.v.items = (int)p->s.v.items & ~(IT_INVISIBILITY | IT_INVULNERABILITY | IT_SUIT | IT_QUAD);
		p->s.v.effects = (int)p->s.v.effects & ~(EF_DIMLIGHT | EF_BLUE | EF_RED );
		p->invisible_finished = 0;
		p->invincible_finished = 0;
		p->super_damage_finished = 0;
		p->radsuit_finished = 0;

		p->ps.spree_max = max(p->ps.spree_current, p->ps.spree_max);
		p->ps.spree_max_q = max(p->ps.spree_current_q, p->ps.spree_max_q);

		adjust_pickup_time( &p->q_pickup_time, &p->ps.itm[itQUAD].time );
		adjust_pickup_time( &p->p_pickup_time, &p->ps.itm[itPENT].time );
		adjust_pickup_time( &p->r_pickup_time, &p->ps.itm[itRING].time );

		if ( isCTF() ) { // if a player ends the game with a rune adjust their rune time

			if ( p->ctf_flag & CTF_RUNE_RES )
				p->ps.res_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_STR )
				p->ps.str_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_HST )
				p->ps.hst_time += g_globalvars.time - p->rune_pickup_time;
			else if ( p->ctf_flag & CTF_RUNE_RGN )
				p->ps.rgn_time += g_globalvars.time - p->rune_pickup_time;
		}
	}
}

// WARNING: if we are skip log, we are also delete demo

void EndMatch ( float skip_log )
{
	gedict_t	*p;

	int old_match_in_progress = match_in_progress;
	char *tmp;
	float f1;

	if( match_over || !match_in_progress )
		return;

	match_over = 1;

// s: zero the flag
	k_berzerk = k_sudden_death = 0;

	if( !strnull( tmp = cvar_string( "_k_host" ) ) )
		trap_cvar_set( "hostname", tmp ); // restore host name at match end

	trap_lightstyle(0, "m");

// spec silence
	{
		int fpd = iKey ( world, "fpd" );

		fpd = fpd & ~64;
		localcmd("serverinfo fpd %d\n", fpd);

		cvar_fset("sv_spectalk", 1);
	}

	G_bprint( 2, "The match is over\n");

	EM_CorrectStats();

	if( /* skip_log */ 0 ) { // qqshka: we are not skip match stats now
		;
	}
	else {
       	if( isTeam() || isCTF() )
			CollectTpStats();

		PlayersStats (); // all info about any player
	
		if ( !cvar("k_midair") )
		{
        	if( isTeam() || isCTF() )
				SummaryTPStats (); // print summary stats like armos powerups weapons etc..

        	if( !isDuel() ) // top stats only in non duel modes
				TopStats (); // print top frags tkills deaths...
		}

        if( isTeam() || isCTF() )
			TeamsStats (); // print basic info like frags for each team
		
		if ( (p = find( world, FOFCLSN, "ghost" )) ) // show legend :)
			G_bprint(2, "\n\x83 - %s player\n\n", redtext("disconnected"));

		lastscore_add(); // save game result somewhere, so we can show it later

		StatsToFile();
	}

	for( p = world; (p = find ( p, FOFCLSN, "ghost" )); ) 
		ent_remove( p );

	StopTimer( skip_log ); // WARNING: if we are skip log, we are also delete demo

	for( f1 = 666; k_teamid >= f1 ; f1++ )
		localcmd("localinfo %d \"\"\n", (int)f1); //removing key

	for( f1 = 1; k_userid >= f1; f1++ )
		localcmd("localinfo %d \"\"\n", (int)f1); //removing key

	if ( old_match_in_progress == 2 ) {
		for ( p = world; (p = find_plr( p )); )
			p->ready = 0; // force players be not ready after match is end.
	}

	EM_on_MatchEndBreak( skip_log );

	NextLevel();
}

void SaveOvertimeStats ()
{
	gedict_t	*p;

	if ( k_overtime ){
		for( p = world; (p = find_plr( p )); )
		{
		 	// save overtime stats
			p->ps.ot_a	    = (int)p->s.v.armorvalue;
			p->ps.ot_items	=      p->s.v.items; // float
			p->ps.ot_h	    = (int)p->s.v.health;
		}
	}
}

void CheckBerzerk( int min, int sec )
{
	int bmin, bsec;
	gedict_t	*p;

	if( !k_berzerkenabled || k_berzerk )
		return;

	// transform berzerk seconds to minutes and seconds, so we can guess is time to turn berzerk on
	bmin = k_berzerkenabled / 60;
	bsec = k_berzerkenabled - bmin * 60;
	bmin++;

	if( sec != bsec || min != bmin )
		return;

	G_bprint(2, "%s\n", redtext("BERZERK!!!"));
	trap_lightstyle ( 0, "ob" );
	k_berzerk = 1;
	
	for( p = world; (p = find_plr( p )); ) {
		if( !ISLIVE( p ) )
			continue;

		adjust_pickup_time( &p->q_pickup_time, &p->ps.itm[itQUAD].time );
		adjust_pickup_time( &p->p_pickup_time, &p->ps.itm[itPENT].time );

		p->s.v.items = (int)p->s.v.items | (IT_QUAD | IT_INVULNERABILITY);
		p->super_time = 1;
		p->super_damage_finished = g_globalvars.time + 3600*10;
		p->invincible_time = 1;
		p->invincible_finished = g_globalvars.time + 2;
		p->k_666 = 1;
	}
}

void CheckOvertime()
{
	gedict_t	*timer, *ed1 = get_ed_scores1(), *ed2 = get_ed_scores2();
	int teams   = CountTeams(), players = CountPlayers();
	int sc = get_scores1() - get_scores2();
	int k_mb_overtime = cvar( "k_overtime" );
	int k_exttime = bound(1, cvar( "k_exttime" ), 999); // at least some reasonable values

	// If 0 no overtime, 1 overtime, 2 sudden death
	// And if its neither then well we exit
	if( !k_mb_overtime || (k_mb_overtime != 1 && k_mb_overtime != 2 && k_mb_overtime != 3) )
	{
		EndMatch( 0 );
		return;
	}
	
    // Overtime.
	// Ok we have now decided that the game is ending, so decide overtime wise here what to do.

	if ( (isDuel() || isFFA()) && ed1 && ed2 )
		sc = ed1->s.v.frags - ed2->s.v.frags;

//	if( k_matchLess ) {
//		k_mb_overtime = 0; // no overtime in matchLess mode
//	}
//	else

	if( (isTeam() || isCTF()) && teams != 2 ) {
		k_mb_overtime = 0; // no overtime in case of less then 2 or more then 2 teams
	}			
	else if(    ( (isDuel() || isFFA()) && ed1 && ed2 ) // duel or ffa
			 || ( (isTeam() || isCTF()) && teams == 2 && players > 2 ) // Handle a 2v2 or above team game
	)
	{
		if (    ( k_mb_overtime == 3 && abs( sc ) > 1 ) // tie-break overtime allowed with one frag difference (c) ktpro
			 || ( k_mb_overtime != 3 && abs( sc ) > 0 ) // time based or sudden death overtime allowed with zero frag difference
		   )
		k_mb_overtime = 0;
	}
	else
		k_mb_overtime = 0;

	if( !k_mb_overtime )
	{
		EndMatch( 0 );
		return;
	}

	k_overtime = k_mb_overtime;
	SaveOvertimeStats ();

	G_bprint(2, "time over, the game is a draw\n");

	if( k_overtime == 1 ) {
		// Ok its increase time
		self->cnt  =  k_exttime;
		self->cnt2 = 60;
		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		G_bprint(2, "\x90%s\x91 minute%s overtime follows\n", dig3(k_exttime), count_s(k_exttime));
		self->s.v.nextthink = g_globalvars.time + 1;
	}
	else if ( k_overtime == 2 ) {
		k_sudden_death = SD_NORMAL;
	}
	else {
		k_sudden_death = SD_TIEBREAK;
	}

	if ( k_sudden_death ) {

		G_bprint(2, "%s %s\n", SD_type_str(), redtext("overtime begins"));

		// added timer removal at sudden death beginning
		for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
			ent_remove( timer );
	}
}

// Called every second during a match. cnt = minutes, cnt2 = seconds left.
// Tells the time every now and then.
void TimerThink ()
{
//	G_bprint(2, "left %2d:%2d\n", (int)self->cnt, (int)self->cnt2);

	if( !k_matchLess && !CountPlayers() ) {
		EndMatch( 1 );
		return;
	}

	if( k_sudden_death )
		return;      

	if( k_pause ) { // wtf, all nextthink fields is set to -1 while paused
		self->s.v.nextthink = g_globalvars.time + 1;
		return;
	}

	if( self->k_teamnum < g_globalvars.time && !k_checkx )
		k_checkx = 1; // global which set to true when some time spend after match start

	( self->cnt2 )--;

	CheckBerzerk( self->cnt, self->cnt2 );

	if( !self->cnt2 ) {
		self->cnt2 = 60;
		self->cnt  -= 1;

		localcmd("serverinfo status \"%d min left\"\n", (int)self->cnt);

		if( !self->cnt ) {
			CheckOvertime();
			return;
		}

		G_bprint(2, "\x90%s\x91 minute%s remaining\n", dig3(self->cnt), count_s(self->cnt));

		self->s.v.nextthink = g_globalvars.time + 1;

		if( k_showscores ) {
			int sc = get_scores1() - get_scores2();

			if ( sc ) {
				G_bprint(2, "%s \x90%s\x91 leads by %s frag%s\n",
						redtext("Team"), cvar_string ( ( sc > 0 ? "_k_team1" : "_k_team2" ) ),
						dig3(abs( (int)sc )), count_s( abs( (int)sc ) ) );
			}
			else
				G_bprint(2, "The game is currently a tie\n");
		}
		return;
	}

	if( self->cnt == 1 && ( self->cnt2 == 30 || self->cnt2 == 15 || self->cnt2 <= 10 ) )
		G_bprint(2, "\x90%s\x91 second%s\n", dig3( self->cnt2 ), count_s( self->cnt2 ) );

	self->s.v.nextthink = g_globalvars.time + 1;
}

// remove/add some items from map regardind with dmm and game mode
void SM_PrepareMap()
{
	gedict_t *p;

	if ( isCTF() )
		SpawnRunes( cvar("k_ctf_runes") );

	for( p = world; (p = nextent(p)); ) {

	// going for the if content record..

		if (    streq( p->s.v.classname, "rocket" )
			 || streq( p->s.v.classname, "grenade" )
		   ) { // this must be removed in any cases
				ent_remove( p );
		}
		else if( deathmatch > 3 ) {
			if(    streq( p->s.v.classname, "weapon_nailgun" )
				|| streq( p->s.v.classname, "weapon_supernailgun" )
				|| streq( p->s.v.classname, "weapon_supershotgun" )
				|| streq( p->s.v.classname, "weapon_rocketlauncher" )
				|| streq( p->s.v.classname, "weapon_grenadelauncher" )
				|| streq( p->s.v.classname, "weapon_lightning" )
			  ) { // no weapons for any of this deathmatches (4 or 5)
				ent_remove( p );
			}
			else if ( deathmatch == 4 ) {
				if(    streq( p->s.v.classname, "item_shells" )
					|| streq( p->s.v.classname, "item_spikes" )
					|| streq( p->s.v.classname, "item_rockets" )
					|| streq( p->s.v.classname, "item_cells" )
					|| (streq( p->s.v.classname, "item_health" ) && ( int ) p->s.v.spawnflags & H_MEGA)
			      ) { // no weapon ammo and megahealth for dmm4
					ent_remove( p );
				}
			}
		} else {
			if( deathmatch == 2 && cvar( "k_dm2mod" ) &&
			 							(   streq( p->s.v.classname, "item_armor1" )
			  	 						 || streq( p->s.v.classname, "item_armor2" )
			     						 || streq( p->s.v.classname, "item_armorInv")
                						)
			
			  ) // no armors in modified dmm2
				ent_remove( p );
		}
	}

	ClearBodyQue(); // hide corpses
}

// put clients in server and reset some params
void SM_PrepareClients()
{
	int hdc, i;
	char *pl_team;
	gedict_t *p, *old;

	k_teamid = 666;
	localcmd("localinfo 666 \"\"\n");
	trap_executecmd (); // <- this really needed

	for( p = world;	(p = find_plr( p )); ) {
		if( !k_matchLess ) { // skip setup k_teamnum in matchLess mode
			pl_team = getteam( p );
			p->k_teamnum = 0;

			if( !strnull( pl_team ) ) {
				i = 665;

				while( k_teamid > i && !p->k_teamnum ) {
					i++;

					if( streq( pl_team, ezinfokey(world, va("%d", i)) ) )
						p->k_teamnum = i;
				}

				if( !p->k_teamnum ) { // team not found in localinfo, so put it in
					i++;
					p->k_teamnum = k_teamid = i;
					localcmd( "localinfo %d \"%s\"\n", i, pl_team );
					trap_executecmd (); // <- this really needed
				}
			} 
			else
				p->k_teamnum = 666;
		}

		p->friendly = p->deaths = p->s.v.frags = 0;

		hdc = p->ps.handicap; // save player handicap

		memset( (void*) &( p->ps ), 0, sizeof(p->ps) ); // clear player stats

		p->ps.handicap = hdc; // restore player handicap

		if ( isRA() ) {
			if ( isWinner( p ) || isLoser( p ) )
				setfullwep( p );

			continue;
		}

		old = self;
		self = p;

		SetNewParms( false );
		PutClientInServer();

		self = old;
	}
}

void SM_PrepareShowscores()
{
	gedict_t *p;
	char *team1 = "", *team2 = "";

	if ( k_matchLess ) // skip this in matchLess mode
		return;

	if ( (!isTeam() && !isCTF()) || CountRTeams() != 2 ) // we need 2 teams
		return;

	if ( (p = find_plr( world )) ) 
		team1 = getteam( p );

	if ( strnull( team1 ) )
		return;

	while( (p = find_plr( p )) ) {
		team2 = getteam( p );

		if( strneq( team1, team2 ) )
			break;
	}

	if ( strnull( team2 ) || streq(team1, team2) )
		return;

	k_showscores = 1;

	cvar_set("_k_team1", team1);
	cvar_set("_k_team2", team2);
}

void SM_PrepareHostname()
{
	char *team1 = cvar_string("_k_team1"), *team2 = cvar_string("_k_team2");

	cvar_set( "_k_host", cvar_string("hostname") );  // save host name at match start

	if ( k_showscores && !strnull( team1 ) && !strnull( team2 ) )
		cvar_set("hostname", va("%s (%.4s vs. %.4s)\x87", cvar_string("hostname"), team1, team2));
	else
		cvar_set("hostname", va("%s\x87", cvar_string("hostname")));
}

void SM_PrepareTeamsStats()
{
	int i;

	tmStats_cnt = 0;
	memset(tmStats, 0, sizeof(tmStats));
	memset(tmStats_names, 0, sizeof(tmStats_names));

	for ( i = 0; i < MAX_TM_STATS; i++ )
		tmStats[i].name = tmStats_names[i];
}

void SM_on_MatchStart()
{
	gedict_t *p;

	for( p = world; (p = find_client( p )); )
		on_match_start( p );
}

// Reset player frags and start the timer.
void StartMatch ()
{
	char *tm;

	k_berzerk    = 0;
	k_nochange   = 0;
	k_showscores = 0;
	k_standby    = 0;
	k_checkx     = 0;

	k_userid   = 1;
	localcmd("localinfo 1 \"\"\n");
	trap_executecmd (); // <- this really needed

	// Check to see if berzerk is set.
	k_berzerkenabled = (cvar( "k_bzk" ) ? bound(0, cvar( "k_btime" ), 3600*10) : 0);

	first_rl_taken = false; // no one took rl yet

	SM_PrepareMap(); // remove/add some items from map regardind with dmm and game mode
	
	match_start_time  = g_globalvars.time;
	match_in_progress = 2;
	
	lastTeamLocationTime = -TEAM_LOCATION_UPDATE_TIME; // update on next frame

	remove_specs_wizards (); // remove wizards

	SM_PrepareClients(); // put clients in server and reset some params

	if ( !strnull( tm = ezinfokey(world, "date_str") ) )
		G_bprint(2, "matchdate: %s\n", tm);

	if ( !k_matchLess || cvar( "k_matchless_countdown" ) )
		G_bprint(2, "%s\n", redtext("The match has begun!"));

// spec silence
	{ 
		int fpd = iKey( world, "fpd" );
		int k_spectalk = bound(0, cvar( "k_spectalk" ), 1);
		cvar_fset( "sv_spectalk", k_spectalk );

		fpd = ( k_spectalk ) ? (fpd & ~64) : (fpd | 64);

		localcmd( "serverinfo fpd %d\n", fpd );
	}

	self->k_teamnum = g_globalvars.time + 3;  //dirty i know, but why waste space?
											  // FIXME: waste space, but be clean
	self->cnt = bound(0, timelimit, 9999);
	self->cnt2 = 60;
	localcmd("serverinfo status \"%d min left\"\n", (int)timelimit);

	self->s.v.think = ( func_t ) TimerThink;
	self->s.v.nextthink = g_globalvars.time + 1;

	SM_PrepareShowscores();

	SM_PrepareHostname();

	SM_PrepareTeamsStats();

	SM_on_MatchStart();

	if ( !self->cnt )
		ent_remove( self ); // timelimit == 0, so match will end no due to timelimit but due to fraglimit or something
}

void PrintCountdown( int seconds )
{
// Countdown: seconds
//
//
// Deathmatch  x
// Mode		  D u e l | T e a m | F F A | C T F | RA
// Midair     On // optional
// Jawnmode   On // optional
// Airstep    On // optional
// TmOverlay  On // optional
// Teamplay    x
// Timelimit  xx
// Fraglimit xxx
// Overtime   xx		Overtime printout, supports sudden death display
// Powerups   On|Off|Jammed
// Dmgfrags   On // optional
// Noweapon


	char text[1024] = {0};
	char *mode = "";
	char *pwr  = "";
	char *ot   = "";
	char *nowp = "";


	strlcat(text, va("%s: %2s\n\n\n", redtext("Countdown"), dig3(seconds)), sizeof(text));
	if ( !isRA() ) // useless in RA
		strlcat(text, va("%s %2s\n", "Deathmatch", dig3(deathmatch)), sizeof(text));

	if ( isRA() )
		mode = redtext("RA");
	else if( isDuel() )
		mode = redtext("D u e l");
	else if ( isTeam() )
		mode = redtext("T e a m");
	else if ( isFFA() )
		mode = redtext("F F A");
	else if ( isCTF() )
		mode = redtext("C T F");
	else
		mode = redtext("Unknown");

	strlcat(text, va("%s %8s\n", "Mode", mode), sizeof(text));

	if ( cvar("k_midair") )
		strlcat(text, va("%s %6s\n", "Midair", redtext("On")), sizeof(text));

	if ( k_jawnmode )
		strlcat(text, va("%s %4s\n", "Jawnmode", redtext("On")), sizeof(text));

	if ( cvar("pm_airstep") )
		strlcat(text, va("%s %5s\n", "Airstep", redtext("On")), sizeof(text));

	if ( cvar("k_teamoverlay") )
		strlcat(text, va("%s %3s\n", "TmOverlay", redtext("On")), sizeof(text));

	if ( !isRA() ) // useless in RA
	if ( isTeam() || isCTF() )
		strlcat(text, va("%s %4s\n", "Teamplay", dig3(teamplay)), sizeof(text));
	if ( timelimit )
		strlcat(text, va("%s %3s\n", "Timelimit", dig3(timelimit)), sizeof(text));
	if ( fraglimit )
		strlcat(text, va("%s %3s\n", "Fraglimit", dig3(fraglimit)), sizeof(text));

	switch ( (int)cvar( "k_overtime" ) ) {
		case 0:  ot = redtext("Off"); break;
		case 1:  ot = dig3( cvar( "k_exttime" ) ); break;
		case 2:  ot = redtext("sd"); break;
		case 3:  ot = va("%s %s", dig3(tiecount()),redtext("tb")); break;
		default: ot	= redtext("Unkn"); break;
	}

	if ( timelimit && cvar( "k_overtime" ) )
		strlcat(text, va("%s %4s\n", "Overtime", ot), sizeof(text));

	switch ( Get_Powerups() ) {
		case 0:  pwr = redtext("Off"); break;
		case 1:  pwr = redtext( "On"); break;
		case 2:  pwr = redtext("Jam"); break;
		default: pwr = redtext("Unkn"); break;
	}

	if ( !isRA() || Get_Powerups() ) // show powerups in RA ?
		strlcat(text, va("%s %4s\n", "Powerups", pwr), sizeof(text));

	if ( cvar("k_dmgfrags") )
		strlcat(text, va("%s %4s\n", "Dmgfrags", redtext("On")), sizeof(text));

	if (    deathmatch == 4 && !cvar("k_midair")
		 && !strnull( nowp = str_noweapon((int)cvar("k_disallow_weapons") & DA_WPNS) )
	   )
		strlcat(text, va("\n%s %4s\n", "Noweapon", 
					redtext(nowp[0] == 32 ? (nowp+1) : nowp)), sizeof(text));

	G_cp2all(text);
}

qboolean isCanStart ( gedict_t *s, qboolean forceMembersWarn )
{
    int k_lockmin     = cvar( "k_lockmin" );
    int k_lockmax     = cvar( "k_lockmax" );
	int k_membercount = cvar( "k_membercount" );
	int i = CountRTeams();
	int sub, nready;
	char *txt = "";
	gedict_t *p;

	if ( !isTeam() && !isCTF() ) // no rules limitation in non team game
		return true;

    if( i < k_lockmin )
    {
		sub = k_lockmin - i;
		txt = va("%d more team%s required!\n", sub, ( sub != 1 ? "s" : "" ));

		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

        return false;
    }

    if( i > k_lockmax )
    {
		sub = i - k_lockmax;
		txt = va("Get rid of %d team%s!\n", sub, ( sub != 1 ? "s" : "" ));

		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

        return false;
    }

	nready = 0;
	for( p = world; (p = find_plr( p )); )
		if( p->ready )
			nready++;

	if ( !CheckMembers( k_membercount ) ) {
		if( !forceMembersWarn ) // warn anyway if we want
		if( nready != k_attendees && !s )
			return false; // inform not in all cases, less annoying

		txt = va("%s %d %s\n"
				 "%s\n",
			 redtext("Server wants at least"), k_membercount, redtext("players in each team"),
			 redtext("Waiting..."));
					
		if ( s )
        	G_sprint(s, 2, "%s", txt);
		else
        	G_bprint(2, "%s", txt);

		return false;
	}

	if ( isCTF() )
	{
		// can't really play ctf if map doesn't have flags
		gedict_t *rflag = find( world, FOFCLSN, "item_flag_team1" );
		gedict_t *bflag = find( world, FOFCLSN, "item_flag_team2" );
       
		if ( !rflag || !bflag )
		{
			txt = "This map does not support CTF mode\n";

			if ( s )
        		G_sprint(s, 2, "%s", txt);
			else
        		G_bprint(2, "%s", txt);

			return false;
		}  
	}

	return true;
}

void standby_think()
{
	gedict_t *p;

	if ( match_in_progress == 1 && !isRA() ) {

		k_standby = 1;

		for( p = world;	(p = find_plr( p )); ) {
			if( !strnull ( p->s.v.netname ) ) {
				//set to ghost, 0.2 second before matchstart
				p->s.v.takedamage = 0;
				p->s.v.solid      = 0;
				p->s.v.movetype   = 0;
				p->s.v.modelindex = 0;
				p->s.v.model      = "";
			}
		}
	}

	ent_remove ( self );
}

// Called every second during the countdown.
void TimerStartThink ()
{
	gedict_t *p;

	k_attendees = CountPlayers();

	if( !isCanStart( NULL, true ) ) {
		G_bprint(2, "Aborting...\n");

		StopTimer( 1 );

		return;
	}

	self->cnt2 -= 1;

	if( self->cnt2 == 1 ) {
		p = spawn();
		p->s.v.owner = EDICT_TO_PROG( world );
		p->s.v.classname = "standby_th";
		p->s.v.nextthink = g_globalvars.time + 0.8;
		p->s.v.think = ( func_t ) standby_think;
	}
    else if( self->cnt2 <= 0 ) {
		G_cp2all("");

		StartMatch();

		return;
	}

	PrintCountdown( self->cnt2 );

	if( self->cnt2 < 6 )
		for( p = world; (p = find_client( p )); )
			stuffcmd (p, "play buttons/switch04.wav\n");

	self->s.v.nextthink = g_globalvars.time + 1;
}


void ShowMatchSettings()
{
	int i;
	char *txt = "";

	G_bprint(2, "Spawnmodel: %s\n", redtext( respawn_model_name( cvar( "k_spw" ) ) ));

// changed to print only if other than default

	if( (i = get_fair_pack()) ) {
		// Output the Fairpack setting here
		switch ( i ) {
			case  0: txt = "off"; break;
			case  1: txt = "best weapon"; break;
			case  2: txt = "last weapon fired"; break;
			default: txt = "!Unknown!"; break;
		}

		G_bprint(2, "Fairpacks setting: %s\n", redtext(txt));
	}

// print qizmo ( FPD ) settings
	i = iKey( world, "fpd" );
	if( i & 170 ) {
		char buf[256] = {0};

		if( i & 2 )
			strlcat(buf, " timer", sizeof(buf));
		if( i & 8 )
			strlcat(buf, " lag", sizeof(buf));
		if( i & 32 )
			strlcat(buf, " enemy", sizeof(buf));
		if( i & 128 )
			strlcat(buf, " point", sizeof(buf));

		G_bprint(2, "QiZmo:%s disabled\n", redtext(buf));
	}
}

// duel_dag_vs_zu-zu[dm3]
// team_no!_vs_fom[dm3]
// ctf_no!_vs_fom[dm3]
// ffa_10[dm3] // where 10 is count of players
// ra_10[dm3] // where 10 is count of players
// unknown_10[dm3] // where 10 is count of players

char *CompilateDemoName ()
{
	static char demoname[512];
	char date[128], *fmt;

	int i;
	gedict_t *p;
	char *name, *vs;

	demoname[0] = 0;

	if ( isRA() ) {
		strlcat( demoname, va("ra_%d", (int)CountPlayers()), sizeof( demoname ) );
	}
	else if ( isDuel() ) {
		strlcat( demoname, "duel", sizeof( demoname ) );
		if ( cvar("k_midair") )
			strlcat( demoname, "_midair", sizeof( demoname ) );

		for( vs = "_", p = world; (p = find_plr( p )); ) 
		{
			if ( strnull( name = getname( p ) ) )
				continue;

			strlcat( demoname, vs, sizeof( demoname ) );
			strlcat( demoname, name, sizeof( demoname ) );
			vs = "_vs_";
		}
	}
	else if ( isTeam() || isCTF() ) {
		char teams[MAX_CLIENTS][MAX_TEAM_NAME];
		int cnt = getteams(teams);
		int clt = cvar("maxclients"); //CountPlayers();

		// guess is this XonX
		if ( clt > 1 && cnt > 1 && !(clt % cnt) )
			clt /= cnt; // yes
		else
			clt = 0; // no

		strlcat( demoname, (isTeam() ? (clt ? va("%don%d", clt, clt) : "team"): "ctf"), sizeof( demoname ) );

		for( vs = "_", i = 0; i < MAX_CLIENTS; i++ ) 
		{
			if ( strnull( teams[i] ) )
				break;

			strlcat( demoname, vs, sizeof( demoname ) );
			strlcat( demoname, teams[i], sizeof( demoname ) );
			vs = "_vs_";
		}
	}
	else if ( isFFA() ) {
		strlcat( demoname, va("ffa_%d", (int)CountPlayers()), sizeof( demoname ) );
	}
	else {
		strlcat( demoname, va("unknown_%d", (int)CountPlayers()), sizeof( demoname ) );
	}

	strlcat( demoname, va("[%s]", g_globalvars.mapname), sizeof( demoname ) );

	fmt = cvar_string( "k_demoname_date" );

	if ( !strnull( fmt ) && QVMstrftime(date, sizeof(date), fmt, 0) )
		strlcat( demoname, date, sizeof( demoname ) );

	return demoname;
}

void StartDemoRecord ()
{
	if ( cvar( "demo_tmp_record" ) ) { // FIXME: TODO: make this more like ktpro
		qboolean record = false;

		if ( isFFA() && cvar( "demo_skip_ktffa_record" ) )
			record = false;
		else
			record = true;

		if ( record ) {
			if( !strnull( cvar_string( "serverdemo" ) ) )
				localcmd("cancel\n");  // demo is recording, cancel before new one

			localcmd( "easyrecord \"%s\"\n", CompilateDemoName() );
		}
	}
}

// Spawns the timer and starts the countdown.
void StartTimer ()
{
	gedict_t *timer;

	if ( match_in_progress || intermission_running || match_over )
		return;

	if ( k_matchLess && !CountPlayers() )
		return; // can't start countdown in matchless mode due to no players,

	k_force = 0;

	for( timer = world; (timer = find(timer, FOFCLSN, "idlebot")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "standby_th")); )
		ent_remove( timer );

	if ( !k_matchLess ) {
		ShowMatchSettings ();

		for( timer = world; (timer = find_client( timer )); )
			stuffcmd(timer, "play items/protect2.wav\n");
	}

	timer = spawn();
	timer->s.v.owner = EDICT_TO_PROG( world );
	timer->s.v.classname = "timer";
	timer->cnt = 0;

    timer->cnt2 = max(3, (int)cvar( "k_count" ));  // at the least we want a 3 second countdown

	if ( k_matchLess ) // check if we need countdown in case of matchless
		if ( !cvar("k_matchless_countdown") )
			timer->cnt2 = 0; // ok - no countdown

	( timer->cnt2 )++;

    timer->s.v.nextthink = g_globalvars.time + 0.001;
	timer->s.v.think = ( func_t ) TimerStartThink;

	match_in_progress = 1;

	localcmd( "serverinfo status Countdown\n" );

	StartDemoRecord (); // if allowed
}

// Whenever a countdown or match stops, remove the timer and reset everything.
// also stop/cancel demo recording
void StopTimer ( int removeDemo )
{
	gedict_t *timer, *p;
	int k_demo_mintime = bound(0, cvar("k_demo_mintime"), 3600);

	if ( k_demo_mintime <= 0 )
		k_demo_mintime = 120; // 120 seconds is default

	if ( match_in_progress == 1 )
		G_cp2all(""); // clear center print

	k_force = 0;
	match_in_progress = 0;

	if ( k_standby )
	{
		// Stops the bug where players are set to ghosts 0.2 second to go and countdown aborts.
		// standby flag needs clearing (sturm)
		k_standby = 0;

		for( p = world; (p = find_plr( p )); ) 
		{
			p->s.v.takedamage = 2;
			p->s.v.solid      = 3;
			p->s.v.movetype   = 3;
			setmodel (p, "progs/player.mdl");
		}
	}

	for( timer = world; (timer = find(timer, FOFCLSN, "timer")); )
		ent_remove( timer );

	for( timer = world; (timer = find(timer, FOFCLSN, "standby_th")); )
		ent_remove( timer );

	if (   removeDemo 
		&& ( !match_start_time || (g_globalvars.time - match_start_time ) < k_demo_mintime )
		&& !strnull( cvar_string( "serverdemo" ) )
	   )
		localcmd("cancel\n");  // demo is recording and must be removed, do it

	match_start_time = 0;

	localcmd("serverinfo status Standby\n");
}

void IdlebotForceStart ()
{
    gedict_t *p;
    int i;

    G_bprint ( 2, "server is tired of waiting\n"
				  "match WILL commence!\n" );

    i = 0;
    for( p = world; (p = find_plr( p )); )
    {
		if( p->ready ) {
    		i++;
		}
		else
		{
    		G_bprint(2, "%s was kicked by IDLE BOT\n", p->s.v.netname);
    		G_sprint(p, 2, "Bye bye! Pay attention next time.\n");

    		stuffcmd(p, "disconnect\n"); // FIXME: stupid way
		}
    }

    k_attendees = i;

    if( k_attendees > 1 ) {
        StartTimer();
	}
    else
    {
        G_bprint(2, "Can't start! More players needed.\n");
		EndMatch( 1 );
    }
}

void IdlebotThink ()
{
	gedict_t *p;
	int i;

	if ( cvar( "k_idletime" ) <= 0 ) {
		ent_remove( self );
		return;
	}

	self->attack_finished -= 1;

	i = CountPlayers();

	if( 0.5f * i > CountRPlayers() || i < 2 ) {
		G_bprint(2, "console: bah! chickening out?\n"
					"server disables the %s\n", redtext("idle bot"));

		ent_remove( self ) ;

		return;
	}

	k_attendees = CountPlayers();

	if ( !isCanStart(NULL, true) ) {
        G_bprint(2, "%s removed\n", redtext("idle bot"));

        ent_remove ( self );

        return;
	}

	if( self->attack_finished < 1 ) {

		IdlebotForceStart();

		ent_remove( self );

		return;

	} else {
		i = self->attack_finished;

		if( i < 5 || !(i % 5) ) {
			for( p = world; (p = find_plr( p )); )
				if( !p->ready )
					G_sprint(p, 2, "console: %d second%s to go ready\n", i, ( i == 1 ? "" : "s" ));
		}
	}

	self->s.v.nextthink = g_globalvars.time + 1;
}

void IdlebotCheck ()
{
	gedict_t *p;
	int i;

	if ( cvar( "k_idletime" ) <= 0 ) {
		if ( (p = find ( world, FOFCLSN, "idlebot" )) )
			ent_remove( p );
		return;
	}

	i = CountPlayers();

	if( 0.5f * i > CountRPlayers() || i < 2 ) {
		p = find ( world, FOFCLSN, "idlebot" );

		if( p ) {
			G_bprint(2, "console: bah! chickening out?\n"
						"server disables the %s\n", redtext("idle bot"));

			ent_remove( p );
		}

		return;
	} 

	if( match_in_progress || intermission_running || k_force )
		return;

	// no idele bot in practice mode
	if ( k_practice ) // #practice mode#
		return;

	if( (p = find ( world, FOFCLSN, "idlebot" )) ) // already have idlebot
		return;

	//50% or more of the players are ready! go-go-go

	k_attendees = CountPlayers();

	if ( !isCanStart( NULL, true ) ) {
        G_sprint(self, 2, "Can't issue %s!\n", redtext("idle bot"));
		return;
	}

	p = spawn();
	p->s.v.classname = "idlebot";
	p->s.v.think = (func_t) IdlebotThink;
	p->s.v.nextthink = g_globalvars.time + 0.001;

	p->attack_finished = max( 3, cvar( "k_idletime" ) );

	G_bprint(2, "\n"
				"server activates the %s\n", redtext("idle bot"));
}

void CheckAutoXonX(qboolean use_time);

// Called by a player to inform that (s)he is ready for a match.
void PlayerReady ()
{
	gedict_t *p;
	float nready;

	if ( self->ct == ctSpec ) {

		if ( !cvar("k_auto_xonx") ) {
			G_sprint(self, 2, "Command not allowed\n");
			return;
		}

		if( self->ready ) {
			G_sprint(self, 2, "Type break to unready yourself\n");
			return;
		}

		self->ready = 1;

		for( p = world; (p = (match_in_progress ? find_spc( p ) : find_client( p ))); )
			G_sprint(p, 2, "%s %s to play\n", self->s.v.netname, redtext("desire"));

		CheckAutoXonX(g_globalvars.time < 10 ? true : false); // forse switch mode asap if possible after some time spend
		
		return;
	}

	if( intermission_running || match_in_progress == 2 || match_over )
			return;

	if ( k_practice ) { // #practice mode#
		G_sprint(self, 2, "%s\n", redtext("Server in practice mode"));
		return;
	}

	if( self->ready ) {
		G_sprint(self, 2, "Type break to unready yourself\n");
		return;
	}
        
    if ( isCTF() )
	{
		if ( !streq(getteam(self), "red") && !streq(getteam(self), "blue") )
		{
			G_sprint( self, 2, "You must be on team red or blue for CTF\n" );
			return;
		}
	}

    if( k_force && ( isTeam() || isCTF() )) {
		nready = 0;
		for( p = world; (p = find_plr( p )); ) {
			if( p->ready ) {
				if( streq( getteam(self), getteam(p) ) && !strnull( getteam(self) ) ){
					nready = 1;
					break;
				}
			}
		}

		if( !nready ) {
			G_sprint(self, 2, "Join an existing team!\n");
			return;
		}
	}

	if ( GetHandicap(self) != 100 )
		G_sprint(self, 2, "\x87%s you are using handicap!\n", redtext( "WARNING:" ));

	self->ready = 1;
	self->v.brk = 0;
	self->k_teamnum = 0;

	// force red or blue color if ctf
	if ( isCTF() )
	{
		if ( streq( getteam(self), "blue" ) )
			stuffcmd( self, "color 13\n" );
		else if ( streq( getteam(self), "red" ) )
			stuffcmd( self, "color 4\n" );
	}

	G_bprint(2, "%s %s%s\n", self->s.v.netname, redtext("is ready"),
						( ( isTeam() || isCTF() ) ? va(" \x90%s\x91", getteam( self ) ) : "" ) );

	nready = 0;
	for( p = world; (p = find_plr( p )); )
		if( p->ready )
			nready++;

	k_attendees = CountPlayers();

	if ( !isCanStart ( NULL, false ) )
		return; // rules does't allow us to start match, idlebot ignored because of same reason

	if ( k_force )
		return; // admin forces match - timer will started somewhere else

	if( nready != k_attendees ) { // not all players ready, check idlebot and return

		IdlebotCheck();

		return;
	}
	
	// ok all players ready

	if ( nready < 2 ) // only one or less players ready, match is pointless
		return;

	G_bprint(2, "All players ready\n"
				"Timer started\n");

	StartTimer();
}

void PlayerBreak ()
{
	int votes;
	gedict_t *p;

	if ( self->ct == ctSpec ) {

		if ( !cvar("k_auto_xonx") || k_matchLess ) {
			G_sprint(self, 2, "Command not allowed\n");
			return;
		}

		if( !self->ready )
			return;

		self->ready = 0;

		for( p = world; (p = (match_in_progress ? find_spc( p ) : find_client( p ))); )
			G_sprint(p, 2, "%s %s to play\n", self->s.v.netname, redtext("lost desire"));
		
		return;
	}

	if( !self->ready || intermission_running || match_over )
		return;

	if ( k_matchLess )
	if ( cvar("k_no_vote_map") ) {
		G_sprint(self, 2, "Voting next map is %s allowed\n", redtext("not"));
		return;
	}

	if( !match_in_progress ) {
		self->ready = 0;

		G_bprint(2, "%s %s\n", self->s.v.netname, redtext("is not ready"));

		return;
	}

	if( !k_matchLess ) // u can't stop countdown (but match u can) in matchless mode
	if( match_in_progress == 1 ) {
		p = find ( world, FOFCLSN, "timer");
		if( p && p->cnt2 > 1 ) {
			self->ready = 0;

			G_bprint(2, "%s %s\n", self->s.v.netname, redtext("stops the countdown"));

			StopTimer( 1 );
		}
		return;
	}

	if( self->v.brk ) {
		self->v.brk = 0;

		G_bprint(2, "%s %s %s vote%s\n", self->s.v.netname,
				redtext("withdraws"), redtext(g_his(self)),
				((votes = get_votes_req( OV_BREAK, true )) ? va(" (%d)", votes) : ""));

		return;
	}

	self->v.brk = 1;

	
	G_bprint(2, "%s %s%s\n", self->s.v.netname, redtext(k_matchLess ? "votes for next map" : "votes for stopping the match"),
				((votes = get_votes_req( OV_BREAK, true )) ? va(" (%d)", votes) : ""));


	// blocking stop countdown in matchless mode by one player
	if ( CountPlayers() == 1 && k_matchLess && match_in_progress == 1 ) {
		G_sprint(self, 2, "You can't stop countdown alone\n");
		return;
	}

	vote_check_break ();
}

