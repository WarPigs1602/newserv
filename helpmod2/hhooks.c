#include <string.h>
#include <assert.h>

#include "helpmod.h"
#include "hchannel.h"
#include "haccount.h"
#include "huser.h"
#include "hban.h"
#include "hqueue.h"
#include "hgen.h"
#include "hstat.h"
/*
void helpmod_hook_quit(int unused, void *args)
{
    nick *nck = ((nick**)args)[0];
    huser *husr = huser_get(nck);
    / * it was someone we didn't even know * /
    if (husr == NULL)
        return;
}
*/

/*
void helpmod_hook_part(int unused, void *args)
{
    channel *chan = ((channel**)args)[0];
    hchannel *hchan = hchannel_get_by_channel(chan);
    nick *nck = ((nick**)args)[1];
    huser *husr;

}
*/
static void helpmod_hook_join(int unused, void *args)
{
    channel *chan = ((channel**)args)[0];
    hchannel *hchan = hchannel_get_by_channel(chan);
    nick *nck = ((nick**)args)[1];
    huser *husr;

    /* if we're not on this channel or the target is G, the event is of no interest */
    if (hchan == NULL || nck == helpmodnick)
        return;

    husr = huser_get(nck);

    assert(husr != NULL); /* hook_channel_newnick should fix this */

    if ((hchan->flags & H_JOINFLOOD_PROTECTION) && !(hchan->flags & H_PASSIVE))
    {
	if (hchan->jf_control < time(NULL))
	    hchan->jf_control = time(NULL);
	else
	    hchan->jf_control++;

	if (hchan->jf_control - time(NULL) > 12 && !IsRegOnly(hchan))
	{
	    if (hchan->flags & H_REPORT && hchannel_is_valid(hchan->report_to))
		helpmod_message_channel(hchan->report_to, "Warning: Possible join flood on %s, setting +r", hchannel_get_name(hchan));
	    hchannel_activate_join_flood(hchan);
	}
    }

    if (hchan->flags & H_PASSIVE)
        return;

    if (hchan->flags & H_DO_STATS)
        hstat_add_join(hchan);

    if (hchan->flags & H_TROJAN_REMOVAL && huser_is_trojan(husr))
    {
        hban_huser(husr, "Trojan client", time(NULL) + 4 * HDEF_d, 1);
        return;
    }

    if (huser_get_level(husr) >= H_STAFF && (huser_get_account_flags(husr) & H_AUTO_OP) && hchannel_authority(hchan, husr))
        helpmod_channick_modes(husr, hchan ,MC_OP,HNOW);

    if (huser_get_level(husr) >= H_TRIAL && (huser_get_account_flags(husr) & H_AUTO_VOICE) && hchannel_authority(hchan, husr))
        helpmod_channick_modes(husr, hchan, MC_VOICE,HNOW);

    if (hchan->flags & H_WELCOME && *hchan->real_channel->index->name->content)
        helpmod_reply(husr, chan, "[%s] %s", hchan->real_channel->index->name->content, hchan->welcome);


    if (hchan->flags & H_QUEUE && hchan->flags & H_QUEUE_SPAMMY)
    {
        if (huser_get_level(husr) < H_STAFF)
            helpmod_reply(husr, NULL, "Channel %s is using a queue system. This means you will have to wait for support. Your queue position is #%d and currently the average time in queue is %s. Please wait for your turn and do not attempt to contact channel operators directly", hchannel_get_name(hchan), hqueue_get_position(hchan, husr), helpmod_strtime(hqueue_average_time(hchan)));
    }
    if (IsModerated(hchan->real_channel) && !(hchan->flags & H_QUEUE) && (hchan->flags & H_ANTI_IDLE))
    {   /* being nice to users */
        if (huser_get_level(husr) < H_STAFF)
            helpmod_reply(husr, NULL, "Channel %s is currently moderated and there are anti-idle measures in place. This usually means that there is no one to handle your problem at this time. Please try again later and don't forget to read the FAQ at www.quakenet.org", hchannel_get_name(hchan));
    }
}

