/* Stubs for the few src/main.c functions referenced by other units.
   Kept faithful to the originals in src/main.c (state bits only; no
   side effects like socket teardown that belong to the daemon core). */

#include "pch.h"

extern userrec_t *user[];
extern int NUSERS;
extern pthread_mutex_t mutex_users;

int user_tst_state(userrec_t *usr, int state)
{
	return ((usr->state)&state)>0;
}

int user_set_state(userrec_t *usr, int state)
{
	if (user_tst_state(usr,STATE_QUIT)) return 0;

	if (state==STATE_QUIT)
	{
		if (user_tst_state(usr,STATE_REGISTERED|STATE_LOGGEDIN)) usr->state=STATE_QUIT_SHOW;
		else usr->state=STATE_QUIT;
	}
	else usr->state=state;

	return 0;
}

int user_set_supports(userrec_t *usr, int supports)
{
	usr->supports=usr->supports|supports;
	return 0;
}

int user_tst_supports(userrec_t *usr, int supports)
{
	return ((usr->supports)&supports)>0;
}

void userlist_add(userrec_t *usr)
{
	pthread_mutex_lock( &mutex_users );
	user[NUSERS]=usr;
	user[NUSERS]->id=NUSERS;
	NUSERS++;
	pthread_mutex_unlock( &mutex_users );
}

void userlist_del(userrec_t *usr)
{
	int id;

	pthread_mutex_lock( &mutex_users );
	id=usr->id;
	user[id]=user[--NUSERS];
	user[id]->id=id;
	user[NUSERS]=NULL;
	pthread_mutex_unlock( &mutex_users );
}
