/*
 * Copyright (c) 2012-2013, Texas Instruments Incorporated
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
 *  ======== ping_rpmsg.c ========
 *
 *  Works with the ping_rpmsg BIOS sample over the rpmsg-proto socket.
 */

#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

/* Ipc Socket Protocol Family */
#include <net/rpmsg.h>

#define CORE0 (0)  /* This should be MultiProc_getId("CORE0") - 1 */

#define NUMLOOPS 100

long diff(struct timespec start, struct timespec end)
{
    struct timespec temp;

    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec - start.tv_sec-1;
        temp.tv_nsec = 1000000000UL + end.tv_nsec - start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }

    return (temp.tv_sec * 1000000UL + temp.tv_nsec / 1000);
}

int main(void)
{
    int sock, err;
    struct sockaddr_rpmsg src_addr, dst_addr;
    socklen_t len;
    const char *msg = "Ping!";
    char buf[512];
    struct timespec   start,end;
    long              elapsed=0,delta;
    int i;

    /* create an RPMSG socket */
    /* QNX PORTING NOTE:  call fd = open("/dev/????", ...); */
    sock = socket(AF_RPMSG, SOCK_SEQPACKET, 0);
    if (sock < 0) {
        printf("socket failed: %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* connect to remote service */
    memset(&dst_addr, 0, sizeof(dst_addr));
    dst_addr.family = AF_RPMSG;
    dst_addr.vproc_id = CORE0;
    dst_addr.addr = 51; // use 51 for ping_tasks;
    //dst_addr.addr = 61; // use 61 for messageQ transport;

    printf("Connecting to address 0x%x on vprocId %d\n",
            dst_addr.addr, dst_addr.vproc_id);

    len = sizeof(struct sockaddr_rpmsg);
    /* QNX PORTING NOTE:  call ioctl(fd, CONNECT_IOTL, *args)*/
    err = connect(sock, (struct sockaddr *)&dst_addr, len);
    if (err < 0) {
        printf("connect failed: %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    /* let's see what local address we got */
    /* QNX PORTING NOTE:  call ioctl(fd, GETLOCALENDPOINT_IOTL, *args)*/
    err = getsockname(sock, (struct sockaddr *)&src_addr, &len);
    if (err < 0) {
        printf("getpeername failed: %s (%d)\n", strerror(errno), errno);
        return -1;
    }

    printf("Our address: socket family: %d, proc id = %d, addr = %d\n",
                 src_addr.family, src_addr.vproc_id, src_addr.addr);

    printf("Sending \"%s\" in a loop.\n", msg);

    for (i = 0; i < NUMLOOPS; i++) {
        clock_gettime(CLOCK_REALTIME, &start);

        /* QNX PORTING NOTE:  call write(fd, msg,len); */
        err = send(sock, msg, strlen(msg) + 1, 0);
        if (err < 0) {
            printf("sendto failed: %s (%d)\n", strerror(errno), errno);
            return -1;
        }

        memset(&src_addr, 0, sizeof(src_addr));

        len = sizeof(src_addr);

        /* QNX PORTING NOTE:  len = read(fd, buf, len); */
        err = recvfrom(sock, buf, sizeof(buf), 0,
                       (struct sockaddr *)&src_addr, &len);

        if (err < 0) {
            printf("recvfrom failed: %s (%d)\n", strerror(errno), errno);
            return -1;
        }
        if (len != sizeof(src_addr)) {
            printf("recvfrom: got bad addr len (%d)\n", len);
            return -1;
        }

        clock_gettime(CLOCK_REALTIME, &end);
        delta = diff(start,end);
        elapsed += delta;

        printf("%d: Received msg: %s, from: %d\n", i, buf, src_addr.addr);

        /*
        printf ("Message time: %ld usecs\n", delta);
        printf("Received a msg from address 0x%x on processor %d\n",
                           src_addr.addr, src_addr.vproc_id);
        printf("Message content: \"%s\".\n", buf);
        */
    }
    printf ("Avg time: %ld usecs over %d iterations\n", elapsed / i, i);

    /* QNX PORTING NOTE:  close(fd); */
    close(sock);

    return 0;
}
