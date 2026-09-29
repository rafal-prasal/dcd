/* $Id: log.c,v 1.2 2007/04/05 21:46:49 rav Exp $ */

/* Intro {{{
 * ----------------------------------------------------------------
 * DConnect Daemon
 *
 *	 #(@) Copyright (c) 2002, DConnect development team
 *	 #(@) Homepage: http://www.dc.ds.pg.gda.pl/
 *
 * ----------------------------------------------------------------
 * }}} */

#include "pch.h"

extern config_t conf;
extern char *__progname;

int log_vfacility[LOG_LEVELS] = { LOG_AUTHPRIV, LOG_USER, LOG_LOCAL0, LOG_LOCAL1, LOG_DAEMON, LOG_LOCAL2, LOG_LOCAL3 };

int log_vpriority[9] = { LOG_DEBUG, LOG_INFO, LOG_NOTICE, LOG_WARNING, LOG_CRIT, LOG_ALERT,LOG_ERR, LOG_EMERG };
char *log_spriority[9] ={ "DEBUG", "INFO", "NOTICE", "WARNING", "CRIT", "ALERT",    "ERROR",  "EMERGENCY" };

FILE *log_files[LOG_LEVELS];

void log_open()
{
    int log_FACILITY=0
        ,i;

    log_close();
    
    if(conf.log_via_syslog) 
    {
        if (conf.log_facility&LEV_ACCESS)      log_FACILITY|=log_vfacility[FAC_ACCESS];
        if (conf.log_facility&LEV_DEBUG)       log_FACILITY|=log_vfacility[FAC_DEBUG];
        if (conf.log_facility&LEV_PROTOCOL)    log_FACILITY|=log_vfacility[FAC_PROTOCOL];
        if (conf.log_facility&LEV_DAEMON)      log_FACILITY|=log_vfacility[FAC_DAEMON];
        if (conf.log_facility&LEV_USER)        log_FACILITY|=log_vfacility[FAC_USER];
        if (conf.log_facility&LEV_CHAT)        log_FACILITY|=log_vfacility[FAC_CHAT];
        if (conf.log_facility&LEV_NETWORK)     log_FACILITY|=log_vfacility[FAC_NETWORK];        

        openlog(__progname,LOG_NDELAY| LOG_PID, log_FACILITY);   
        
        return;
    }

    for(i=0; i<LOG_LEVELS; i++) log_files[i]=NULL;
        
    if (conf.log_facility&LEV_ACCESS)   log_files[FAC_ACCESS]=fopen(conf.log_access,"a+");
    if (conf.log_facility&LEV_DEBUG)    log_files[FAC_DEBUG]=fopen(conf.log_debug,"a+");
    if (conf.log_facility&LEV_USER)     log_files[FAC_USER]=fopen(conf.log_user,"a+");
    if (conf.log_facility&LEV_PROTOCOL) log_files[FAC_PROTOCOL]=fopen(conf.log_protocol,"a+");
    if (conf.log_facility&LEV_DAEMON)   log_files[FAC_DAEMON]=fopen(conf.log_daemon,"a+");
    if (conf.log_facility&LEV_CHAT)     log_files[FAC_CHAT]=fopen(conf.log_chat,"a+");
    if (conf.log_facility&LEV_NETWORK)  log_files[FAC_NETWORK]=fopen(conf.log_network,"a+");

}

void log_print(int facility, int priority, char *msg)
{
	fprintf(log_files[facility],"[%s]: %s\r\n", log_spriority[priority], msg);
}


void log_write(int facility, int priority, const char * fmt, ...)
{
    va_list args;
    char *msg;


    
    if((unsigned int)priority>conf.log_priority) return;

    va_start(args, fmt);
            
    if(conf.log_via_syslog) vsyslog( log_vfacility[facility]|log_vpriority[priority] , fmt, args);
    else
        if(log_files[facility]!=NULL)
        {        
            msg=(char *)my_vsprintf(fmt,args);
            log_print(facility, priority, msg);            
            my_free(msg);
        }
    
    va_end(args);
}

void log_close()
{
    int i;

    if(conf.log_via_syslog) closelog();
    else
        for(i=0; i<LOG_LEVELS; i++)
            if(log_files[i]!=NULL)
            {
                fflush(log_files[i]);
                fclose(log_files[i]);
            }
}

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
