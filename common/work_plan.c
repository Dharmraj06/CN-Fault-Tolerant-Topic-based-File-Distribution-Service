#include "work_plan.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int add_unit(WorkPlan *plan,
                    uint32_t file_index,
                    const FileEntry *file,
                    uint64_t start,
                    uint64_t end)
{
    WorkUnit *unit;

    if (plan == NULL || file == NULL || start > end || end > file->file_size) {
        return -1;
    }

    if (plan->total_units >= plan->capacity_units) {
        uint32_t new_cap = plan->capacity_units ? plan->capacity_units * 2 : 64;
        if (new_cap < plan->capacity_units || new_cap > MAX_WORK_UNITS) {
            return -1;
        }
        WorkUnit *new_units = realloc(plan->units, (size_t)new_cap * sizeof(WorkUnit));
        if (new_units == NULL) {
            return -1;
        }
        plan->units = new_units;
        plan->capacity_units = new_cap;
    }

    unit = &plan->units[plan->total_units];
    memset(unit, 0, sizeof(*unit));

    unit->unit_id = plan->total_units;
    unit->file_index = file_index;

    if (snprintf(unit->filename, sizeof(unit->filename), "%s",
                 file->filename) >= (int)sizeof(unit->filename)) {
        return -1;
    }

    if (snprintf(unit->relative_path, sizeof(unit->relative_path), "%s",
                 file->relative_path) >= (int)sizeof(unit->relative_path)) {
        return -1;
    }

    unit->start_offset = start;
    unit->end_offset = end;
    unit->committed_offset = start;
    unit->state = UNIT_IDLE;
    unit->assigned_worker_id = -1;

    plan->total_units++;
    return 0;
}

int create_work_plan(const TopicManifest *manifest,
                     int num_conns,
                     WorkPlan **out_plan)
{
    WorkPlan *plan;
    uint32_t i;
    uint32_t initial_cap = 64;

    if (out_plan == NULL) {
        return -1;
    }
    *out_plan = NULL;

    if (manifest == NULL || num_conns < 1 || num_conns > 16 ||
        (manifest->file_count > 0 && manifest->files == NULL)) {
        return -1;
    }

    plan = calloc(1, sizeof(*plan));
    if (plan == NULL) {
        return -1;
    }

    if (manifest->file_count >= (uint32_t)num_conns) {
        initial_cap = manifest->file_count > 0 ? manifest->file_count : 16;
    } else if (manifest->file_count > 0) {
        /* Estimate slices needed */
        uint64_t total_slices = 0;
        for (i = 0; i < manifest->file_count; ++i) {
            uint64_t sz = manifest->files[i].file_size;
            if (sz == 0) {
                total_slices += 1;
            } else {
                total_slices += (sz + WORK_SLICE_SIZE - 1) / WORK_SLICE_SIZE;
            }
        }
        if (total_slices > MAX_WORK_UNITS) {
            free(plan);
            return -1;
        }
        initial_cap = (uint32_t)total_slices;
        if (initial_cap < 16) initial_cap = 16;
    } else {
        initial_cap = 16;
    }

    if (initial_cap > MAX_WORK_UNITS) {
        free(plan);
        return -1;
    }

    plan->units = calloc(initial_cap, sizeof(WorkUnit));
    if (plan->units == NULL) {
        free(plan);
        return -1;
    }
    plan->capacity_units = initial_cap;

    if (pthread_mutex_init(&plan->plan_mutex, NULL) != 0) {
        free(plan->units);
        free(plan);
        return -1;
    }
    plan->mutex_initialized = 1;

    /*
     * If there are enough files, assign one complete file per work unit.
     * Workers can claim units dynamically; no file ranges overlap.
     */
    if (manifest->file_count >= (uint32_t)num_conns) {
        for (i = 0; i < manifest->file_count; ++i) {
            const FileEntry *file = &manifest->files[i];

            if (add_unit(plan, i, file, 0, file->file_size) != 0) {
                destroy_work_plan(plan);
                return -1;
            }
        }
    } else {
        /*
         * With fewer files than connections, split large files into
         * 256 KiB slices. 256 KiB is aligned to four 64 KiB chunks.
         * Empty and small files still receive one unit.
         */
        for (i = 0; i < manifest->file_count; ++i) {
            const FileEntry *file = &manifest->files[i];
            uint64_t offset = 0;

            if (file->file_size == 0) {
                if (add_unit(plan, i, file, 0, 0) != 0) {
                    destroy_work_plan(plan);
                    return -1;
                }
                continue;
            }

            while (offset < file->file_size) {
                uint64_t remaining = file->file_size - offset;
                uint64_t length = remaining < WORK_SLICE_SIZE
                                ? remaining : WORK_SLICE_SIZE;

                if (add_unit(plan, i, file, offset, offset + length) != 0) {
                    destroy_work_plan(plan);
                    return -1;
                }
                offset += length;
            }
        }
    }

    *out_plan = plan;
    return 0;
}

