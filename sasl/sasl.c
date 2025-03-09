#include "sasl.h"
#include "../lib/irc_string.h"
#include "../control/control.h"
#include "../nick/nick.h"
#include "../irc/irc.h"
#include "../localuser/localuser.h"
#include "../lib/flags.h"
#include "../lib/version.h"
#include "../server/server.h"
#include <stdio.h>
#include <string.h>

int handlesaslmsg(void *source, int cargc, char **cargv) {
  reguser *rup;
  nick *sender=source;
  char *authtype = "SASL";
  
  
  if (!(rup=findreguserbynick(cargv[1]))) {
	printf("%s AUTHENTICATE %s FAIL\n",mynumeric->content,longtonumeric(sender->numeric,2)); 
    irc_send("%s AUTHENTICATE %s FAIL",mynumeric->content,longtonumeric(sender->numeric,2)); 
    return CMD_ERROR;
  }

  if (!checkpassword(rup, cargv[2])) {
    irc_send("%s AUTHENTICATE %s FAIL",mynumeric->content,longtonumeric(sender->numeric,2));   
    printf("%s AUTHENTICATE %s FAIL\n",mynumeric->content,longtonumeric(sender->numeric,2)); 
    return CMD_ERROR;
  }

  irc_send("%s AUTHENTICATE %s SUCCESS %s %s",mynumeric->content,longtonumeric(sender->numeric,5),cargv[0],cargv[1]);
  irc_send("%s AC %s %s %ld %d", mynumeric->content,cargv[0], rup->username, rup->lastauth, rup->ID);

  return CMD_OK;
}

void _init() {
    registerserverhandler("SASL",&handlesaslmsg,5);
}

void _fini() {
  deregisterserverhandler("SASL",&handlesaslmsg);
}