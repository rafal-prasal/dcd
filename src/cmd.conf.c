/* $Id: cmd.conf.c,v 1.28 2007/03/01 13:41:22 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *       #(@) Copyright (c) 2002, DConnect development team
 *       #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */  
 
#include "pch.h"

extern char *__progname;
extern config_t conf;

extern so_opts_t listen_opts, user_opts;

/*	parses config line similar to this {{{
	[#\ \t]*option[\ \t]*=+[\ \t]*value[\ \t\r\n]*
	
	'#'   returns 0
	fine  returns 1
	error returns -1
*/
	
int parse_config_line(char *line,char **cmd,char **val)
{
	char *p=line;
	int i=0;

	i=strlen(line)-1;

	while(i>0 && (p[i]=='\n' || p[i]=='\r' || isspace(p[i]))) p[i--]='\000';
	i=0;

	while(p[i] && isspace(p[i])) i++;

	if (!p[i] || p[i]=='#') return 0;

	*cmd=&p[i];

	while(p[i] && (isalpha(p[i]) || p[i]=='_' || p[i]=='-')) i++;

	if (!isspace(p[i]) && (p[i]!='=')) return -1;
	p[i++]='\000';

	while(p[i] && (isspace(p[i]) || p[i]=='=')) i++;
	
	if (!p[i] || p[i]=='\n' || p[i]=='\r') return -1;

	*val=&p[i];
	
	return 1;
} /* }}} */

void conf_admin_contact(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.admin_contact);
}

void conf_allow_broken_key(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if ((ival!=0) && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be in 0 or 1",cmd);
		exit(1);
	}

	conf.allow_broken_key=ival;
}

void conf_allow_chat(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
		exit(1);
	}

	conf.allow_chat=ival;
}

void conf_allow_downloads(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
		exit(1);
	}

	conf.allow_downloads=ival;
}


void conf_allow_forwarding(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
		exit(1);
	}

	conf.allow_forwarding=ival;
}

void conf_allow_search(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
	
		exit(1);
	}

	conf.allow_search=ival;
}

void conf_registered_only(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
		exit(1);
	}

	conf.registered_only=ival;
}
void conf_allow_non_us_ascii_nicks(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);

		exit(1);
	}

	conf.allow_non_us_ascii_nicks=ival;
}

void conf_allow_passive(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be 0 or 1",cmd);
		exit(1);
	}

	conf.allow_passive=ival;
}

void conf_bind_address(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.listen_interface);
}


void conf_conf_motd(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_motd);
}

void conf_conf_nallow(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_nallow);
}

void conf_conf_usercommands(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_usercommands);
}

void conf_conf_welcome(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_welcome);
}

void conf_conf_hublinks(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_hublinks);
}

void conf_conf_cusers(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_cusers);
}

void conf_conf_callow(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_callow);
}

void conf_conf_banned(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_banned);
}

void conf_conf_penalties(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_penalties);
}

void conf_conf_rules(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.conf_rules);
}

void conf_group(conf_param_t *param)
{
	char *value=param->value;
	struct group *group_data;
	int start = param->start;

	if (!start) return;
	
	group_data=getgrnam(value);

	if(!group_data)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"Group '%s' not found.",value);
		exit(1);
	}

	conf.gid=group_data->gr_gid;
	my_duplicate(value,&conf.group);						
}

void conf_hubname(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.hubname);
}

void conf_listen_port_main(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;
	int start = param->start;

	if (!start) return;

	ival=strtol(value, NULL, 10);

	if (ival<=0 || ival>65535)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be in <0,65535>",cmd);
		exit(1);
	}

	conf.listen_port_main=ival;
	conf.listen_port_udp=ival;
}

void conf_listen_port_cons(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;
	int start = param->start;

	if (!start) return;

	ival=strtol(value, NULL, 10);

	if (ival<0 || ival>65535)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be in <0,65535>",cmd);
		exit(1);
	}

	conf.listen_port_cons=ival;
}

