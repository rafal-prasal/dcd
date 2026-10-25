/* $Id: main.h,v 1.164 2007/02/20 08:30:58 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#if !defined( __MAIN_H__INCLUDED__ )
#define __MAIN_H__INCLUDED__

#if !defined( BUILD_TIME )
#define BUILD_TIME "unknown"
#endif /* BUILD_TIME */

/* default configuration {{{ */

/* maximum sizes (for security)		*/
#define MAXBLOCK	1024*1024

/* maximum amount of data to read from socket */
#define MAX_RECEIVE_ONCE 1024

/* maximum buffer which is used for temporary stored user commands */
#define MAX_USER_BUFFER 2*MAX_RECEIVE_ONCE

#define NICK_LEN	32
#define VER_LEN		32
#define DESC_LEN	256
#define CTYPE_LEN	64
#define EMAIL_LEN	64
#define SHARE_LEN	30

#define SALT_LEN	100

/* main config file			*/
#define CONF_MAIN	"dcd.conf"

/* welcome message file			*/
#define CONF_WELCOME	"dcd.welcome"

/* Message Of The Day file		*/
#define CONF_MOTD	"dcd.motd"

/* bans file				*/
#define CONF_BANNED	"dcd.banned"

/* password file			*/
#define CONF_CUSERS	"dcd.users"

/* password file			*/
#define CONF_HUBLINKS	"dcd.hublinks"

/* user commands file	*/
#define CONF_USERCOMMANDS	"dcd.usercommands"

/* console host access file		*/
#define CONF_CALLOW	"console.allow"

/* allowed nicks file			*/
#define CONF_NALLOW	"nicks.allow"

/* a penalties file	*/
#define CONF_PENALTIES	"dcd.penalties"

/* a rules file	*/
#define CONF_RULES	"dcd.rules"

/* service listening interface	*/
#define LISTEN_INTERFACE "0.0.0.0"

/* main service listening port number	*/
#define LISTEN_PORT_MAIN 411

/* remote console listening port	*/
#define LISTEN_PORT_CONS 511

/* contact to administrator of hub. this is displayed when user is not allowed to connect to hub*/
#define ADMIN_CONTACT "god@heaven.com"

#define MAXPENALTIES 100*MAXUSERS

/* limit for number of users (dc protocol level); <=MAXUSERS (!!!)	*/
#define USERLIMIT	 1000

/* name of our hub, obviously		*/
#define HUBNAME	"DConnectDaemon"

/* set to 1, if hub should allow clients to connect even when they reply
 * with an invalid $Key string		*/
#define ALLOW_BROKEN_KEY 0

/* allow chats or not - default yes	*/
#define ALLOW_CHAT	 1

/* allow using nt us ascii characters */
#define ALLOW_NON_US_ASCII_NICKS	0

/* allow chats or not - default yes	*/
#define ALLOW_PASSIVE	 1

/* allow chats or not - default no	*/
#define ALLOW_FORWARDING	 0

/* allow_downloads */
#define ALLOW_DOWNLOADS 1

/* allow_search */
#define ALLOW_SEARCH 1

/* registered_only */
#define REGISTERED_ONLY 0

/* standard penalty duration in minutes */
#define STD_PENALTY_DURATION	10

/* ping timeout duration in seconds; 0 = no timeout	*/
#define PING_TIMEOUT 0

/* amount of bytes for SEND_BUF	*/
#define VAL_SO_SNDBUF 10*1024

/* amount of bytes for RCV_BUF	*/
#define VAL_SO_RCVBUF 10*1024

/* registration time limit in seconds; 0 = no limit	*/
#define REGISTER_TIMEOUT 30

/* minimal time of waiting before managing users communicates*/
#define MINIMAL_SLEEP_TIME 0

/* minimal interval for updating dcd.penalties file */
#define PENALTIES_UPDATE_INTERVAL 30

/* minimal interval for searching */
#define SEARCH_INTERVAL 30

/* interval for cleaner thread */
#define CLEANER_INTERVAL 5

/* maximal amount of linked hubs */
#define MAXHUBS 100

/* maximum amount of commands which can be executed by hub at once for each user */
#define MAX_COMMANDS 100

#define MAX_N_LISTEN_MAIN	5 // maximal amount of main_port listening threads

