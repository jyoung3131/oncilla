/**
 * file: evmsg.c
 * author: Alexander Merritt, merritt.alex@gatech.edu
 * desc: EVPath state and initialization
 */

/* System includes */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Other project includes */
#include <evpath.h>
#include <gen_thread.h>

/* Project includes */
#include <evpath_msg.h>

/* Globals */

FMField evmsg_fields[] =
{
    {"rank", "integer", sizeof(int), FMOffset(evmsg_ptr, rank)},
    {NULL, NULL, 0, 0}
};

FMStructDescRec evfmt_list[] =
{
    {"evmsg", evmsg_fields, sizeof(struct evmsg), NULL},
    {NULL, NULL, 0, NULL}
};

/* Internal state */

static CManager cm;
static EVstone recv_stone;

/* Private functions */

/* evpath-registered message handler */
static int
process_msg(CManager cm, void *vevent, void *data, attr_list attrs)
{
    evmsg_ptr msg = vevent;
    printf("pid %d got a message: rank %d\n", getpid(), msg->rank);
    return 0;
}

/* thread for receiving incoming messages */
static void *
recv_thread(void *arg)
{
    attr_list attrs = create_attr_list();
    add_int_attr(attrs, attr_atom_from_string("IP_PORT"), 12345);
    CMlisten_specific(cm, attrs);

    /* create a recv stone and associate handler for incoming messages */
    recv_stone = EValloc_stone(cm);
    EVassoc_terminal_action(cm, recv_stone, evfmt_list, process_msg, NULL);

    /* block forever */
    CMrun_network(cm);
    pthread_exit(NULL);
}

static int
spawn_listener(void)
{
    pthread_t ignored;
    return pthread_create(&ignored, NULL, recv_thread, NULL);
}

static void
fixenv(void)
{
    const int envlen = 256;
    char *env = NULL;
    char envmod[envlen];

    /* append evpath path to LD_LIBRARY_PATH as MPI seems to erase it */
    env = getenv("LD_LIBRARY_PATH");
    if (env) strncpy(envmod, env, envlen);
    strncat(envmod, ":/opt/share/evpath/lib", envlen);
    setenv("LD_LIBRARY_PATH", envmod, 1);

    //setenv("EVerbose", "1", 1);
}

/* Public functions */

int ev_init(int rank)
{
    fixenv();

    gen_pthread_init(); /* tell evpath i'm using pthreads */
    cm = CManager_create();

    spawn_listener();
    sleep(4); /* XXX allow CMrun_network to be called before proceeding */

    evmsg msg;

    if (rank == 0)
    {
        /* nothing? */
    }
    else
    {
        msg.rank = rank;

        EVsource source;
        attr_list contact_list;
        EVstone remote_stone = 0; /* first stone */

        EVstone stone = EValloc_stone(cm);
        contact_list = create_attr_list();
        add_int_attr(contact_list, attr_atom_from_string("IP_PORT"), 12345);
        add_string_attr(contact_list, attr_atom_from_string("IP_HOST"), "octane1");
        EVassoc_bridge_action(cm, stone, contact_list, remote_stone);

        source = EVcreate_submit_handle(cm, stone, evfmt_list);
        EVsubmit(source, &msg, NULL);

        //spawn_listener();
    }

    printf("rank%d sleeping 5\n", rank);
    sleep(5);

    return 0;
}