void conf_log_facility(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<0 || ival>65535)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be in <0,65535>",cmd);
		exit(1);
	}

	conf.log_facility=ival;
}

void conf_idle_timeout(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<0)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' >=0",cmd);
		exit(1);
	}

	conf.idle_timeout=ival;
}

void conf_max_chat_length(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<0 || ival>=MAX_USER_BUFFER)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be >=0 and <%d",cmd,MAX_USER_BUFFER);
		exit(1);
	}

	conf.max_chat_length=ival;
}

void conf_max_receive_once(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;

	ival=strtol(value, NULL, 10);

	if(ival<=0 || ival>MAX_RECEIVE_ONCE) ival = conf.max_receive_once=MAX_RECEIVE_ONCE;

	conf.max_receive_once=ival;
}


void conf_max_chat_length_kick(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival!=0 && ival!=1)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be in 0 or 1",cmd);
		exit(1);
	}

	conf.kick_max_chat_length=ival;
}

void conf_max_commands(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<0)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be >=0",cmd);
		exit(1);
	}

	conf.max_commands=ival;
}

void conf_minimal_sleep_time(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);
	if (ival<0)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be >=0",cmd);

		exit(1);
	}

	conf.minimal_sleep_time=ival;
}

void conf_min_share(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;

	ival=strtol(value, NULL, 10);

	if (!ival || !sscanf(value,"%"SCNu64,&(conf.minshare))) conf.minshare=0;
}

void conf_min_slots(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	
	ival=strtol(value, NULL, 10);

	if (ival<0) ival=0;

	conf.minslots=ival;
}

void conf_n_listen_main(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;

	int start = param->start;

	if(!start) return;
	
	ival=strtol(value, NULL, 10);
	if (ival<1 || ival>MAX_N_LISTEN_MAIN) ival=N_LISTEN_MAIN;

	conf.n_listen_main=ival;	
}

void conf_n_user_manager(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	int start = param->start;

	if(!start) return;
	
	ival=strtol(value, NULL, 10);
	
	if (ival<1 || ival>MAX_N_USER_MANAGER) ival=N_USER_MANAGER;

	conf.n_user_manager=ival;	
}

void conf_redirect_hub_is_full(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.redirect_hub_is_full);
}

void conf_redirect_min_share(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.redirect_minshare);
}

void conf_redirect_min_slots(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.redirect_minslots);
}

void conf_redirect_switch(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;
	
	ival=strtol(value, NULL, 10);

	if (ival<0 || ival>65535)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be in <0,65535>",cmd);
		exit(1);
	}

	conf.redirect_switch=ival;	
}


void conf_register_timeout(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<0)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be >=0",cmd);
		exit(1);
	}

	conf.register_timeout=ival;
}

void conf_penalties_update_interval(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	
	ival=strtol(value, NULL, 10);
	if (ival<10) ival=10;
	
	conf.penalties_update_interval=ival;
}

void conf_search_interval(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	
	ival=strtol(value, NULL, 10);
	if (ival<0) ival=2;
	
	conf.search_interval=ival;
}

void conf_std_penalty_duration(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	
	ival=strtol(value, NULL, 10);
	if (ival<0) ival=10;
	
	conf.std_penalty_duration=ival;
}

void conf_socket_send_buffer(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	int start = param->start;

	if(!start) return;

	ival=strtol(value, NULL, 10);
	if (ival<0) ival=VAL_SO_SNDBUF;

	listen_opts.so_sndbuf=ival;
	user_opts.so_sndbuf=ival;
	
	conf.std_penalty_duration=ival;
}

void conf_socket_receive_buffer(conf_param_t *param)
{
	long ival=0;

	char *value=param->value;
	int start = param->start;
	if(!start) return;
	
	ival=strtol(value, NULL, 10);
	if (ival<0) ival=VAL_SO_RCVBUF;

	listen_opts.so_rcvbuf=ival;
	user_opts.so_rcvbuf=ival;
	
	conf.std_penalty_duration=ival;
}