static void helpmod_hook_channel_newnick(int unused, void *args)
{
    channel *chan = ((channel**)args)[0];
    hchannel *hchan = hchannel_get_by_channel(chan);
    nick *nck = ((nick**)args)[1];
    huser *husr;

    /* if we're not on this channel or the target is G, the event is of no interest */
    if (hchan == NULL || nck == helpmodnick)
        return;

    if ((husr = huser_get(nck)) == NULL)
        husr = huser_add(nck);

    assert(huser_on_channel(husr, hchan) == NULL);
    assert(hchannel_on_channel(hchan, husr) == NULL);

    hchannel_add_user(hchan, husr);
    huser_add_channel(husr, hchan);

    if (hchan->flags & H_PASSIVE)
        return;

    huser_activity(husr, NULL);

    if (huser_get_level(husr) == H_LAMER || (huser_get_level(husr) <= H_TRIAL && hban_check(nck)))
    {
        hban *hb = hban_check(nck);

	const char *banmask = hban_ban_string(nck, HBAN_HOST);
	helpmod_setban(hchan, banmask, time(NULL) + 1 * HDEF_d, MCB_ADD, HNOW);

	if (hb)
	    helpmod_kick(hchan, husr, "%s", hban_get_reason(hb));
	else
	    helpmod_kick(hchan, husr, "Your presence on channel %s is not wanted", hchannel_get_name(hchan));

	return;
    }
    if (hchan->flags & H_QUEUE)
        hqueue_handle_queue(hchan, NULL);
}

static void helpmod_hook_channel_lostnick(int unused, void *args)
{
    channel *chan = ((channel**)args)[0];
    hchannel *hchan = hchannel_get_by_channel(chan);
    huser_channel *huserchan;
    nick *nck = ((nick**)args)[1];
    huser *husr;

    /* In event that G was kicked, nothing is done */
    if (nck == helpmodnick)
        return;

    /* hackery, can't think of a better way to do this */
    huser *oper = NULL;
    int handle_queue = 0;
        
    /* if we're not on this channel, the event is of no interest */
    if (hchan == NULL)
        return;

    husr = huser_get(nck);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(hchannel_on_channel(hchan, husr) != NULL);
    assert(huserchan != NULL);

    if ((hchan->flags & H_QUEUE) && (hchan->flags & H_QUEUE_MAINTAIN) && !(hchan->flags & H_PASSIVE)) /* && (huser_get_level(husr) == H_PEON) && (huserchan->flags & HCUMODE_VOICE) && (hchannel_count_queue(hchan)))*/
	if (serverlist[homeserver(husr->real_user->numeric)].linkstate != LS_SQUIT)
            /* if it was a netsplit, we do not trigger autoqueue */
        {
            oper = huserchan->responsible_oper;
            handle_queue = !0;
        }

    hchannel_del_user(hchan, husr);
    huser_del_channel(husr, hchan);

    if (nck == helpmodnick)
    { /* if H left the channel, we remove all the users from the channel */
        while (hchan->channel_users)
        {
            huser_del_channel(hchan->channel_users->husr, hchan);
            hchannel_del_user(hchan, hchan->channel_users->husr);
        }
    }

    else if (handle_queue)
        hqueue_handle_queue(hchan, oper);
}

static void helpmod_hook_nick_lostnick(int unused, void *args)
{
    nick *nck = (nick*)args;
    huser *husr = huser_get(nck);

    /* it was someone we didn't even know */
    if (husr == NULL)
	return;

    huser_del(husr);
}

