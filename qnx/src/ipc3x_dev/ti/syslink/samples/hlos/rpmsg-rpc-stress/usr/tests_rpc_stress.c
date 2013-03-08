/*
 * Copyright (c) 2013, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * tests_rpc_stress.c
 *
 * Stress tests for rpmsg-rpc.
 *
 */

#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>
#include <semaphore.h>

#include "ti/ipc/rpmsg_rpc.h"


typedef struct {
    int a;
} fxn_triple_args;

/* Note: Set bit 31 to indicate static function indicies:
 * This function order will be hardcoded on BIOS side, hence preconfigured:
 */
#define FXN_IDX_FXNTRIPLE            (1 | 0x80000000)

#define NUM_ITERATIONS               20

static int test_status = 0;
static bool runTest = true;

int exec_cmd(int fd, char *msg, size_t len, char *reply_msg, int *reply_len)
{
    int ret = 0;

    ret = write(fd, msg, len);
    if (ret < 0) {
        perror("Can't write to rpc_example instance");
        return -1;
    }

    /* Now, await normal function result from rpc_example service:
     * Note: len should be max length of response expected.
     */
    ret = read(fd, reply_msg, len);
    if (ret < 0) {
        perror("Can't read from rpc_example instance");
        return -1;
    }
    else {
        *reply_len = ret;
    }
    return 0;
}

int send_cmd(int fd, char *msg, int len)
{
    int ret = 0;

    ret = write(fd, msg, len);
    if (ret < 0) {
         perror("Can't write to rpc_example instance\n");
         return -1;
    }

    return(0);
}

int recv_cmd(int fd, int len, char *reply_msg, int *reply_len)
{
    int ret = 0;

    /* Now, await normal function result from rpc_example service: */
    // Note: len should be max length of response expected.
    ret = read(fd, reply_msg, len);
    if (ret < 0) {
         perror("Can't read from rpc_example instance\n");
         return -1;
    }
    else {
          *reply_len = ret;
    }
    return(0);
}

typedef struct test_exec_args {
    int fd;
    int start_num;
    int test_num;
    sem_t * sem;
    int thread_num;
} test_exec_args;

static pthread_t * clientThreads = NULL;
static sem_t * clientSems = NULL;
static char **clientPackets = NULL;
static bool *readMsg = NULL;
static int *fds;

void * test_exec_call(void * arg)
{
    int               i;
    int               packet_len;
    int               reply_len;
    char              packet_buf[512] = {0};
    char              return_buf[512] = {0};
    test_exec_args    *args = (test_exec_args *) arg;
    int               fd = args->fd;
    struct rppc_function *function;
    struct rppc_function_return *returned;

    for (i = args->start_num; i < args->start_num + NUM_ITERATIONS; i++) {
        function = (struct rppc_function *)packet_buf;
        function->fxn_id = FXN_IDX_FXNTRIPLE;
        function->num_params = 1;
        function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
        function->params[0].size = sizeof(int);
        function->params[0].data = i;
        function->num_translations = 0;

        returned = (struct rppc_function_return *)return_buf;

        /* Exec command: */
        packet_len = sizeof(struct rppc_function) +\
                     (function->num_translations * \
                      sizeof(struct rppc_param_translation));
        if (args->test_num == 1)  {
            if (exec_cmd(fd, (char *)packet_buf, packet_len,
                         (char *)return_buf, &reply_len)) {
                test_status = -1;
                break;
            }
        }
        else if (args->test_num == 2 || args->test_num == 3) {
            if (send_cmd(fd, (char *)packet_buf, packet_len)) {
                test_status = -1;
                break;
            }
            sem_wait(&clientSems[args->thread_num]);
            memcpy(return_buf, clientPackets[args->thread_num], 512);
            readMsg[args->thread_num] = true;
        }
       if (i * 3 != returned->status) {
           printf ("rpc_stress: "
                   "called fxnTriple(%d), result = %d, expected %d\n",
                        function->params[0].data, returned->status, i * 3);
           test_status = -1;
           break;
       }
       else {
           printf ("rpc_stress: called fxnTriple(%d), result = %d\n",
                        function->params[0].data, returned->status);
       }
    }

    return NULL;
}

