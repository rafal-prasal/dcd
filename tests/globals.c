/* Globals normally owned by src/main.c, provided so that test binaries
   can link against the daemon sources without main(). */

#include "pch.h"

int NUSERS = 0;
int NHUBS = 0;
int NLOGINS = 0;
int NPATTERNS = 0;
int NPENAL = 0;
int maxfd = 0;
volatile sig_atomic_t rehash = 0;

int listen_main = -1;
int listen_udp = -1;
int listen_cons = -1;

userrec_t *user[MAXUSERS];
hub_t *hub[MAXHUBS];
loginrec_t *login[MAXUSERS];
char *pattern[MAXUSERS];
Tpenalty *penalties[MAXPENALTIES];

config_t conf;
so_opts_t listen_opts, user_opts;

time_t starttime = 0;
time_t last_penalties_update = 0;

pthread_mutex_t mutex_users;
pthread_mutex_t mutex_inet_ntoa;
pthread_mutex_t mutex_penalties;
pthread_mutex_t mutex_myinfo;

FILE* file_net = NULL;
FILE* file_proto = NULL;
FILE* file_wrng = NULL;
FILE* file_con = NULL;
FILE* file_chat = NULL;

const char *reserved_nicks[] =
{
	"Hub",
	"Hub-Security",
	"Vandel\\Debug",
	"Client",
	"" /* indicates end of list */
};