static void helpmod_hook_channel_owner(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr, *husr2;

    if (hchan == NULL)
        return;

    husr  = huser_get(((nick**)args)[2]);
    husr2 = huser_get(((nick**)args)[1]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags |= HCUMODE_OWNER;

    if (hchan->flags & H_PASSIVE)
        return;

    /* if the +o was given by a network service, G will not interfere */
    if (husr2 == NULL || strlen(huser_get_nick(husr2)) == 1)
        return;

    if (huser_get_level(husr) < H_STAFF)
        helpmod_channick_modes(husr, hchan, MC_DEOWNER ,HNOW);
}

static void helpmod_hook_channel_deowner(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr;

    if (hchan == NULL)
        return;

    husr = huser_get(((nick**)args)[2]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags &= ~HCUMODE_OWNER;
}

static void helpmod_hook_channel_admin(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr, *husr2;

    if (hchan == NULL)
        return;

    husr  = huser_get(((nick**)args)[2]);
    husr2 = huser_get(((nick**)args)[1]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags |= HCUMODE_ADMIN;

    if (hchan->flags & H_PASSIVE)
        return;

    /* if the +o was given by a network service, G will not interfere */
    if (husr2 == NULL || strlen(huser_get_nick(husr2)) == 1)
        return;

    if (huser_get_level(husr) < H_STAFF)
        helpmod_channick_modes(husr, hchan, MC_DEADMIN ,HNOW);
}

static void helpmod_hook_channel_deadmin(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr;

    if (hchan == NULL)
        return;

    husr = huser_get(((nick**)args)[2]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags &= ~HCUMODE_ADMIN;
}

static void helpmod_hook_channel_opped(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr, *husr2;

    if (hchan == NULL)
        return;

    husr  = huser_get(((nick**)args)[2]);
    husr2 = huser_get(((nick**)args)[1]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags |= HCUMODE_OP;

    if (hchan->flags & H_PASSIVE)
        return;

    /* if the +o was given by a network service, G will not interfere */
    if (husr2 == NULL || strlen(huser_get_nick(husr2)) == 1)
        return;

    if (huser_get_level(husr) < H_STAFF)
        helpmod_channick_modes(husr, hchan, MC_DEOP ,HNOW);
}

static void helpmod_hook_channel_deopped(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr;

    if (hchan == NULL)
        return;

    husr = huser_get(((nick**)args)[2]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags &= ~HCUMODE_OP;
}

static void helpmod_hook_channel_voiced(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr;

    if (hchan == NULL)
        return;
    husr = huser_get(((nick**)args)[2]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags |= HCUMODE_VOICE;

    if (hchan->flags & H_QUEUE)
    {
        huserchan->flags |= HQUEUE_DONE;
        huserchan->last_activity = time(NULL);
    }
}

static void helpmod_hook_channel_devoiced(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser_channel *huserchan;
    huser *husr;

    if (hchan == NULL)
        return;

    husr  = huser_get(((nick**)args)[2]);

    assert(husr != NULL);

    huserchan = huser_on_channel(husr, hchan);

    assert(huserchan != NULL);

    huserchan->flags &= ~HCUMODE_VOICE;

    if (hchan->flags & H_PASSIVE)
        return;

    if ((hchan->flags & H_QUEUE) && (hchan->flags & H_QUEUE_MAINTAIN) && (huser_get_level(husr) == H_PEON) && (huserchan->flags & HCUMODE_VOICE) && (hchannel_count_queue(hchan)))
    {
        if (serverlist[homeserver(husr->real_user->numeric)].linkstate != LS_SQUIT)
        { /* if it was a netsplit, we do not trigger autoqueue */
            hqueue_handle_queue(hchan, huserchan->responsible_oper);
        }
    }
}

static void helpmod_hook_channel_topic(int unused, void *args)
{
    hchannel *hchan = hchannel_get_by_channel(((channel**)args)[0]);
    huser *husr;

    if (hchan == NULL || hchan->flags & H_PASSIVE)
        return;

    husr = huser_get(((nick**)args)[2]);

    if (hchan->flags & H_HANDLE_TOPIC)
    {
        if (husr != NULL && huser_get_level(husr) >= H_OPER)
        {
            htopic_del_all(&hchan->topic);
            htopic_add(&hchan->topic, hchan->real_channel->topic->content, 0);
        }
        else
        {
            hchannel_set_topic(hchan);
        }
    }
}

static void helpmod_hook_nick_account(int unused, void *args)
{
    nick *nck = (nick*)args;
    huser *husr = huser_get(nck);
    huser_channel *huserchan, *huserchannext;
    if (husr == NULL)
        return;
    else
	husr->account = haccount_get_by_name(nck->authname);

    if (huser_get_level(husr) == H_LAMER) {
        for (huserchan = husr->hchannels; huserchan; huserchan = huserchannext) {
            huserchannext = huserchan->next;
            helpmod_kick(huserchan->hchan, husr, "Your presence on channel %s is not wanted", hchannel_get_name(huserchan->hchan));
        }
    }
}

static void helpmod_hook_server_newserver(int unused, void *args)
{
    hchannel *hchan;
    long numeric = (long)args;
    server srv = serverlist[numeric];

    /* check linkstate to prevent spam */
    if (srv.linkstate == LS_LINKING)
    {
	for (hchan = hchannels;hchan != NULL;hchan = hchan->next)
	    if ((hchan->flags & H_HANDLE_TOPIC) && (hchan->topic != NULL))
		hchannel_set_topic(hchan);
    }
}

void helpmod_registerhooks(void)
{
/*    registerhook(HOOK_NICK_QUIT, &helpmod_hook_quit); */
    /*if (registerhook(HOOK_CHANNEL_PART, &helpmod_hook_part));*/
    registerhook(HOOK_CHANNEL_JOIN, &helpmod_hook_join);
    registerhook(HOOK_NICK_LOSTNICK, &helpmod_hook_nick_lostnick);
    registerhook(HOOK_CHANNEL_NEWNICK, &helpmod_hook_channel_newnick);
    registerhook(HOOK_CHANNEL_LOSTNICK, &helpmod_hook_channel_lostnick);
    registerhook(HOOK_CHANNEL_OPPED, &helpmod_hook_channel_opped);
    registerhook(HOOK_CHANNEL_DEOPPED, &helpmod_hook_channel_deopped);
    registerhook(HOOK_CHANNEL_VOICED, &helpmod_hook_channel_voiced);
    registerhook(HOOK_CHANNEL_DEVOICED, &helpmod_hook_channel_devoiced);
    registerhook(HOOK_CHANNEL_ADMIN, &helpmod_hook_channel_admin);
    registerhook(HOOK_CHANNEL_DEADMIN, &helpmod_hook_channel_deadmin);
    registerhook(HOOK_CHANNEL_OWNER, &helpmod_hook_channel_owner);
    registerhook(HOOK_CHANNEL_DEOWNER, &helpmod_hook_channel_deowner);
    registerhook(HOOK_CHANNEL_TOPIC, &helpmod_hook_channel_topic);
    registerhook(HOOK_NICK_ACCOUNT, &helpmod_hook_nick_account);
    registerhook(HOOK_SERVER_NEWSERVER, &helpmod_hook_server_newserver);
}

void helpmod_deregisterhooks(void)
{
/*    deregisterhook(HOOK_NICK_QUIT, &helpmod_hook_quit); */
    /*if (deregisterhook(HOOK_CHANNEL_PART, &helpmod_hook_part));*/
    deregisterhook(HOOK_CHANNEL_JOIN, &helpmod_hook_join);
    deregisterhook(HOOK_NICK_LOSTNICK, &helpmod_hook_nick_lostnick);
    deregisterhook(HOOK_CHANNEL_NEWNICK, &helpmod_hook_channel_newnick);
    deregisterhook(HOOK_CHANNEL_LOSTNICK, &helpmod_hook_channel_lostnick);
    deregisterhook(HOOK_CHANNEL_OPPED, &helpmod_hook_channel_opped);
    deregisterhook(HOOK_CHANNEL_DEOPPED, &helpmod_hook_channel_deopped);
    deregisterhook(HOOK_CHANNEL_VOICED, &helpmod_hook_channel_voiced);
    deregisterhook(HOOK_CHANNEL_DEVOICED, &helpmod_hook_channel_devoiced);    
	deregisterhook(HOOK_CHANNEL_ADMIN, &helpmod_hook_channel_admin);
    deregisterhook(HOOK_CHANNEL_DEADMIN, &helpmod_hook_channel_deadmin);
    deregisterhook(HOOK_CHANNEL_OWNER, &helpmod_hook_channel_owner);
    deregisterhook(HOOK_CHANNEL_DEOWNER, &helpmod_hook_channel_deowner);
    deregisterhook(HOOK_CHANNEL_TOPIC, &helpmod_hook_channel_topic);
    deregisterhook(HOOK_NICK_ACCOUNT, &helpmod_hook_nick_account);
    deregisterhook(HOOK_SERVER_NEWSERVER, &helpmod_hook_server_newserver);
}