void * test_select_thread (void * arg)
{
    int fd;
    int reply_len;
    char return_buf[512] = {0};
    struct rppc_function_return *rtn_packet =
                               (struct rppc_function_return *)return_buf;
    int n, i;
    fd_set rfd;
    int max_fd = -1;

    while (runTest) {
        FD_ZERO(&rfd);
        for (i = 0; i < (int)arg; i++) {
            FD_SET(fds[i], &rfd);
            max_fd = max(max_fd, fds[i]);
        }
        n = select(1 + max_fd, &rfd, NULL, NULL, NULL);
        switch (n) {
            case -1:
                perror("select");
                return NULL;
            default:
                for (i = 0; i < (int)arg; i++) {
                    if (FD_ISSET(fds[i], &rfd)) {
                        fd = fds[i];
                        break;
                    }
                }
                break;
        }
        if (recv_cmd(fd, 512, (char *)rtn_packet, &reply_len)) {
            test_status = -1;
            printf("test_select_thread: recv_cmd failed!");
            break;
        }

        if (runTest == false)
            break;

        /* Decode reply: */
        while (readMsg[i] == false) {
            sleep(1);
        }
        memcpy(clientPackets[i], rtn_packet, 512);
        readMsg[i] = false;
        sem_post(&clientSems[i]);
    }
    return NULL;
}

void * test_read_thread (void * arg)
{
    int fd = (int)arg;
    int reply_len;
    char  return_buf[512] = {0};
    struct rppc_function_return *rtn_packet =
                               (struct rppc_function_return *)return_buf;
    int               packet_id;

    while (runTest) {
        if (recv_cmd(fd, 512, (char *)rtn_packet, &reply_len)) {
            test_status = -1;
            printf("test_read_tread: recv_cmd failed!");
            break;
        }

        if (runTest == false)
            break;

        /* Decode reply: */
        packet_id = ((rtn_packet->status / 3) - 1) / NUM_ITERATIONS;
        while (readMsg[packet_id] == false) {
            sleep(1);
        }
        memcpy(clientPackets[packet_id], rtn_packet, 512);
        readMsg[packet_id] = false;
        sem_post(&clientSems[packet_id]);
    }
    return NULL;
}

