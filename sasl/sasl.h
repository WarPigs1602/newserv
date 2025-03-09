/* 
 * chanserv.h:
 *  Top level data structures and function prototypes for the
 *  channel service module
 */

#ifndef __SASL_H
#define __SASL_H

#define _GNU_SOURCE
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <stdint.h>

#include "../dbapi/dbapi.h"
#include "../chanserv/chanserv.h"
#include "../chanserv/authlib.h"
#include "../lib/sstring.h"
#include "../core/schedule.h"
#include "../lib/flags.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../parser/parser.h"
#include "../localuser/localuserchannel.h"
#endif