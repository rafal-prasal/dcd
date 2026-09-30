/* $Id: misc.h,v 1.68 2007/02/19 20:59:19 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __vXhz__MISC_H__INCLUDED__ )
#define __vXhz__MISC_H__INCLUDED__

// various defines:
extern int NUSERS;			/* current number of users */
extern time_t starttime;	/* start time in sec */

// lock generator: 'salt'
void lock_gen( char *lock);

// lock generator: 'salt'
void lock_key(userrec_t *usr, char *lock);

// feed a random numbers generator
void randomize( void );

// convert user's nick to table index
int nick2id( char *nick );

// set/clear a user's nick, keeping the nick->id hash in sync
// (frees the old nick string; caller must hold mutex_users)
void user_set_nick( userrec_t *usr, char *nick );

// drop a user's nick key before its string is freed (disconnect path)
void nick_hash_forget( userrec_t *usr );

// re-home a nick key after userlist_del() moved the user to a new index
void nick_hash_rehome( char *nick, int id );

// convert hub's ip to table index
int ip2hub( char *ip );

// usr2id() - convert user's data pointer to table index
int usr2id( userrec_t *usr );

// check if a given nick is a valid one
char validnick(userrec_t *usr, char *nick );

// check for a valid $Key sequence
char validkey(userrec_t *usr, char *key );

// fill user's record with data received in $MyINFO
int fill_myinfo( userrec_t *usr, myinfo_t *myinfo, char *info, char *line );
int compare_myinfo( myinfo_t *myinfo0, myinfo_t *myinfo1 );
void free_myinfo( myinfo_t *myinfo );
char *flag_myinfo( myinfo_t *myinfo );
void move_myinfo( myinfo_t *from, myinfo_t *to);

// check clients' idle time
void check_timeout(userrec_t *usr );

// from openssh
char *get_progname( char *argv0 );

// allocate a sufficiently large string and print into it - simplified version
char *my_vsprintf( const char *fmt, va_list ap );

// allocate a sufficiently large string and print into it - simplified version
char *my_sprintf( const char *fmt, ... );

// compose direct path to file
void make_path( char *dir, char **file );

// compose direct paths to files for further use
void make_paths( config_t *conf );

// return string representing daemon's uptime
void uptime( time_t starttime, char *str );

/* controlled realloc */
void *my_realloc( void *ptr,size_t size );

/* controlled malloc */
void *my_malloc( size_t size );

/* a semaphored inet_ntoa */
char *my_inet_ntoa( struct in_addr addr );

/* memleak controlled strdup()*/
void my_duplicate(char *str, char **duplicate);
//void my_duplicate1(char *str, char **duplicate,char *plik, int linia);
//#define my_duplicate(x,y) my_duplicate1(x,y,__FILE__,__LINE__)

/* controlled free */
void my_free1( void *ptr );
#define my_free(x) my_free1(&(x))

int add2buf(char **buf, char *string);

#endif /* __vXhz__MISC_H__INCLUDED__ */

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}} */

