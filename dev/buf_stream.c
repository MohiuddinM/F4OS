#include <stddef.h>
#include <stdlib.h>
#include <mm/mm.h>
#include <kernel/sched.h>
#include <kernel/semaphore.h>
#include <kernel/fault.h>
#include <dev/resource.h>

#include <dev/buf_stream.h>

struct buf_stream {
    char *buf;
};

/* Warning! Buffer streams use the buffer you pass them, they do not copy them! */
rd_t open_buf_stream(char *buf);
char buf_stream_read(void *env, int *error);
int buf_stream_write(char c, void *env);
int buf_stream_close(resource *resource);

rd_t open_buf_stream(char *buf) {
    struct buf_stream *env = malloc(sizeof(struct buf_stream)); 
    if (!env) {
        printk("OOPS: Could not allocate space for buffer stream resource.\r\n");
        return -1;
    }

    resource *new_r = create_new_resource();
    if (!new_r) {
        printk("OOPS: Unable to allocate space for buffer stream resource.\r\n");
        free(env);
        return -1;
    }

    env->buf = buf;

    new_r->env    = env;
    new_r->writer = &buf_stream_write;
    new_r->swriter = NULL;
    new_r->reader = &buf_stream_read;
    new_r->closer = &buf_stream_close;
    new_r->read_sem = malloc(sizeof(semaphore));
    if (new_r->read_sem) {
        init_semaphore(new_r->read_sem);
    }
    else {
        printk("OOPS: Unable to allocate memory for buffer stream semaphore.\r\n");
        kfree(new_r);
        free(env);
        return -1;
    }
    new_r->write_sem = new_r->read_sem;

    return add_resource(curr_task->task, new_r);
}

char buf_stream_read(void *env, int *error) {
    if (error != NULL) {
        *error = 0;
    }

    return *((struct buf_stream *) env)->buf == '\0' ? '\0' : *((struct buf_stream *) env)->buf++;
}

int buf_stream_write(char c, void *env) {
    *((struct buf_stream *) env)->buf++ = c;
    *((struct buf_stream *) env)->buf = '\0';
    return 1;
}

int buf_stream_close(resource *resource) {
    acquire_for_free(resource->read_sem);
    free(resource->env);
    free((void*) resource->read_sem);
    return 0;
}
