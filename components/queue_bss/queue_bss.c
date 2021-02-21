/*
 * Copyright (c) 2021, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "esp32/rom/ets_sys.h"
#include "liblfds711.h"
#include "queue_bss.h"

struct queue_bss {
    struct lfds711_queue_bss_state item_queue_state;
    struct lfds711_queue_bss_state item_free_state;
    struct lfds711_queue_bss_element *item_queue_element;
    struct lfds711_queue_bss_element *item_free_element;
    uint8_t *item;
    uint32_t *item_len;
    uint32_t item_max_len;
    uint32_t item_num;
};

queue_bss_handle_t queue_bss_init(uint32_t item_num, uint32_t item_len) {
    struct queue_bss *q = calloc(item_num, item_len);
    if (NULL == q) {
        goto free_q;
    }
    q->item_queue_element = malloc(sizeof(struct lfds711_queue_bss_element) * item_num);
    if (NULL == q->item_queue_element) {
        goto free_item_queue_element;
    }
    q->item_free_element = malloc(sizeof(struct lfds711_queue_bss_element) * item_num);
    if (NULL == q->item_free_element) {
        goto free_item_free_element;
    }
    q->item = malloc(sizeof(uint8_t) * item_len * item_num);
    if (NULL == q->item) {
        goto free_item;
    }
    q->item_len = malloc(sizeof(uint32_t) * item_num);
    if (NULL == q->item_len) {
        goto free_item_len;
    }
    uint8_t *cur_ptr = q->item;
    q->item_max_len = item_len;
    q->item_num = item_num;

    lfds711_queue_bss_init_valid_on_current_logical_core(&q->item_free_state, q->item_free_element, item_num, NULL);

    for (uint32_t i = 0; i < (item_num - 1); i++, cur_ptr += item_len) {
        q->item_len[i] = 0;
        if (!lfds711_queue_bss_enqueue(&q->item_free_state, &q->item_len[i], cur_ptr)) {
            ets_printf("queue_bss: Enqueue fail %d\n", i);
            goto free_item_free_queue;
        }
    }

    lfds711_queue_bss_init_valid_on_current_logical_core(&q->item_queue_state, q->item_queue_element, item_num, NULL);

    return (queue_bss_handle_t)q;

free_item_free_queue:
    lfds711_queue_bss_cleanup(&q->item_free_state, NULL);
free_item_len:
    free(q->item_len);
free_item:
    free(q->item);
free_item_free_element:
    free(q->item_free_element);
free_item_queue_element:
    free(q->item_queue_element);
free_q:
    free(q);
    ets_printf("queue_bss: Init fail\n");
    return NULL;
}

void queue_bss_init_othercores(void) {
    LFDS711_MISC_MAKE_VALID_ON_CURRENT_LOGICAL_CORE_INITS_COMPLETED_BEFORE_NOW_ON_ANY_OTHER_LOGICAL_CORE;
}

int32_t queue_bss_enqueue(queue_bss_handle_t qhdl, uint8_t *item, uint32_t item_len) {
    struct queue_bss *q = (struct queue_bss *)qhdl;
    uint8_t *qitem;
    uint32_t *qitem_len;

    if (NULL == q) {
        ets_printf("queue_bss: NULL handle\n");
        return -1;
    }

    if (item_len > q->item_max_len) {
        ets_printf("queue_bss: Item too large %d, max: %d\n", item_len, q->item_max_len);
        return -1;
    }

    if (!lfds711_queue_bss_dequeue(&q->item_free_state, (void **)&qitem_len, (void **)&qitem)) {
        ets_printf("queue_bss: No free slot\n");
        return -1;
    }
    memcpy(qitem, item, item_len);
    *qitem_len = item_len;

    if (!lfds711_queue_bss_enqueue(&q->item_queue_state, qitem_len, qitem)) {
        ets_printf("queue_bss: Queue full!\n");
        queue_bss_return(qhdl, qitem, qitem_len);
        return -1;
    }
    return 0;
}

uint8_t *queue_bss_dequeue(queue_bss_handle_t qhdl, uint32_t **item_len) {
    struct queue_bss *q = (struct queue_bss *)qhdl;
    uint8_t *qitem;
    uint32_t *qitem_len;

    if (NULL == q || NULL == item_len) {
        ets_printf("queue_bss: NULL paramters\n");
        return NULL;
    }

    if (lfds711_queue_bss_dequeue(&q->item_queue_state, (void **)&qitem_len, (void **)&qitem))
    {
        *item_len = qitem_len;
        return qitem;
    }
    return NULL;
}

int32_t queue_bss_return(queue_bss_handle_t qhdl, uint8_t *item, uint32_t *item_len) {
    struct queue_bss *q = (struct queue_bss *)qhdl;

    if (NULL == q || NULL == item || NULL == item_len) {
        ets_printf("queue_bss: NULL paramters\n");
        return -1;
    }

    if (lfds711_queue_bss_enqueue(&q->item_free_state, item_len, item)) {
        return -1;
    }
    return 0;
}

void queue_bss_deinit(queue_bss_handle_t qhdl) {
    struct queue_bss *q = (struct queue_bss *)qhdl;

    if (NULL == q) {
        ets_printf("queue_bss: NULL handle\n");
        return;
    }

    lfds711_queue_bss_cleanup(&q->item_free_state, NULL);
    free(q->item_len);
    free(q->item);
    free(q->item_free_element);
    free(q->item_queue_element);
    free(q);
}