void conf_user(conf_param_t *param)
{
	char *value=param->value;
	struct passwd *user_data;
	int start = param->start;

	if (!start) return;
	
	user_data=getpwnam(value);

	if(!user_data)
	{
        log_write(FAC_DAEMON,PR_CRITICAL, "User '%s' not found.",value);
		exit(1);
	}

	conf.uid=user_data->pw_uid;
	my_duplicate(value,&conf.user);						
}

void conf_user_limit(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;

	ival=strtol(value, NULL, 10);

	if (ival<=0 || ival>MAXUSERS)
	{
    
        log_write(FAC_DAEMON,PR_CRITICAL, "'%s' should be in (0,%d>",cmd,MAXUSERS);
		exit(1);
	}

	conf.userlimit=ival;
}

void conf_unknown(conf_param_t *param)
{
	char *line=param->line;

    log_write(FAC_DAEMON,PR_CRITICAL,"Unknown option: '%s'\n",line);
	exit(1);
}

void conf_deprecated(conf_param_t *param)
{
	char *line=param->line;

    log_write(FAC_DAEMON,PR_CRITICAL,"Deprecated option: '%s'\n",line);
	exit(1);
}

void conf_obsolete(conf_param_t *param)
{
	char *line=param->line;

    log_write(FAC_DAEMON,PR_CRITICAL,"Obsolete option: '%s'\n",line);
}