WorkUnit *get_next_work_unit(WorkPlan *plan, int worker_id)
{
    uint32_t i;
    WorkUnit *result = NULL;

    if (plan == NULL || !plan->mutex_initialized || worker_id < 0) {
        return NULL;
    }

    pthread_mutex_lock(&plan->plan_mutex);

    for (i = 0; i < plan->total_units; ++i) {
        WorkUnit *unit = &plan->units[i];

        if (unit->state == UNIT_IDLE &&
            unit->committed_offset < unit->end_offset) {
            unit->state = UNIT_ASSIGNED;
            unit->assigned_worker_id = worker_id;
            result = unit;
            break;
        }
    }

    /*
     * Zero-length files need no data writes. Mark their units complete
     * so they do not remain permanently idle.
     */
    if (result == NULL) {
        for (i = 0; i < plan->total_units; ++i) {
            WorkUnit *unit = &plan->units[i];

            if (unit->state == UNIT_IDLE &&
                unit->start_offset == unit->end_offset) {
                unit->state = UNIT_COMMITTED;
                plan->completed_units++;
            }
        }
    }

    pthread_mutex_unlock(&plan->plan_mutex);
    return result;
}

int mark_work_unit_failed(WorkPlan *plan, uint32_t unit_id)
{
    WorkUnit *unit;

    if (plan == NULL || !plan->mutex_initialized ||
        unit_id >= plan->total_units) {
        return -1;
    }

    pthread_mutex_lock(&plan->plan_mutex);
    unit = &plan->units[unit_id];

    if (unit->state != UNIT_ASSIGNED) {
        pthread_mutex_unlock(&plan->plan_mutex);
        return -1;
    }

    /*
     * Preserve committed_offset: a replacement worker resumes from
     * the last committed position, not from the beginning.
     */
    unit->state = UNIT_IDLE;
    unit->assigned_worker_id = -1;

    pthread_mutex_unlock(&plan->plan_mutex);
    return 0;
}

int mark_work_unit_complete(WorkPlan *plan, uint32_t unit_id)
{
    WorkUnit *unit;

    if (plan == NULL || !plan->mutex_initialized ||
        unit_id >= plan->total_units) {
        return -1;
    }

    pthread_mutex_lock(&plan->plan_mutex);
    unit = &plan->units[unit_id];

    if (unit->state != UNIT_ASSIGNED ||
        unit->committed_offset != unit->end_offset) {
        pthread_mutex_unlock(&plan->plan_mutex);
        return -1;
    }

    unit->state = UNIT_COMMITTED;
    unit->assigned_worker_id = -1;
    plan->completed_units++;

    pthread_mutex_unlock(&plan->plan_mutex);
    return 0;
}

void destroy_work_plan(WorkPlan *plan)
{
    if (plan == NULL) {
        return;
    }

    if (plan->mutex_initialized) {
        pthread_mutex_destroy(&plan->plan_mutex);
    }
    free(plan->units);
    free(plan);
}

int mark_work_unit_progress(WorkPlan *plan,
                            uint32_t unit_id,
                            int worker_id,
                            uint64_t committed_offset)
{
    WorkUnit *unit;

    if (plan == NULL || !plan->mutex_initialized ||
        unit_id >= plan->total_units || worker_id < 0) {
        return -1;
    }

    pthread_mutex_lock(&plan->plan_mutex);

    unit = &plan->units[unit_id];

    if (unit->state != UNIT_ASSIGNED ||
        unit->assigned_worker_id != worker_id ||
        committed_offset < unit->committed_offset ||
        committed_offset > unit->end_offset) {
        pthread_mutex_unlock(&plan->plan_mutex);
        return -1;
    }

    unit->committed_offset = committed_offset;

    pthread_mutex_unlock(&plan->plan_mutex);
    return 0;
}

/*
 * Atomically claim a work unit and copy its fields to caller-owned storage.
 * Returns 1 when claimed, 0 when no work remains, and -1 on invalid input
 * or mutex failure.
 */
int claim_next_work_unit(WorkPlan *plan, int worker_id, WorkUnit *out_unit)
{
    uint32_t i;

    if (plan == NULL || out_unit == NULL ||
        !plan->mutex_initialized || worker_id < 0) {
        return -1;
    }

    if (pthread_mutex_lock(&plan->plan_mutex) != 0) {
        return -1;
    }

    for (i = 0; i < plan->total_units; ++i) {
        WorkUnit *unit = &plan->units[i];

        if (unit->state == UNIT_IDLE &&
            unit->committed_offset < unit->end_offset) {
            unit->state = UNIT_ASSIGNED;
            unit->assigned_worker_id = worker_id;
            *out_unit = *unit;

            pthread_mutex_unlock(&plan->plan_mutex);
            return 1;
        }
    }

    /* Empty files need no data transfer. */
    for (i = 0; i < plan->total_units; ++i) {
        WorkUnit *unit = &plan->units[i];

        if (unit->state == UNIT_IDLE &&
            unit->start_offset == unit->end_offset) {
            unit->state = UNIT_COMMITTED;
            unit->assigned_worker_id = -1;
            plan->completed_units++;
        }
    }

    pthread_mutex_unlock(&plan->plan_mutex);
    return 0;
}