#define N_LISTEN_MAIN	2 // default amount of main_port listening threads

#define MAX_N_USER_MANAGER	10 // maximal amount of main_port listening threads

#define N_USER_MANAGER	1 // default amount of main_port listening threads

#define MAX_CHAT_LENGTH 0 //maximal chat message length

#define KICK_MAX_CHAT_LENGTH 0 //maximal chat message length

#define REDIRECT_ACCESS "127.0.0.1" // if user has no access

#define REDIRECT_HUB_IS_FULL "127.0.0.1" // if hub is full

#define REDIRECT_MINSHARE "127.0.0.1" // if user does not reach minimum number of shares

#define REDIRECT_MINSLOTS "127.0.0.1" // if user does not reach minimum number of slots

/* end of default values }}} */

/* LINE / line flags {{{ */
#define LINE_NORMAL 0
#define LINE_PUNISHMENT 1
/* }}} */

/* REDIRECT / redirect flags {{{ */
#define REDIRECT_SWITCH_OFF 0
#define REDIRECT_SWITCH_ACCESS 1
#define REDIRECT_SWITCH_HUB_IS_FULL 2
#define REDIRECT_SWITCH_MINSHARE 4
#define REDIRECT_SWITCH_MINSLOTS 8
/* }}} */


/* STATE / user flags {{{ */
#define STATE_QUIT			1 // quiting without notice
#define STATE_SHOW_QUIT		2 // this will be tested whether to sendinfo on quiting
#define STATE_QUIT_SHOW		3 // user will quit and send info

#define STATE_CONNECTED		4 // user is connected
#define STATE_KEY			8 // user sent valid key
#define STATE_PASSWORD		16 // user has to send password
#define STATE_LOGGEDIN		32 // user logged in
#define STATE_SR			64 // hub is testing minslots on user
#define STATE_SLOTS_TESTED	128 // hub is testing minslots on user
#define STATE_REGISTERED	256 // user send valid info
#define STATE_WELCOME 		512 // send WELCOME to user
/* }}} */

/* NICK / nick validation errors {{{ */
#define NICK_IS_OK 0
#define NICK_IS_TOO_LONG 1
#define NICK_IS_WITH_INVALID_CHARACTERS 2
#define NICK_REQUIRES_PASSWORD 3
#define NICK_IS_ALREADY_USED 4
#define NICK_IS_RESERVED 5
#define NICK_IS_NOT_IN_PATTERNS 6
#define NICK_IS_EMPTY 7
/* }}} */

/* MyINFO / $MyINFO validation errors {{{ */
#define MyINFO_OK 0
#define MyINFO_ALL 1
#define MyINFO_NICK 2
#define MyINFO_DESC 3
#define MyINFO_SPACE 4
#define MyINFO_SPEED 5
#define MyINFO_FLAG 6
#define MyINFO_EMAIL 7
#define MyINFO_SHARE 8
/* }}} */

/* SUPPORTS / what of extended protocol is supported {{{ */
#define SUPPORTS_NONE 0
#define SUPPORTS_NoHello 1
#define SUPPORTS_NoGetINFO 2
#define SUPPORTS_NoHello_NoGetINFO 3
#define SUPPORTS_UserIP2 4
#define SUPPORTS_UserCommand 8
/* }}} */

/* record with penalties data */
typedef struct
{
	char *ip;
	char *nick;
	char *op; // name of the op who punished user
	int penalty;
	unsigned long start_date;	// start date of punishment
	unsigned long end_date;	// end date of punishment
	char *reason;
} Tpenalty;


typedef struct
{
	char desc[DESC_LEN];
	char ctype[CTYPE_LEN];			// connection type
	char flag;
	char email[EMAIL_LEN];
	uint64_t share;

	char cache[13+NICK_LEN+1+DESC_LEN+3+CTYPE_LEN+1+1+EMAIL_LEN+1+SHARE_LEN+2+1];
}myinfo_t;

