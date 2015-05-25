/*
 * Copyright (c) 2015 Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from this
 * software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <nuttx/arch.h>
#include <nuttx/list.h>
#include <nuttx/greybus/greybus.h>
#include <arch/atomic.h>

#include <errno.h>
#include <assert.h>
#include <stdlib.h>

struct gb_work {
    struct gb_operation *operation;
    gb_work_func_t func;
    struct list_head list;
};

static void *gb_workqueue_thread(void *data)
{
    volatile struct gb_workqueue *wq = data;
    struct gb_work *work;
    unsigned int result;
    irqstate_t flags;
    int retval;

    DEBUGASSERT(wq);

    while (!wq->exit) {
        retval = sem_wait((sem_t*) &wq->semaphore);
        if (retval < 0) {
            if (errno != EINTR)
                return NULL;
            continue;
        }

        flags = irqsave();
        work = list_entry(wq->queue.next, struct gb_work, list);
        list_del(&work->list);
        irqrestore(flags);

        if (work->func) {
            result = work->func(work->operation);
            if (result != GB_OP_NO_RESPONSE)
                gb_operation_send_response(work->operation, result);
        }

        gb_operation_unref(work->operation);
        free(work);
    }

    return NULL;
}

struct gb_workqueue *gb_workqueue_create(void)
{
    int retval;
    struct gb_workqueue *wq;

    wq = zalloc(sizeof(*wq));
    if (!wq)
        return NULL;

    list_init(&wq->queue);
    sem_init(&wq->semaphore, 0, 0);

    retval = pthread_create(&wq->thread, NULL, gb_workqueue_thread, wq);
    if (retval)
        goto error_pthread_create;

    return wq;

error_pthread_create:
    free(wq);
    return NULL;
}

int gb_workqueue_queue(struct gb_workqueue *wq, struct gb_operation *operation,
                       gb_work_func_t func)
{
    struct gb_work *work;
    irqstate_t flags;
    int retval;

    DEBUGASSERT(wq);
    DEBUGASSERT(operation);
    DEBUGASSERT(func);

    work = zalloc(sizeof(*work));
    if (!work)
        return -ENOMEM;

    list_init(&work->list);
    work->func = func;
    work->operation = operation;
    gb_operation_ref(operation);

    flags = irqsave();
    list_add(&wq->queue, &work->list);

    retval = sem_post(&wq->semaphore);
    if (retval < 0)
        goto error_sem_post;

    irqrestore(flags);

    return 0;

error_sem_post:
    flags = irqsave();
    list_del(&work->list);
    irqrestore(flags);
    gb_operation_unref(operation);
    free(work);
    return retval;
}

void gb_workqueue_destroy(struct gb_workqueue *wq)
{
    struct gb_work *work;

    if (!wq)
        return;

    wq->exit = true;
    pthread_join(wq->thread, NULL);

    while (!list_is_empty(&wq->queue)) {
        work = list_entry(wq->queue.next, struct gb_work, list);
        list_del(&work->list);
        free(work);
    }

    free(wq);
}
