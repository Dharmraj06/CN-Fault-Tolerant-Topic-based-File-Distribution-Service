#ifndef WORK_PLAN_H
#define WORK_PLAN_H

#include <stdint.h>
#include <pthread.h>
#include "topic_manager.h"

#define MAX_WORK_UNITS 1024
#define WORK_SLICE_SIZE (256ULL * 1024ULL)

typedef enum {
    UNIT_IDLE = 0,
    UNIT_ASSIGNED,
    UNIT_COMMITTED,
    UNIT_FAILED
} WorkUnitState;

typedef struct {
    uint32_t unit_id;
    uint32_t file_index;
    char filename[MAX_FILENAME_LEN];
    char relative_path[MAX_PATH_BUFFER_LEN];
    uint64_t start_offset;
    uint64_t end_offset;       /* Exclusive: [start_offset, end_offset) */
    uint64_t committed_offset; /* Absolute offset within the file */
    WorkUnitState state;
    int assigned_worker_id;
} WorkUnit;

typedef struct {
    uint64_t session_id;
    uint32_t total_units;
    uint32_t completed_units;
    WorkUnit units[MAX_WORK_UNITS];
    pthread_mutex_t plan_mutex;
    int mutex_initialized;
} WorkPlan;

int create_work_plan(const TopicManifest *manifest,
                     int num_conns,
                     WorkPlan **out_plan);

WorkUnit *get_next_work_unit(WorkPlan *plan, int worker_id);
/* Safely claim and copy a work unit while holding the mutex. */
int claim_next_work_unit(WorkPlan *plan, int worker_id, WorkUnit *out_unit);

int mark_work_unit_progress(WorkPlan *plan, uint32_t unit_id, int worker_id, uint64_t committed_offset);
int mark_work_unit_failed(WorkPlan *plan, uint32_t unit_id);
int mark_work_unit_complete(WorkPlan *plan, uint32_t unit_id);
void destroy_work_plan(WorkPlan *plan);

#endif
