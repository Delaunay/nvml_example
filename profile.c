/*****************************************************************************\
 *  acct_gather_profile_none.c - slurm profile accounting plugin for none.
 *****************************************************************************
 *  Copyright (C) 2013 Bull S. A. S.
 *		Bull, Rue Jean Jaures, B.P.68, 78340, Les Clayes-sous-Bois.
 *
 *  Written by Rod Schultz <rod.schultz@bull.com>
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
\*****************************************************************************/

#include "src/common/slurm_xlator.h"
#include "src/common/slurm_jobacct_gather.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/slurm_protocol_defs.h"
#include "src/slurmd/common/proctrack.h"
#include "src/common/slurm_acct_gather_profile.h"

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>

#include <curl/curl.h>
#include <nvml.h>


#define _DEBUG 1

/*
 * These variables are required by the generic plugin interface.  If they
 * are not found in the plugin, the plugin loader will ignore it.
 *
 * plugin_name - a string giving a human-readable description of the
 * plugin.  There is no maximum length, but the symbol must refer to
 * a valid string.
 *
 * plugin_type - a string suggesting the type of the plugin or its
 * applicability to a particular form of data or method of data handling.
 * If the low-level plugin API is used, the contents of this string are
 * unimportant and may be anything.  Slurm uses the higher-level plugin
 * interface which requires this string to be of the form
 *
 *	<application>/<method>
 *
 * where <application> is a description of the intended application of
 * the plugin (e.g., "jobacct" for Slurm job completion logging) and <method>
 * is a description of how this plugin satisfies that application.  Slurm will
 * only load job completion logging plugins if the plugin_type string has a
 * prefix of "jobacct/".
 *
 * plugin_version - an unsigned 32-bit integer containing the Slurm version
 * (major.minor.micro combined into a single number).
 */
const char plugin_name[] = "AcctGatherProfile GPU plugin";
const char plugin_type[] = "acct_gather_profile/mila_gpu";


int check(nvmlReturn_t error, const char *file, const char *fun, int line, const char *call_str) 
{
    if (error != NVML_SUCCESS) {
        debug("[!] %s/%s:%d %s %s\n", file, fun, line, nvmlErrorString(error), call_str);
        return 0;
    };
    return 1;
}
#define CHK(X) check(X, __FILE__, __func__, __LINE__, #X)

enum {
    FIELD_GPUFREQ,  // 1.8 Ghz
    FIELD_GPUMEM,   // Mem in Mo
    FIELD_GPUUTIL,  // Util %
    FIELD_GPUPOWER, // Power usage
	FIELD_CNT
};

typedef struct {
	char ** names;
	uint32_t *types;
	size_t size;
	char * name;
} table_t;

typedef struct {
    uint32_t def;
	char *dir;
} slurm_mila_conf_t;

union data_t{
	uint64_t u;
	double	 d;
};

static          slurm_mila_conf_t mila_conf;
static uint32_t g_profile_running = ACCT_GATHER_PROFILE_NOT_SET;
static          stepd_step_rec_t *g_job = NULL;

// NVML variables
static nvmlDevice_t* devices = NULL;
static uint32_t      device_count = 0;

// Metrics we will populate
static acct_gather_profile_dataset_t new_dataset_fields[] = {
    { "GPUFrequency", PROFILE_FIELD_DOUBLE },
    { "GPUMem", PROFILE_FIELD_DOUBLE },
    { "GPUUtilization", PROFILE_FIELD_DOUBLE },
    { "GPUPower", PROFILE_FIELD_DOUBLE },
	{ NULL, PROFILE_FIELD_NOT_SET }
};
// Offset storing where our GPU data starts in the dataset
static uint32_t datasset_offset = 0;
static acct_gather_profile_dataset_t* profile_dataset = NULL;
static uint32_t profile_dataset_len = 0;

/*
 * init() is called when the plugin is loaded, before any other functions
 * are called.  Put global initialization here.
 */
extern int init(void)
{
	debug("%s loaded", plugin_name);
    CHK(nvmlInitWithFlags(NVML_INIT_FLAG_NO_GPUS));

    // CHK(nvmlDeviceSetAccountingMode());
	return SLURM_SUCCESS;
}

