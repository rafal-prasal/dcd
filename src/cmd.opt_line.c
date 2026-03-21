/* $Id: cmd.opt_line.c,v 1.7 2006/04/04 10:05:56 rav Exp $ */

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

/* opt_help() - just show a brief usage {{{ */
void opt_help(opt_param_t *param)
{
	printf( "DConnect Daemon v%s\r\n\r\n",PACKAGE_VERSION );
	printf( "Usage:\t%s [options]\r\n\r\n", __progname );
	printf( "-h\t\t this help screen\r\n" );
	printf( "-v\t\t dump version and exit\r\n" );
	printf( "-c config_file\t use alternate config file\r\n" );
	printf( "\r\nReport bugs to <dc-hub@ds.pg.gda.pl>.\r\n" );
	exit( 1 );
} /* }}} */

/* opt_version() - show version {{{ */
void opt_version(opt_param_t *param)
{
	printf("%s\r\n",PACKAGE_VERSION);
	exit(1);
} /* }}} */

/* opt_config() - change default config file path {{{ */
void opt_config(opt_param_t *param)
{
	if (!param->value || !param->value[0]) opt_help(NULL);
	
	my_duplicate(param->value,&conf.conf_main);

	param->value[0]='\000'; //setting to \000 because next option was parameter to this option
} /* }}} */

/* opt_unknown() - change default config file path {{{ */
void opt_unknown(opt_param_t *param)
{
	fprintf(stderr,"illegal option '%s'\r\n\r\n",param->opt);
	opt_help(NULL);
}

/* opt_line() - parse command-line options {{{ */
void opt_line(int argc, char *argv[] )
{
	int i;
	opt_param_t param;

	cmd_t opt_set[] = {
		{"--config", 	(funct_t)opt_config		},
		{"--help", 		(funct_t)opt_help		},
		{"--version", 	(funct_t)opt_version	},
		{"-c", 			(funct_t)opt_config		},
		{"-h",			(funct_t)opt_help		},
		{"-v", 			(funct_t)opt_version	},
	 	{NULL,			(funct_t)opt_unknown	}
	};
	
	int n_opt_set=sizeof(opt_set)/sizeof(cmd_t);
					 
	for ( i = 1; i < argc; i++ )
	{
		//if this was parameter for previous option
		
		if (!argv[i][0]) continue;
		
		param.opt=argv[i];
		
		param.value=NULL;
		if (i+1<argc) param.value=argv[i+1];
		
		bin_cmd_exec(opt_set,n_opt_set,argv[i],&param);
	}
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