/* record with user's data */
typedef struct
{
	unsigned int sock;
	char *buf; // uncomplete data holder
	char bufrd; // buffer ready flag

	int state; // STATE_*
	int supports; // SUPPORTS_*

	int id;		//number on userlist
	char *nick;
	char *ip;
	char *con_ip; // ip of the interface to which user is connected
	char *ver;

	time_t idle;
	time_t last_search;
 
	unsigned char cons;// 0 - normal user
					// 1 - console user


	char *perm;
	int slots; //amount of slots :)
	int slots_old;

	time_t sent_search_interval;
	
	Tpenalty *penalty;			// penalty ///
	
	char *password;
	char *reason;	//reason of quiting

	char *key;

	unsigned long recv_bytes;	/* cumulative bytes read/sent for this user */
	unsigned long send_bytes;

	myinfo_t myinfo;

	char userip[7+1+NICK_LEN+1+15+1+1];
	
}userrec_t;

/* filled with config file data */
typedef struct
{
	char *conf_main;
	char *conf_welcome;
	char *conf_motd;
	char *conf_banned;
	char *conf_cusers;
	char *conf_callow;
	char *conf_nallow;
	char *conf_penalties;
	char *conf_rules;
	char *conf_hublinks;
	char *conf_usercommands;

	unsigned char log_level;

    int log_via_syslog;
    unsigned int log_facility;
    unsigned int log_priority;
	
	char *log_dir;
	char *log_main;
    char *log_chat;
	char *log_access;
	char *log_debug;
	char *log_user;
	char *log_protocol;
	char *log_daemon;
    char *log_network;
    
    
	char *listen_interface;
	char *admin_contact;
	unsigned short int listen_port_main;
	unsigned short int listen_port_cons;
	unsigned short int listen_port_udp;
	char *hubname;
	unsigned int userlimit;
	char allow_broken_key;
	char allow_chat;
	long std_penalty_duration;

	long minimal_sleep_time;
	long penalties_update_interval;

	long n_listen_main;
	long n_user_manager;

	unsigned int idle_timeout;
	unsigned int register_timeout;

	long search_interval;

	char *nick_pattern;

	uint64_t minshare;
	uint32_t minslots;

 	unsigned int allow_non_us_ascii_nicks;
 	unsigned int allow_passive;
	unsigned int allow_forwarding;
	unsigned int allow_downloads;
	unsigned int allow_search;

	char *user;
	char *group;

	uid_t uid;
	gid_t gid;

	unsigned char flags;

 	unsigned int max_commands;
 	unsigned int max_chat_length;
 	unsigned int kick_max_chat_length;

 	unsigned int max_receive_once;

 	unsigned int redirect_switch;
 	char *redirect_hub_is_full;
 	char *redirect_minshare;
 	char *redirect_minslots;

 	int sndbuf;
 	int rcvbuf;
	
	char *msg_welcome;
	char *msg_motd;
	char *msg_rules;
	
	char *msg_usercommands;

	char registered_only;
} config_t;

typedef struct{
 	int so_debug;
 	int so_broadcast;
 	int so_reuseaddr;
 	int so_oobinline;
 	int so_keepalive;
 	struct linger so_linger;
 	int so_sndbuf;
 	int so_rcvbuf;
 	int so_dontroute;
 	int so_rcvlowat;
 	int so_rcvtimeo;
 	int so_sndlowat;
 	int so_sndtimeo;
}so_opts_t;

typedef struct{
	char *nick;
	char *password;
	char *email;
	char *desc;
	char *perm;
} loginrec_t;


typedef struct{
	struct sockaddr_in udp;
	char *ip;
	char *port;
	char *pass_in;
	char *pass_out;
	int timeout;
	int timeout_retry;
	char *interface;

	char *Up_cache;
	char *UpToo_cache;

	int sock;
} hub_t;


int user_set_state(userrec_t *usr,int state);
int user_tst_state(userrec_t *usr, int state);

int user_set_supports(userrec_t *usr, int supports);
int user_tst_supports(userrec_t *usr, int supports);

void userlist_add(userrec_t *usr);
void userlist_del(userrec_t *usr);

void adduser( int sock, char *remote, char *local);
void deluser( userrec_t *usr );
void finish( int sig );

#endif /* __vXhz__MAIN_H__INCLUDED__ */

/* VIM Settings {{{
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * soft-stop-width: 4
 * c indent on
 * End:
 * vim600: sw=4 ts=4 sts=4 cindent fdm=marker
 * vim<600: sw=4 ts=4
 * }}}*/