int test_rpc_stress_select(int core_id, int num_comps)
{
    int ret = 0;
    int i = 0, j = 0;
    struct rppc_create_instance connreq;
    struct rppc_function *function;
    pthread_t select_thread;
    int               packet_len;
    char              packet_buf[512] = {0};
    test_exec_args args[num_comps];

    fds = malloc (sizeof(int) * num_comps);
    if (!fds) {
        return -1;
    }
    for (i = 0; i < num_comps; i++) {
        /* Connect to the rpc_example ServiceMgr on the specified core: */
        if (core_id == 0) {
            fds[i] = open("/dev/rpmsg-omx0", O_RDWR);
            if (fds[i] < 0) {
                perror("Can't open OMX device");
                ret = -1;
                break;
            }
            strcpy(connreq.name, "rpmsg-omx0");
        }
        else if (core_id == 1) {
            fds[i] = open("/dev/rpc_example", O_RDWR);
            if (fds[i] < 0) {
                perror("Can't open rpc_example device");
                break;
            }
            strcpy(connreq.name, "rpc_example");
        }
        else if (core_id == 2) {
            fds[i] = open("/dev/rpmsg-omx2", O_RDWR);
            if (fds[i] < 0) {
                perror("Can't open OMX device");
                break;
            }
            strcpy(connreq.name, "rpmsg-omx2");
        }
        /* Create an rpc_example server instance, and rebind its address to this
        * file descriptor.
        */
        ret = ioctl(fds[i], RPPC_IOC_CREATE, &connreq);
        if (ret < 0) {
            perror("Can't connect to rpc_example instance");
            close(fds[i]);
            break;
        }
        printf("rpc_sample: Connected to %s\n", connreq.name);
    }
    if (i != num_comps) {
        /* cleanup */
        for (j = 0; j < i; j++) {
            ret = close(fds[j]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        free(fds);
        return -1;
    }

    clientSems = malloc(sizeof(sem_t) * num_comps);
    if (!clientSems) {
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    readMsg = malloc(sizeof(bool) * num_comps);
    if (!readMsg) {
        free(clientSems);
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    clientPackets = malloc(sizeof(char *) * num_comps);
    if (!clientPackets) {
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    for (i = 0; i < num_comps; i++) {
        clientPackets[i] = malloc(512 * sizeof(char));
        if (!clientPackets[i]) {
            for (j = 0; j < i; j++) {
                free(clientPackets[j]);
            }
            free(clientPackets);
            free(readMsg);
            free(clientSems);
            free(clientThreads);
            for (i = 0; i < num_comps; i++) {
                ret = close(fds[i]);
                if (ret < 0) {
                    perror("Can't close rpc_example fd ??");
                }
            }
            return -1;
        }
    }

    ret = pthread_create(&select_thread, NULL, test_select_thread,
                         (void *)num_comps);
    if (ret < 0) {
        perror("Can't create thread");
        ret = -1;
    }

    clientThreads = malloc(sizeof(pthread_t) * num_comps);
    if (!clientThreads) {
        for (i = 0; i < num_comps; i++) {
            ret = close(fds[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        free(fds);
        return -1;
    }

    for ( i = 0; i < num_comps; i++) {
        ret = sem_init(&clientSems[i], 0, 0);
        args[i].fd = fds[i];
        args[i].start_num = 1;
        args[i].test_num = 3;
        args[i].sem = &clientSems[i];
        args[i].thread_num = i;
        readMsg[i] = true;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
    }

    free(clientThreads);

    function = (struct rppc_function *)packet_buf;
    function->fxn_id = FXN_IDX_FXNTRIPLE;
    function->num_params = 1;
    function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
    function->params[0].size = sizeof(int);
    function->params[0].data = i;
    function->num_translations = 0;

    /* Exec command: */
    packet_len = sizeof(struct rppc_function) +\
                 (function->num_translations *\
                  sizeof(struct rppc_param_translation));

    runTest = false;
    if (send_cmd(fds[0], (char *)packet_buf, packet_len)) {
        test_status = -1;
    }

    pthread_join(select_thread, NULL);

    for (i = 0; i < num_comps; i++) {
        free(clientPackets[i]);
    }
    free(clientPackets);
    free(readMsg);
    free(clientSems);

    for (i = 0; i < num_comps; i++) {
        /* Terminate connection and destroy rpc_example instance */
        ret = close(fds[i]);
        if (ret < 0) {
            perror("Can't close rpc_example fd ??");
            ret = -1;
        }
        printf("rpc_sample: Closed connection to %s!\n", connreq.name);
    }

    free(fds);
    return ret;
}

int test_rpc_stress_multi_threads(int core_id, int num_threads)
{
    int ret = 0;
    int i = 0, j = 0;
    int fd;
    int packet_len;
    char packet_buf[512] = {0};
    pthread_t read_thread;
    struct rppc_create_instance connreq;
    struct rppc_function *function;
    test_exec_args args[num_threads];

    /* Connect to the rpc_example ServiceMgr on the specified core: */
    if (core_id == 0) {
        fd = open("/dev/rpmsg-omx0", O_RDWR);
        if (fd < 0) {
            perror("Can't open OMX device");
            return -1;
        }
            strcpy(connreq.name, "rpmsg-omx0");

    }
    else if (core_id == 1) {
        fd = open("/dev/rpc_example", O_RDWR);
        if (fd < 0) {
            perror("Can't open rpc_example device");
            return -1;
        }
            strcpy(connreq.name, "rpc_example");
    }
    else if (core_id == 2) {
        fd = open("/dev/rpmsg-omx2", O_RDWR);
        if (fd < 0) {
            perror("Can't open OMX device");
            return -1;
        }
            strcpy(connreq.name, "rpmsg-omx2");
    }
    /* Create an rpc_example server instance, and rebind its address to this
    * file descriptor.
    */
    ret = ioctl(fd, RPPC_IOC_CREATE, &connreq);
    if (ret < 0) {
        perror("Can't connect to rpc_example instance");
        close(fd);
        return -1;
    }
    printf("rpc_sample: Connected to %s\n", connreq.name);

    clientThreads = malloc(sizeof(pthread_t) * num_threads);
    if (!clientThreads) {
        ret = close(fd);
        return -1;
    }
    clientSems = malloc(sizeof(sem_t) * num_threads);
    if (!clientSems) {
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    readMsg = malloc(sizeof(bool) * num_threads);
    if (!readMsg) {
        free(clientSems);
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    clientPackets = malloc(sizeof(char *) * num_threads);
    if (!clientPackets) {
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        ret = close(fd);
        return -1;
    }

    for (i = 0; i < num_threads; i++) {
        clientPackets[i] = malloc(512 * sizeof(char));
        if (!clientPackets[i]) {
            for (j = 0; j < i; j++) {
                free(clientPackets[j]);
            }
            free(clientPackets);
            free(readMsg);
            free(clientSems);
            free(clientThreads);
            close(fd);
            return -1;
        }
    }

    ret = pthread_create(&read_thread, NULL, test_read_thread, (void *)fd);
    if (ret < 0) {
        perror("Can't create thread");
        for (i = 0; i < num_threads; i++) {
            free(clientPackets[i]);
        }
        free(clientPackets);
        free(readMsg);
        free(clientSems);
        free(clientThreads);
        close(fd);
        return -1;
    }

    for ( i = 0; i < num_threads; i++) {
        ret = sem_init(&clientSems[i], 0, 0);
        args[i].fd = fd;
        args[i].start_num = (i * NUM_ITERATIONS) + 1;
        args[i].test_num = 2;
        args[i].sem = &clientSems[i];
        args[i].thread_num = i;
        readMsg[i] = true;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
        sem_destroy(&clientSems[j]);
    }

    function = (struct rppc_function *)packet_buf;
    function->fxn_id = FXN_IDX_FXNTRIPLE;
    function->num_params = 1;
    function->params[0].type = RPPC_PARAM_TYPE_ATOMIC;
    function->params[0].size = sizeof(int);
    function->params[0].data = i;
    function->num_translations = 0;

    /* Exec command: */
    packet_len = sizeof(struct rppc_function) +\
                 (function->num_translations *\
                  sizeof(struct rppc_param_translation));

    runTest = false;
    if (send_cmd(fd, (char *)packet_buf, packet_len)) {
        test_status = -1;
    }

    pthread_join(read_thread, NULL);

    for (i = 0; i < num_threads; i++) {
        free(clientPackets[i]);
    }
    free(clientPackets);
    free(readMsg);
    free(clientSems);
    free(clientThreads);

    /* Terminate connection and destroy rpc_example instance */
    ret = close(fd);
    if (ret < 0) {
        perror("Can't close rpc_example fd ??");
        ret = -1;
    }
    printf("rpc_sample: Closed connection to %s!\n", connreq.name);

    return ret;
}

int test_rpc_stress_multi_srvmgr(int core_id, int num_comps)
{
    int ret = 0;
    int i = 0, j = 0;
    int fd[num_comps];
    struct rppc_create_instance connreq;
    test_exec_args args[num_comps];

    for (i = 0; i < num_comps; i++) {
        /* Connect to the rpc_example ServiceMgr on the specified core: */
        if (core_id == 0) {
            fd[i] = open("/dev/rpmsg-omx0", O_RDWR);
            if (fd[i] < 0) {
                perror("Can't open OMX device");
                ret = -1;
                break;
            }
            strcpy(connreq.name, "rpmsg-omx0");
        }
        else if (core_id == 1) {
            fd[i] = open("/dev/rpc_example", O_RDWR);
            if (fd[i] < 0) {
                perror("Can't open rpc_example device");
                ret = -1;
                break;
            }
            strcpy(connreq.name, "rpc_example");
        }
        else if (core_id == 2) {
            fd[i] = open("/dev/rpmsg-omx2", O_RDWR);
            if (fd[i] < 0) {
                perror("Can't open OMX device");
                ret = -1;
                break;
            }
            strcpy(connreq.name, "rpmsg-omx2");
        }
        /* Create an rpc_example server instance, and rebind its address to this
        * file descriptor.
        */
        ret = ioctl(fd[i], RPPC_IOC_CREATE, &connreq);
        if (ret < 0) {
            perror("Can't connect to rpc_example instance");
            close(fd[i]);
            break;
        }
        printf("rpc_sample: Connected to %s\n", connreq.name);
    }
    if (i != num_comps) {
        /* cleanup */
        for (j = 0; j < i; j++) {
            ret = close(fd[j]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    clientThreads = malloc(sizeof(pthread_t) * num_comps);
    if (!clientThreads) {
        for (i = 0; i < num_comps; i++) {
            ret = close(fd[i]);
            if (ret < 0) {
                perror("Can't close rpc_example fd ??");
            }
        }
        return -1;
    }

    for ( i = 0; i < num_comps; i++) {
        args[i].fd = fd[i];
        args[i].start_num = 1;
        args[i].test_num = 1;
        args[i].sem = NULL;
        args[i].thread_num = i;
        ret = pthread_create(&clientThreads[i], NULL, test_exec_call,
                             (void *)&args[i]);
        if (ret < 0) {
            perror("Can't create thread");
            ret = -1;
            break;
        }
        printf("Created thread %d\n", i);
    }

    for (j = 0; j < i; j++) {
        printf("Join thread %d\n", j);
        pthread_join(clientThreads[j], NULL);
    }

    free(clientThreads);

    for (i = 0; i < num_comps; i++) {
        /* Terminate connection and destroy rpc_example instance */
        ret = close(fd[i]);
        if (ret < 0) {
            perror("Can't close rpc_example fd ??");
            ret = -1;
        }
        printf("rpc_sample: Closed connection to %s!\n", connreq.name);
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int ret;
    int test_id = -1;
    int core_id = 0;
    int num_comps = 1;
    int num_threads = 1;
    int c;

    while (1)
    {
        c = getopt (argc, argv, "t:c:x:l:");
        if (c == -1)
            break;

        switch (c)
        {
        case 't':
            test_id = atoi(optarg);
            break;
        case 'c':
            core_id = atoi(optarg);
            break;
        case 'x':
            num_comps = atoi(optarg);
            break;
        case 'l':
            num_threads = atoi(optarg);
            break;
        default:
            printf ("Unrecognized argument\n");
        }
    }

    if (test_id < 0 || test_id > 3) {
        printf("Invalid test id\n");
        return 1;
    }

    switch (test_id) {
        case 1:
            /* multiple threads each with an RPMSG-RPC ServiceMgr instance */
            if (core_id < 0 || core_id > 2) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_comps < 0) {
                printf("Invalid num comps id\n");
                return 1;
            }
            ret = test_rpc_stress_multi_srvmgr(core_id, num_comps);
            break;
        case 2:
            /* Multiple threads, 1 RPMSG-RPC ServiceMgr instances */
            if (core_id < 0 || core_id > 2) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_threads < 0) {
                printf("Invalid num threads\n");
                return 1;
            }
            ret = test_rpc_stress_multi_threads(core_id, num_threads);
            break;
        case 3:
            /* 1 thread using multiple RPMSG-RPC ServiceMgr instances */
            if (core_id < 0 || core_id > 2) {
                printf("Invalid core id\n");
                return 1;
            }
            if (num_comps < 0) {
                printf("Invalid num comps id\n");
                return 1;
            }
            ret = test_rpc_stress_select(core_id, num_comps);
            break;
        default:
            break;
    }

    if (ret < 0 || test_status < 0) {
        printf ("TEST STATUS: FAILED.\n");
    }
    else {
        printf ("TEST STATUS: PASSED.\n");
    }
    return 0;
}