extern int fini(void)
{
    CHK(nvmlShutdown());
	return SLURM_SUCCESS;
}

/**
 * define the options this plugin is going to load
 */
extern void acct_gather_profile_p_conf_options(s_p_options_t **full_options,
					       int *full_options_cnt)
{
	debug3("%s %s called", plugin_type, __func__);

	s_p_options_t options[] = {
		{"ProfileMilaGpuDir", S_P_STRING},
		{NULL} };

	transfer_s_p_options(full_options, options, full_options_cnt);
	return;
}



static bool _run_in_daemon(void)
{
	static bool set = false;
	static bool run = false;

	debug3("%s %s called", plugin_type, __func__);

	if (!set) {
		set = 1;
		run = run_in_daemon("slurmstepd");
	}

	return run;
}


/**
 * Read configuration file `acct_gather.conf` and set the configuration in `mila_conf`
 */
extern void acct_gather_profile_p_conf_set(s_p_hashtbl_t *tbl)
{
	// char *tmp = NULL;
	// _reset_slurm_profile_conf();

	if (tbl) {
		s_p_get_string(&mila_conf.dir, "ProfileMilaGpuDir", tbl);
	}

	if (!mila_conf.dir)
		fatal("No ProfileMilaGpuDir in your acct_gather.conf file.  "
		      "This is required to use the %s plugin", plugin_type);

	debug("%s loaded", plugin_name);
}


/**
 * This is for people outside our pluging that wants to check its config.
 * it is quite useless as it seems the enum is just for HDF5 profiling.
 * better to use `acct_gather_profile_p_conf_values` instead
 */
extern void acct_gather_profile_p_get(enum acct_gather_profile_info info_type,
				      void *data)
{
	uint32_t *uint32 = (uint32_t *) data;
    char **tmp_char = (char **) data;

	switch (info_type) {
    case ACCT_GATHER_PROFILE_DIR:
        *tmp_char = xstrdup(mila_conf.dir);
		break;

	case ACCT_GATHER_PROFILE_DEFAULT:
        *uint32 = mila_conf.def;
        break;

	case ACCT_GATHER_PROFILE_RUNNING:
		*uint32 = g_profile_running;
		break;
	default:
		break;
	}

	return;
}

/**
 * Export plugin configuration to hashtable
 */
extern void acct_gather_profile_p_conf_values(List *data)
{
	config_key_pair_t *key_pair;
	debug3("%s %s called", plugin_type, __func__);

	xassert(*data);

	key_pair = xmalloc(sizeof(config_key_pair_t));
	key_pair->name = xstrdup("ProfileMilaGpuDir");
	key_pair->value = xstrdup(mila_conf.dir);
	list_append(*data, key_pair);

    key_pair = xmalloc(sizeof(config_key_pair_t));
	key_pair->name = xstrdup("ProfileMilaGpuDefault");
	key_pair->value =
		xstrdup(acct_gather_profile_to_string(mila_conf.def));
	list_append(*data, key_pair);

	return;
}

/**
 * force profiling 
 */
extern bool acct_gather_profile_p_is_active(uint32_t type)
{
	return true;
}

/* Called once per step on each node from slurmstepd, before launching tasks.
 * Provides an opportunity to create files and other node-step level initialization. 
 */
extern int acct_gather_profile_p_node_step_start(stepd_step_rec_t* job)
{
    // This is where we can handle the --profile=%s options
    // we are not interested in this yet
    xassert(_run_in_daemon());
    g_job = job;

    // Get device the job has access to;
    // dump environment
    char **iter = job->env;

	while (*iter != NULL) {
		size_t n = strlen(*iter);

        printf("%s=%s\n", *iter, iter[n + 1]);
		iter += 1;
	}

	return SLURM_SUCCESS;
}


extern int acct_gather_profile_p_child_forked(void)
{
	return SLURM_SUCCESS;
}

extern int acct_gather_profile_p_node_step_end(void)
{
    /*
    for(int i = 0; i < profile_dataset_len; i++){
        xfree(profile_dataset[i].name);
    }
    xfree(profile_dataset);
    */
    
	return SLURM_SUCCESS;
}