/* conf_cmd_exec() - parse config line {{{ */
void conf_cmd_exec(int start, char *line )
{
	void conf_listen_port_main_ssl(conf_param_t *param);
	void conf_ssl_cert_file(conf_param_t *param);

	cmd_t conf_set[] = 
	{
		{"admin_contact",				(funct_t)conf_admin_contact			},
		{"allow_broken_key",			(funct_t)conf_allow_broken_key		},
		{"allow_chat",					(funct_t)conf_allow_chat			},
		{"allow_downloads", 			(funct_t)conf_allow_downloads		},
		{"allow_forwarding",			(funct_t)conf_allow_forwarding		},
		{"allow_non_us_ascii_nicks",	(funct_t)conf_allow_non_us_ascii_nicks	},
		{"allow_passive",				(funct_t)conf_allow_passive			},
		{"allow_search", 				(funct_t)conf_allow_search			},
		{"bind_address",				(funct_t)conf_bind_address			},
		{"conf_banned",					(funct_t)conf_conf_banned			},
		{"conf_callow",					(funct_t)conf_conf_callow			},
		{"conf_cusers",					(funct_t)conf_conf_cusers			},
		{"conf_hublinks",				(funct_t)conf_conf_hublinks			},
		{"conf_motd",					(funct_t)conf_conf_motd				},
		{"conf_nallow",					(funct_t)conf_conf_nallow			},
		{"conf_penalties",				(funct_t)conf_conf_penalties		},
		{"conf_rules",					(funct_t)conf_conf_rules			},
		{"conf_usercommands",			(funct_t)conf_conf_usercommands		},
		{"conf_welcome",				(funct_t)conf_conf_welcome			},
		{"debug_level",					(funct_t)conf_obsolete				},
		{"group",						(funct_t)conf_group					},
		{"hubname",						(funct_t)conf_hubname 				},
		{"idle_timeout",				(funct_t)conf_idle_timeout			},
		{"kick_max_chat_length",		(funct_t)conf_max_chat_length_kick	},		
		{"listen_interface",			(funct_t)conf_obsolete				},
		{"listen_port_cons",			(funct_t)conf_listen_port_cons 		},
		{"listen_port_main",			(funct_t)conf_listen_port_main 		},
		{"log_chat",					(funct_t)conf_obsolete				},
		{"log_con",						(funct_t)conf_obsolete				},
		{"log_dir",						(funct_t)conf_obsolete				},
        {"log_facility",                (funct_t)conf_log_facility          },
		{"log_level",					(funct_t)conf_obsolete				},
		{"log_main",					(funct_t)conf_obsolete				},
		{"log_net",						(funct_t)conf_obsolete				},
		{"log_proto",					(funct_t)conf_obsolete				},
		{"log_wrng",					(funct_t)conf_obsolete				},
		{"max_chat_length",				(funct_t)conf_max_chat_length		},
		{"max_chat_length_kick",		(funct_t)conf_max_chat_length_kick	},
		{"max_commands", 				(funct_t)conf_max_commands			},
		{"max_receive_once", 			(funct_t)conf_max_receive_once		},
		{"min_share",					(funct_t)conf_min_share				},
		{"min_slots",					(funct_t)conf_min_slots				},
		{"minimal_sleep_time",			(funct_t)conf_minimal_sleep_time	},
		{"n_listen_main", 				(funct_t)conf_n_listen_main			},
		{"n_user_manager", 				(funct_t)conf_n_user_manager		},
		{"penalties_update_interval", 	(funct_t)conf_penalties_update_interval	},
		{"ping_timeout",				(funct_t)conf_deprecated			},
		{"redirect_access",				(funct_t)conf_obsolete      		},
		{"redirect_hub_is_full",		(funct_t)conf_redirect_hub_is_full	},
		{"redirect_min_share",			(funct_t)conf_redirect_min_share	},
		{"redirect_min_slots",			(funct_t)conf_redirect_min_slots	},
		{"redirect_switch",				(funct_t)conf_redirect_switch		},
		{"register_timeout",			(funct_t)conf_register_timeout		},
		{"registered_only",				(funct_t)conf_registered_only		},
		{"search_interval",				(funct_t)conf_search_interval		},
		{"send_retries",				(funct_t)conf_obsolete				},
		{"send_retries_time",			(funct_t)conf_obsolete				},
		{"socket_receive_buffer",		(funct_t)conf_socket_receive_buffer	},
		{"socket_send_buffer",			(funct_t)conf_socket_send_buffer	},
		{"std_penalty_duration",		(funct_t)conf_std_penalty_duration	},
		{"stealth_mode",			    (funct_t)conf_obsolete		        },
		{"user",						(funct_t)conf_user					},
		{"user_limit",					(funct_t)conf_user_limit			},
		{"listen_port_main_ssl",			(funct_t)conf_listen_port_main_ssl 	},
		{"ssl_cert_file",			(funct_t)conf_ssl_cert_file		},
		{NULL,							(funct_t)conf_unknown				}
	};

	int n_conf_set = sizeof(conf_set)/sizeof(cmd_t);

	char *cmd=NULL,
		 *value=NULL,
		 *line_copy=NULL;

	conf_param_t param;
	int ret=0;

		
	line_copy=strdup(line);
					 
	ret=parse_config_line(line_copy,&cmd,&value);
	
	switch(ret)
	{
		case -1: 
                log_write(FAC_DAEMON,PR_ERROR,"Parsing failed for '%s'",line);	
				break;
		
		case 1:
				param.line=line;
				param.cmd=cmd;
				param.value=value;
				param.start=start;
				bin_cmd_exec(conf_set, n_conf_set, cmd, &param);	
		break;
	}
		
	
	my_free(line_copy);
} /* }}} */

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

void conf_listen_port_main_ssl(conf_param_t *param)
{
	long ival=0;

	char *cmd=param->cmd,
		 *value=param->value;
	int start = param->start;

	if (!start) return;

	ival=strtol(value, NULL, 10);

	if (ival<0 || ival>65535)
	{
        log_write(FAC_DAEMON,PR_CRITICAL,"'%s' should be in <0,65535>",cmd);
		exit(1);
	}

	conf.listen_port_main_ssl=ival;
}

void conf_ssl_cert_file(conf_param_t *param)
{
	char *value=param->value;

	my_duplicate(value,&conf.ssl_cert_file);
}