// 
extern int acct_gather_profile_p_task_start(uint32_t taskid)
{
    xassert(_run_in_daemon());
	xassert(g_job);

	return SLURM_SUCCESS;
}

extern int acct_gather_profile_p_task_end(pid_t taskpid)
{
	return SLURM_SUCCESS;
}

extern int acct_gather_profile_p_create_group(const char* name)
{
	return SLURM_SUCCESS;
}

/**
 * void* data contains data that was gathered by the jobacct_gather call 
 *  `_record_profile`
 */
extern int acct_gather_profile_p_add_sample_data(int dataset_id, void* data,
						 time_t sample_time)
{
    debug("acct_gather_profile_p_add_sample_data %d", dataset_id);

    acct_gather_profile_dataset_t* iter = profile_dataset;

    struct passwd *pws;
    pws = getpwuid(g_job->uid);
    const char* username = pws->pw_name;

    static const char* UINT64_FMT = 
        "%s,user=%s,job=%d,step=%d,task=%s,host=%s value=%"PRIu64" %"PRIu64"\n";

    static const char* DOUBLE_FMT = 
        "%s,user=%s,job=%d,step=%d,task=%s,host=%s value=%"PRIu64" %"PRIu64"\n";

    char *str = NULL;

    int i = 0;
    while(iter && iter->type != PROFILE_FIELD_NOT_SET){
        switch(iter->type){
            case PROFILE_FIELD_UINT64:
                xstrfmtcat(str, UINT64_FMT, iter->name, username, 
                    g_job->jobid, g_job->stepid, "Task", g_job->node_name, 
                    ((union data_t*) data)[i].u
                );
                break;

            case PROFILE_FIELD_DOUBLE:
                xstrfmtcat(str, DOUBLE_FMT, iter->name, username, 
                    g_job->jobid, g_job->stepid, "Task", g_job->node_name, 
                    ((union data_t*) data)[i].d
                );
                break;

            case PROFILE_FIELD_NOT_SET:
                break;
        }

        iter += 1;
        i += 1;
    }
    // ---
    debug("gather GPU info %d", dataset_id);
    // uint32_t ntasks = g_job->ntasks;
    // stepd_step_task_info_t* task = g_job->task[];
    pid_t pid = g_job->jobacct->pid;

    // ---
    xfree(str);
	return SLURM_SUCCESS;
}


/**
 * acct_gather_profile_dataset_t is hardcoded in 
 * src/plugins/jobacct_gather/common/common_jag.c:741
 *
 */
extern int acct_gather_profile_p_create_dataset(const char* name,
						int64_t parent,
						acct_gather_profile_dataset_t *dataset)
{

    // Insert our fields inside the dataset 
    // dataset = append_dataset(dataset, new_dataset_fields);
    profile_dataset = dataset;
    debug("acct_gather_profile_p_create_dataset %s", name); 


    return SLURM_SUCCESS;
}

//
acct_gather_profile_dataset_t* append_dataset(
    acct_gather_profile_dataset_t* dataset, 
    acct_gather_profile_dataset_t* new_fields)

{
    // this should only be called once
    xassert(datasset_offset == 0);
    xassert(profile_dataset == NULL);

    int current_size = 0;
    int new_fields_size = 0;

    acct_gather_profile_dataset_t* iter = dataset;
    while(iter && iter->type != PROFILE_FIELD_NOT_SET){
        current_size += 1;
        iter += 1;
    }

    iter = new_fields;
    while(iter && iter->type != PROFILE_FIELD_NOT_SET){
        new_fields_size += 1;
        iter += 1;
    }

    profile_dataset_len = (new_fields_size + current_size + 1);
    acct_gather_profile_dataset_t* merged = xmalloc(
        sizeof(acct_gather_profile_dataset_t) * profile_dataset_len);

    for(int i = 0; i < current_size; ++i){
        merged[i].name = xstrdup(dataset[i].name);
        merged[i].type = dataset[i].type;
    }

    for(int i = 0; i < new_fields_size; ++i){
        int j = i + current_size;

        merged[j].name = xstrdup(dataset[i].name);
        merged[j].type = dataset[i].type;
    }

    datasset_offset = current_size;
    profile_dataset = merged;
    return merged;
}



