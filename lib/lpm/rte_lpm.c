/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 * Copyright(c) 2020 Arm Limited
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <sys/queue.h>

#include <rte_log.h>
#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_eal_memconfig.h>
#include <rte_string_fns.h>
#include <rte_errno.h>
#include <rte_tailq.h>

#include "rte_lpm.h"

TAILQ_HEAD(rte_lpm_list, rte_tailq_entry);
//所有lpm表都tailq里
static struct rte_tailq_elem rte_lpm_tailq = {
	.name = "RTE_LPM",
};
EAL_REGISTER_TAILQ(rte_lpm_tailq)

#define MAX_DEPTH_TBL24 24

enum valid_flag {
	INVALID = 0,
	VALID
};

/** @internal Rule structure. */
struct rte_lpm_rule {
	uint32_t ip; /**< Rule IP address. */
	uint32_t next_hop; /**< Rule next hop. */
};

/** @internal Contains metadata about the rules table. */
struct rte_lpm_rule_info {
	uint32_t used_rules; /**< Used rules so far. */
	uint32_t first_rule; /**< Indexes the first rule of a given depth. */
};

/** @internal LPM structure. */ //lpm的内部结构体
struct __rte_lpm {
	/* Exposed LPM data. */
	struct rte_lpm lpm;

	/* LPM metadata. */
	char name[RTE_LPM_NAMESIZE];        /**< Name of the lpm. */
	uint32_t max_rules; /**< Max. balanced rules per lpm. */
	uint32_t number_tbl8s; /**< Number of tbl8s. */
	/**< Rule info table. */
	struct rte_lpm_rule_info rule_info[RTE_LPM_MAX_DEPTH];
	struct rte_lpm_rule *rules_tbl; /**< LPM rules. */

	/* RCU config. */
	struct rte_rcu_qsbr *v;		/* RCU QSBR variable. */
	enum rte_lpm_qsbr_mode rcu_mode;/* Blocking, defer queue. */
	struct rte_rcu_qsbr_dq *dq;	/* RCU QSBR defer queue. */
};

/* Macro to enable/disable run-time checks. */
#if defined(RTE_LIBRTE_LPM_DEBUG)
#include <rte_debug.h>
#define VERIFY_DEPTH(depth) do {                                \
	if ((depth == 0) || (depth > RTE_LPM_MAX_DEPTH))        \
		rte_panic("LPM: Invalid depth (%u) at line %d", \
				(unsigned)(depth), __LINE__);   \
} while (0)
#else
#define VERIFY_DEPTH(depth)
#endif

/*
 * Converts a given depth value to its corresponding mask value.
 *
 * depth  (IN)		: range = 1 - 32
 * mask   (OUT)		: 32bit mask
 */
static uint32_t __attribute__((pure))
depth_to_mask(uint8_t depth)
{
	VERIFY_DEPTH(depth);

	/* To calculate a mask start with a 1 on the left hand side and right
	 * shift while populating the left hand side with 1's
	 */
    //(int)0x80000000 是负数，右移最左边会补1
	return (int)0x80000000 >> (depth - 1);
}

/*
 * Converts given depth value to its corresponding range value.
 */
static uint32_t __attribute__((pure))
depth_to_range(uint8_t depth)
{
	VERIFY_DEPTH(depth);

	/*
	 * Calculate tbl24 range. (Note: 2^depth = 1 << depth)
	 */
	if (depth <= MAX_DEPTH_TBL24)
		return 1 << (MAX_DEPTH_TBL24 - depth);

	/* Else if depth is greater than 24 */
	return 1 << (RTE_LPM_MAX_DEPTH - depth);
}

/*
 * Find an existing lpm table and return a pointer to it.根据名字查找
 */
struct rte_lpm *
rte_lpm_find_existing(const char *name)
{
	struct __rte_lpm *i_lpm = NULL;
	struct rte_tailq_entry *te;
	struct rte_lpm_list *lpm_list;

	lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, lpm_list, next) {
		i_lpm = te->data;
		if (strncmp(name, i_lpm->name, RTE_LPM_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return &i_lpm->lpm;
}

/*
 * Allocates memory for LPM object 创建LPM表
 */
struct rte_lpm *
rte_lpm_create(const char *name, int socket_id,
		const struct rte_lpm_config *config)
{
	char mem_name[RTE_LPM_NAMESIZE];
	struct __rte_lpm *i_lpm; //内部实际lpm结构
	struct rte_lpm *lpm = NULL;
	struct rte_tailq_entry *te;
	uint32_t mem_size, rules_size, tbl8s_size;
	struct rte_lpm_list *lpm_list;

	lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

	RTE_BUILD_BUG_ON(sizeof(struct rte_lpm_tbl_entry) != 4);

	/* Check user arguments. */
	if ((name == NULL) || (socket_id < -1) || (config->max_rules == 0)
			|| config->number_tbl8s > RTE_LPM_MAX_TBL8_NUM_GROUPS) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "LPM_%s", name);

	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */ //根据名字查找一次，确保没重名
	TAILQ_FOREACH(te, lpm_list, next) {
		i_lpm = te->data;
		if (strncmp(name, i_lpm->name, RTE_LPM_NAMESIZE) == 0)
			break;
	}

	if (te != NULL) {
		rte_errno = EEXIST;
		goto exit;
	}

	/* Determine the amount of memory to allocate. */
	mem_size = sizeof(*i_lpm);
	rules_size = sizeof(struct rte_lpm_rule) * config->max_rules;
	tbl8s_size = sizeof(struct rte_lpm_tbl_entry) *
			RTE_LPM_TBL8_GROUP_NUM_ENTRIES * config->number_tbl8s;

	/* allocate tailq entry */
	te = rte_zmalloc("LPM_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, LPM, "Failed to allocate tailq entry\n");
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Allocate memory to store the LPM data structures. */
	i_lpm = rte_zmalloc_socket(mem_name, mem_size,
			RTE_CACHE_LINE_SIZE, socket_id);
	if (i_lpm == NULL) {
		RTE_LOG(ERR, LPM, "LPM memory allocation failed\n");
		rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}
	//rule分配内存
	i_lpm->rules_tbl = rte_zmalloc_socket(NULL,
			(size_t)rules_size, RTE_CACHE_LINE_SIZE, socket_id);

	if (i_lpm->rules_tbl == NULL) {
		RTE_LOG(ERR, LPM, "LPM rules_tbl memory allocation failed\n");
		rte_free(i_lpm);
		i_lpm = NULL;
		rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}
    //tbl8分配内存
	i_lpm->lpm.tbl8 = rte_zmalloc_socket(NULL,
			(size_t)tbl8s_size, RTE_CACHE_LINE_SIZE, socket_id);

	if (i_lpm->lpm.tbl8 == NULL) {
		RTE_LOG(ERR, LPM, "LPM tbl8 memory allocation failed\n");
		rte_free(i_lpm->rules_tbl);
		rte_free(i_lpm);
		i_lpm = NULL;
		rte_free(te);
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Save user arguments. */
	i_lpm->max_rules = config->max_rules;
	i_lpm->number_tbl8s = config->number_tbl8s;
	strlcpy(i_lpm->name, name, sizeof(i_lpm->name));

	te->data = i_lpm;
	lpm = &i_lpm->lpm;

	TAILQ_INSERT_TAIL(lpm_list, te, next);

exit:
	rte_mcfg_tailq_write_unlock();

	return lpm;
}

/*
 * Deallocates memory for given LPM table.
 */
void
rte_lpm_free(struct rte_lpm *lpm)
{
	struct rte_lpm_list *lpm_list;
	struct rte_tailq_entry *te;
	struct __rte_lpm *i_lpm;

	/* Check user arguments. */
	if (lpm == NULL)
		return;
	i_lpm = container_of(lpm, struct __rte_lpm, lpm);

	lpm_list = RTE_TAILQ_CAST(rte_lpm_tailq.head, rte_lpm_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, lpm_list, next) {
		if (te->data == (void *)i_lpm)
			break;
	}
	if (te != NULL)
		TAILQ_REMOVE(lpm_list, te, next);

	rte_mcfg_tailq_write_unlock();

	if (i_lpm->dq != NULL)
		rte_rcu_qsbr_dq_delete(i_lpm->dq);
	rte_free(i_lpm->lpm.tbl8);
	rte_free(i_lpm->rules_tbl);
	rte_free(i_lpm);
	rte_free(te);
}

static void
__lpm_rcu_qsbr_free_resource(void *p, void *data, unsigned int n)
{
	struct rte_lpm_tbl_entry *tbl8 = ((struct __rte_lpm *)p)->lpm.tbl8;
	struct rte_lpm_tbl_entry zero_tbl8_entry = {0};
	uint32_t tbl8_group_index = *(uint32_t *)data;

	RTE_SET_USED(n);
	/* Set tbl8 group invalid */
	__atomic_store(&tbl8[tbl8_group_index], &zero_tbl8_entry,
		__ATOMIC_RELAXED);
}

/* Associate QSBR variable with an LPM object.
 */
int
rte_lpm_rcu_qsbr_add(struct rte_lpm *lpm, struct rte_lpm_rcu_config *cfg)
{
	struct rte_rcu_qsbr_dq_parameters params = {0};
	char rcu_dq_name[RTE_RCU_QSBR_DQ_NAMESIZE];
	struct __rte_lpm *i_lpm;

	if (lpm == NULL || cfg == NULL) {
		rte_errno = EINVAL;
		return 1;
	}

	i_lpm = container_of(lpm, struct __rte_lpm, lpm);
	if (i_lpm->v != NULL) {
		rte_errno = EEXIST;
		return 1;
	}

	if (cfg->mode == RTE_LPM_QSBR_MODE_SYNC) {
		/* No other things to do. */
	} else if (cfg->mode == RTE_LPM_QSBR_MODE_DQ) {
		/* Init QSBR defer queue. */
		snprintf(rcu_dq_name, sizeof(rcu_dq_name),
				"LPM_RCU_%s", i_lpm->name);
		params.name = rcu_dq_name;
		params.size = cfg->dq_size;
		if (params.size == 0)
			params.size = i_lpm->number_tbl8s;
		params.trigger_reclaim_limit = cfg->reclaim_thd;
		params.max_reclaim_size = cfg->reclaim_max;
		if (params.max_reclaim_size == 0)
			params.max_reclaim_size = RTE_LPM_RCU_DQ_RECLAIM_MAX;
		params.esize = sizeof(uint32_t);	/* tbl8 group index */
		params.free_fn = __lpm_rcu_qsbr_free_resource;
		params.p = i_lpm;
		params.v = cfg->v;
		i_lpm->dq = rte_rcu_qsbr_dq_create(&params);
		if (i_lpm->dq == NULL) {
			RTE_LOG(ERR, LPM, "LPM defer queue creation failed\n");
			return 1;
		}
	} else {
		rte_errno = EINVAL;
		return 1;
	}
	i_lpm->rcu_mode = cfg->mode;
	i_lpm->v = cfg->v;

	return 0;
}

/*
 * Adds a rule to the rule table.
 *
 * NOTE: The rule table is split into 32 groups. Each group contains rules that
 * apply to a specific prefix depth (i.e. group 1 contains rules that apply to
 * prefixes with a depth of 1 etc.). In the following code (depth - 1) is used
 * to refer to depth 1 because even though the depth range is 1 - 32, depths
 * are stored in the rule table from 0 - 31.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static int32_t
rule_add(struct __rte_lpm *i_lpm, uint32_t ip_masked, uint8_t depth,
	uint32_t next_hop)
{
	uint32_t rule_gindex, rule_index, last_rule;
	int i;

	VERIFY_DEPTH(depth);

	/* Scan through rule group to see if rule already exists. */
    //检测规则是否已经存在
	if (i_lpm->rule_info[depth - 1].used_rules > 0) {

		/* rule_gindex stands for rule group index. */
        //该组第一个索引位置
		rule_gindex = i_lpm->rule_info[depth - 1].first_rule;
		/* Initialise rule_index to point to start of rule group. */
		rule_index = rule_gindex;
		/* Last rule = Last used rule in this rule group. */
        //该组最后一个索引的后一位置
		last_rule = rule_gindex + i_lpm->rule_info[depth - 1].used_rules;

		for (; rule_index < last_rule; rule_index++) {
            //开始遍历，检测是否已经存在
			/* If rule already exists update next hop and return. */
			if (i_lpm->rules_tbl[rule_index].ip == ip_masked) {

				if (i_lpm->rules_tbl[rule_index].next_hop
						== next_hop)
					return -EEXIST;
				i_lpm->rules_tbl[rule_index].next_hop = next_hop;

				return rule_index;
			}
		}
        //不存在，检测新的位置是否溢出
		if (rule_index == i_lpm->max_rules)
			return -ENOSPC;
	} else {
        //要插入的组无任何记录
		/* Calculate the position in which the rule will be stored. */
		rule_index = 0;
        //从该组开始，往前查找被使用组的下一个位置，作为新的插入位置
		for (i = depth - 1; i > 0; i--) {
			if (i_lpm->rule_info[i - 1].used_rules > 0) {
				rule_index = i_lpm->rule_info[i - 1].first_rule
						+ i_lpm->rule_info[i - 1].used_rules;
				break;
			}
		}
		if (rule_index == i_lpm->max_rules)
			return -ENOSPC;
        //记录该组第一个rule-table的索引
		i_lpm->rule_info[depth - 1].first_rule = rule_index;
	}
    /*
     * 没有检测要插入的位置是否被下一组占用，
     * 而是将后续全部后移一个位置
     */
	/* Make room for the new rule in the array. */
    //从后一组开始，依次往前移动一位
	for (i = RTE_LPM_MAX_DEPTH; i > depth; i--) {
		if (i_lpm->rule_info[i - 1].first_rule
				+ i_lpm->rule_info[i - 1].used_rules == i_lpm->max_rules)
			return -ENOSPC;

		if (i_lpm->rule_info[i - 1].used_rules > 0) {
		//后续某组有数组，直接将该组第一个元素移动到最后去
			i_lpm->rules_tbl[i_lpm->rule_info[i - 1].first_rule
				+ i_lpm->rule_info[i - 1].used_rules]
					= i_lpm->rules_tbl[i_lpm->rule_info[i - 1].first_rule];
            //移动完后，更新移动组的起始位置
			i_lpm->rule_info[i - 1].first_rule++;
		}
	}
    //插入新的rule
	/* Add the new rule. */
	i_lpm->rules_tbl[rule_index].ip = ip_masked;
	i_lpm->rules_tbl[rule_index].next_hop = next_hop;

	/* Increment the used rules counter for this rule group. */
	i_lpm->rule_info[depth - 1].used_rules++;

	return rule_index;
}

/*
 * Delete a rule from the rule table.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static void
rule_delete(struct __rte_lpm *i_lpm, int32_t rule_index, uint8_t depth)
{
	int i;

	VERIFY_DEPTH(depth);
    //该组最后一个元素赋值到要删除的位置上
	i_lpm->rules_tbl[rule_index] =
			i_lpm->rules_tbl[i_lpm->rule_info[depth - 1].first_rule
			+ i_lpm->rule_info[depth - 1].used_rules - 1];
    //将后续所有组的记录往前移动一位
	for (i = depth; i < RTE_LPM_MAX_DEPTH; i++) {
		if (i_lpm->rule_info[i].used_rules > 0) {
			i_lpm->rules_tbl[i_lpm->rule_info[i].first_rule - 1] =
					i_lpm->rules_tbl[i_lpm->rule_info[i].first_rule
						+ i_lpm->rule_info[i].used_rules - 1];
			i_lpm->rule_info[i].first_rule--;
		}
	}
    //更新该组记录数
	i_lpm->rule_info[depth - 1].used_rules--;
}

/*
 * Finds a rule in rule table.
 * NOTE: Valid range for depth parameter is 1 .. 32 inclusive.
 */
static int32_t
rule_find(struct __rte_lpm *i_lpm, uint32_t ip_masked, uint8_t depth)
{
	uint32_t rule_gindex, last_rule, rule_index;

	VERIFY_DEPTH(depth);

	rule_gindex = i_lpm->rule_info[depth - 1].first_rule;
	last_rule = rule_gindex + i_lpm->rule_info[depth - 1].used_rules;

	/* Scan used rules at given depth to find rule. */
	for (rule_index = rule_gindex; rule_index < last_rule; rule_index++) {
		/* If rule is found return the rule index. */
		if (i_lpm->rules_tbl[rule_index].ip == ip_masked)
			return rule_index;
	}

	/* If rule is not found return -EINVAL. */
	return -EINVAL;
}

/*
 * Find, clean and allocate a tbl8.
 */
static int32_t
_tbl8_alloc(struct __rte_lpm *i_lpm)
{
	uint32_t group_idx; /* tbl8 group index. */
	struct rte_lpm_tbl_entry *tbl8_entry;

	/* Scan through tbl8 to find a free (i.e. INVALID) tbl8 group. */
	for (group_idx = 0; group_idx < i_lpm->number_tbl8s; group_idx++) {
		tbl8_entry = &i_lpm->lpm.tbl8[group_idx *
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES];
		/* If a free tbl8 group is found clean it and set as VALID. */
		if (!tbl8_entry->valid_group) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.next_hop = 0,
				.valid = INVALID,
				.depth = 0,
				.valid_group = VALID,
			};

			memset(&tbl8_entry[0], 0,
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES *
					sizeof(tbl8_entry[0]));

			__atomic_store(tbl8_entry, &new_tbl8_entry,
					__ATOMIC_RELAXED);

			/* Return group index for allocated tbl8 group. */
			return group_idx;
		}
	}

	/* If there are no tbl8 groups free then return error. */
	return -ENOSPC;
}

static int32_t
tbl8_alloc(struct __rte_lpm *i_lpm)
{
	int32_t group_idx; /* tbl8 group index. */

	group_idx = _tbl8_alloc(i_lpm);
	if (group_idx == -ENOSPC && i_lpm->dq != NULL) {
		/* If there are no tbl8 groups try to reclaim one. */
		if (rte_rcu_qsbr_dq_reclaim(i_lpm->dq, 1,
				NULL, NULL, NULL) == 0)
			group_idx = _tbl8_alloc(i_lpm);
	}

	return group_idx;
}

static int32_t
tbl8_free(struct __rte_lpm *i_lpm, uint32_t tbl8_group_start)
{
	struct rte_lpm_tbl_entry zero_tbl8_entry = {0};
	int status;

	if (i_lpm->v == NULL) {
		/* Set tbl8 group invalid*/
		__atomic_store(&i_lpm->lpm.tbl8[tbl8_group_start], &zero_tbl8_entry,
				__ATOMIC_RELAXED);
	} else if (i_lpm->rcu_mode == RTE_LPM_QSBR_MODE_SYNC) {
		/* Wait for quiescent state change. */
		rte_rcu_qsbr_synchronize(i_lpm->v,
			RTE_QSBR_THRID_INVALID);
		/* Set tbl8 group invalid*/
		__atomic_store(&i_lpm->lpm.tbl8[tbl8_group_start], &zero_tbl8_entry,
				__ATOMIC_RELAXED);
	} else if (i_lpm->rcu_mode == RTE_LPM_QSBR_MODE_DQ) {
		/* Push into QSBR defer queue. */
		status = rte_rcu_qsbr_dq_enqueue(i_lpm->dq,
				(void *)&tbl8_group_start);
		if (status == 1) {
			RTE_LOG(ERR, LPM, "Failed to push QSBR FIFO\n");
			return -rte_errno;
		}
	}

	return 0;
}

static __rte_noinline int32_t
add_depth_small(struct __rte_lpm *i_lpm, uint32_t ip, uint8_t depth,
		uint32_t next_hop)
{
#define group_idx next_hop
	uint32_t tbl24_index, tbl24_range, tbl8_index, tbl8_group_end, i, j;

	/* Calculate the index into Table24. */
	tbl24_index = ip >> 8; //取高3字节
	tbl24_range = depth_to_range(depth); //24bit地址对应的子网地址空间
	/*-------------------------
     *例如：添加192.168.0.0/16这个条路由
	 *192.168.0.0 右移动8bit 变成192.168.0
	 * rang = 1<<(24-16),即0xff
     * IP掩码为16，切前16bt在192.168.0--192.168.ff范围内时，都会命中该路由
	 * 需要将路由添加到192.168.0--192.168.ff对应的地址空间
	 */
	for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {
		/*
		 * For invalid OR valid and non-extended tbl 24 entries set
		 * entry.
		 */
        //如果条目无效，就添加
        //如果条目被占用，在没有tbl8表并且子网长度比当前小时，则更新条目
		//如果有tbl8，后续处理
        //如果之前的掩码长度比目前大，则不更新
		if (!i_lpm->lpm.tbl24[i].valid || (i_lpm->lpm.tbl24[i].valid_group == 0 &&
				i_lpm->lpm.tbl24[i].depth <= depth)) {

			struct rte_lpm_tbl_entry new_tbl24_entry = {
				.next_hop = next_hop,
				.valid = VALID,
				.valid_group = 0,
				.depth = depth,
			};

			/* Setting tbl24 entry in one go to avoid race
			 * conditions
			 */
			__atomic_store(&i_lpm->lpm.tbl24[i], &new_tbl24_entry,
					__ATOMIC_RELEASE);

			continue;
		}
		//有tbl8表，说明当前子网掩码长度比之前的小
        //只有掩码长度超过24才会用到tbl8
        //实参进来的长度是超过24的，我认为下面这段应该没用
		if (i_lpm->lpm.tbl24[i].valid_group == 1) {
			/* If tbl24 entry is valid and extended calculate the
			 *  index into tbl8.
			 */
			tbl8_index = i_lpm->lpm.tbl24[i].group_idx *
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
			tbl8_group_end = tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

			for (j = tbl8_index; j < tbl8_group_end; j++) {
				if (!i_lpm->lpm.tbl8[j].valid ||
						i_lpm->lpm.tbl8[j].depth <= depth) {
					struct rte_lpm_tbl_entry
						new_tbl8_entry = {
						.valid = VALID,
						.valid_group = VALID,
						.depth = depth,
						.next_hop = next_hop,
					};

					/*
					 * Setting tbl8 entry in one go to avoid
					 * race conditions
					 */
					__atomic_store(&i_lpm->lpm.tbl8[j],
						&new_tbl8_entry,
						__ATOMIC_RELAXED);

					continue;
				}
			}
		}
	}
#undef group_idx
	return 0;
}

static __rte_noinline int32_t
add_depth_big(struct __rte_lpm *i_lpm, uint32_t ip_masked, uint8_t depth,
		uint32_t next_hop)
{
#define group_idx next_hop
	uint32_t tbl24_index;
	int32_t tbl8_group_index, tbl8_group_start, tbl8_group_end, tbl8_index,
		tbl8_range, i;

	tbl24_index = (ip_masked >> 8);
	tbl8_range = depth_to_range(depth); //tbl8 地址空间范围
	//由于子网掩码长度大于24，就一表项
	if (!i_lpm->lpm.tbl24[tbl24_index].valid) { //表项未被占用
		/* Search for a free tbl8 group. */
		tbl8_group_index = tbl8_alloc(i_lpm);

		/* Check tbl8 allocation was successful. */
		if (tbl8_group_index < 0) {
			return tbl8_group_index;
		}

		/* Find index into tbl8 and range. */
		tbl8_index = (tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES) +
				(ip_masked & 0xFF);
		/*
         * 例如：192.168.1.128/25
         * range范围是2^7,即0x00--0x7f
		 * ip_masked & 0xff = 128
         * tbl8_index = (tbl8_group_index * RTE_LPM_TBL8_GROUP_NUM_ENTRIES) + 128
         */
		/* Set tbl8 entry. */ //刚分配的直接赋值
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = depth,
				.valid_group = i_lpm->lpm.tbl8[i].valid_group,
				.next_hop = next_hop,
			};
			__atomic_store(&i_lpm->lpm.tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		/*
		 * Update tbl24 entry to point to new tbl8 entry. Note: The
		 * ext_flag and tbl8_index need to be updated simultaneously,
		 * so assign whole structure in one go
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.group_idx = tbl8_group_index,
			.valid = VALID,
			.valid_group = 1,
			.depth = 0,
		};

		/* The tbl24 entry must be written only after the
		 * tbl8 entries are written.
		 */
		__atomic_store(&i_lpm->lpm.tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELEASE);

	} /* If valid entry but not extended calculate the index into Table8. */
	else if (i_lpm->lpm.tbl24[tbl24_index].valid_group == 0) {
		//表项被占用，但是tbl8表
        //申请tbl8表，并赋值，更新tbl24表项
		/* Search for free tbl8 group. */
		tbl8_group_index = tbl8_alloc(i_lpm);

		if (tbl8_group_index < 0) {
			return tbl8_group_index;
		}

		tbl8_group_start = tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl8_group_end = tbl8_group_start +
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

		/* Populate new tbl8 with tbl24 value. */
		for (i = tbl8_group_start; i < tbl8_group_end; i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = i_lpm->lpm.tbl24[tbl24_index].depth,
				.valid_group = i_lpm->lpm.tbl8[i].valid_group,
				.next_hop = i_lpm->lpm.tbl24[tbl24_index].next_hop,
			};
			__atomic_store(&i_lpm->lpm.tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		tbl8_index = tbl8_group_start + (ip_masked & 0xFF);

		/* Insert new rule into the tbl8 entry. */
		for (i = tbl8_index; i < tbl8_index + tbl8_range; i++) {
			struct rte_lpm_tbl_entry new_tbl8_entry = {
				.valid = VALID,
				.depth = depth,
				.valid_group = i_lpm->lpm.tbl8[i].valid_group,
				.next_hop = next_hop,
			};
			__atomic_store(&i_lpm->lpm.tbl8[i], &new_tbl8_entry,
					__ATOMIC_RELAXED);
		}

		/*
		 * Update tbl24 entry to point to new tbl8 entry. Note: The
		 * ext_flag and tbl8_index need to be updated simultaneously,
		 * so assign whole structure in one go.
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
				.group_idx = tbl8_group_index,
				.valid = VALID,
				.valid_group = 1,
				.depth = 0,
		};

		/* The tbl24 entry must be written only after the
		 * tbl8 entries are written.
		 */
		__atomic_store(&i_lpm->lpm.tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELEASE);

	} else { /*
		* If it is valid, extended entry calculate the index into tbl8.
		*/
		//tbl24表项被占用，并且有tbl24
        //直接查找表项，按子网掩码长度决定是否更新表项
		tbl8_group_index = i_lpm->lpm.tbl24[tbl24_index].group_idx;
		tbl8_group_start = tbl8_group_index *
				RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl8_index = tbl8_group_start + (ip_masked & 0xFF);

		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {

			if (!i_lpm->lpm.tbl8[i].valid ||
					i_lpm->lpm.tbl8[i].depth <= depth) {
				struct rte_lpm_tbl_entry new_tbl8_entry = {
					.valid = VALID,
					.depth = depth,
					.next_hop = next_hop,
					.valid_group = i_lpm->lpm.tbl8[i].valid_group,
				};

				/*
				 * Setting tbl8 entry in one go to avoid race
				 * condition
				 */
				__atomic_store(&i_lpm->lpm.tbl8[i], &new_tbl8_entry,
						__ATOMIC_RELAXED);

				continue;
			}
		}
	}
#undef group_idx
	return 0;
}

/*
 * Add a route，先将信息存到rte_lpm_rule里，再存到rte_lpm_tbl_entry里
 */
int
rte_lpm_add(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
		uint32_t next_hop)
{
	int32_t rule_index, status = 0;
	struct __rte_lpm *i_lpm;
	uint32_t ip_masked;

	/* Check user arguments. */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM_MAX_DEPTH))
		return -EINVAL;
    //找到__rte_lpm起始位置
	i_lpm = container_of(lpm, struct __rte_lpm, lpm);
    //计算与掩码运算后的IP数值
	ip_masked = ip & depth_to_mask(depth);

	/* Add the rule to the rule table. *///加到struct rte_lpm_rule *rules_tbl里
	rule_index = rule_add(i_lpm, ip_masked, depth, next_hop);

	/* Skip table entries update if The rule is the same as
	 * the rule in the rules table.
	 */
	if (rule_index == -EEXIST) //已存在，更新next_hop返回
		return 0;

	/* If the is no space available for new rule return error. */
	if (rule_index < 0) { //无空间，返回
		return rule_index;
	}
    // 添加到rte_lpm_tbl_entry里
	if (depth <= MAX_DEPTH_TBL24) {
		status = add_depth_small(i_lpm, ip_masked, depth, next_hop);
	} else { /* If depth > RTE_LPM_MAX_DEPTH_TBL24 *///掩码长度是(24,32]
		status = add_depth_big(i_lpm, ip_masked, depth, next_hop);

		/*
		 * If add fails due to exhaustion of tbl8 extensions delete
		 * rule that was added to rule table.
		 */
		if (status < 0) {
			rule_delete(i_lpm, rule_index, depth);

			return status;
		}
	}

	return 0;
}

/*
 * Look for a rule in the high-level rules table
 */
int
rte_lpm_is_rule_present(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
uint32_t *next_hop)
{
	struct __rte_lpm *i_lpm;
	uint32_t ip_masked;
	int32_t rule_index;

	/* Check user arguments. */
	if ((lpm == NULL) ||
		(next_hop == NULL) ||
		(depth < 1) || (depth > RTE_LPM_MAX_DEPTH))
		return -EINVAL;

	/* Look for the rule using rule_find. */
	i_lpm = container_of(lpm, struct __rte_lpm, lpm);
	ip_masked = ip & depth_to_mask(depth);
	rule_index = rule_find(i_lpm, ip_masked, depth);

	if (rule_index >= 0) {
		*next_hop = i_lpm->rules_tbl[rule_index].next_hop;
		return 1;
	}

	/* If rule is not found return 0. */
	return 0;
}

static int32_t
find_previous_rule(struct __rte_lpm *i_lpm, uint32_t ip, uint8_t depth,
		uint8_t *sub_rule_depth)
{
	int32_t rule_index;
	uint32_t ip_masked;
	uint8_t prev_depth;

	for (prev_depth = (uint8_t)(depth - 1); prev_depth > 0; prev_depth--) {
		ip_masked = ip & depth_to_mask(prev_depth);

		rule_index = rule_find(i_lpm, ip_masked, prev_depth);

		if (rule_index >= 0) {
			*sub_rule_depth = prev_depth;
			return rule_index;
		}
	}

	return -1;
}

static int32_t
delete_depth_small(struct __rte_lpm *i_lpm, uint32_t ip_masked,
	uint8_t depth, int32_t sub_rule_index, uint8_t sub_rule_depth)
{
#define group_idx next_hop
	uint32_t tbl24_range, tbl24_index, tbl8_group_index, tbl8_index, i, j;

	/* Calculate the range and index into Table24. */
	tbl24_range = depth_to_range(depth);
	tbl24_index = (ip_masked >> 8);
	struct rte_lpm_tbl_entry zero_tbl24_entry = {0};

	/*
	 * Firstly check the sub_rule_index. A -1 indicates no replacement rule
	 * and a positive number indicates a sub_rule_index.
	 */
	if (sub_rule_index < 0) {
		/*
		 * If no replacement rule exists then invalidate entries
		 * associated with this rule.
		 */
		for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {

			if (i_lpm->lpm.tbl24[i].valid_group == 0 &&
					i_lpm->lpm.tbl24[i].depth <= depth) {
				__atomic_store(&i_lpm->lpm.tbl24[i],
					&zero_tbl24_entry, __ATOMIC_RELEASE);
			} else if (i_lpm->lpm.tbl24[i].valid_group == 1) {
				/*
				 * If TBL24 entry is extended, then there has
				 * to be a rule with depth >= 25 in the
				 * associated TBL8 group.
				 */

				tbl8_group_index = i_lpm->lpm.tbl24[i].group_idx;
				tbl8_index = tbl8_group_index *
						RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

				for (j = tbl8_index; j < (tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES); j++) {

					if (i_lpm->lpm.tbl8[j].depth <= depth)
						i_lpm->lpm.tbl8[j].valid = INVALID;
				}
			}
		}
	} else {
		/*
		 * If a replacement rule exists then modify entries
		 * associated with this rule.
		 */

		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.next_hop = i_lpm->rules_tbl[sub_rule_index].next_hop,
			.valid = VALID,
			.valid_group = 0,
			.depth = sub_rule_depth,
		};

		struct rte_lpm_tbl_entry new_tbl8_entry = {
			.valid = VALID,
			.valid_group = VALID,
			.depth = sub_rule_depth,
			.next_hop = i_lpm->rules_tbl
			[sub_rule_index].next_hop,
		};

		for (i = tbl24_index; i < (tbl24_index + tbl24_range); i++) {

			if (i_lpm->lpm.tbl24[i].valid_group == 0 &&
					i_lpm->lpm.tbl24[i].depth <= depth) {
				__atomic_store(&i_lpm->lpm.tbl24[i], &new_tbl24_entry,
						__ATOMIC_RELEASE);
			} else  if (i_lpm->lpm.tbl24[i].valid_group == 1) {
				/*
				 * If TBL24 entry is extended, then there has
				 * to be a rule with depth >= 25 in the
				 * associated TBL8 group.
				 */

				tbl8_group_index = i_lpm->lpm.tbl24[i].group_idx;
				tbl8_index = tbl8_group_index *
						RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

				for (j = tbl8_index; j < (tbl8_index +
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES); j++) {

					if (i_lpm->lpm.tbl8[j].depth <= depth)
						__atomic_store(&i_lpm->lpm.tbl8[j],
							&new_tbl8_entry,
							__ATOMIC_RELAXED);
				}
			}
		}
	}
#undef group_idx
	return 0;
}

/*
 * Checks if table 8 group can be recycled.
 *
 * Return of -EEXIST means tbl8 is in use and thus can not be recycled.
 * Return of -EINVAL means tbl8 is empty and thus can be recycled
 * Return of value > -1 means tbl8 is in use but has all the same values and
 * thus can be recycled
 */
static int32_t
tbl8_recycle_check(struct rte_lpm_tbl_entry *tbl8,
		uint32_t tbl8_group_start)
{
	uint32_t tbl8_group_end, i;
	tbl8_group_end = tbl8_group_start + RTE_LPM_TBL8_GROUP_NUM_ENTRIES;

	/*
	 * Check the first entry of the given tbl8. If it is invalid we know
	 * this tbl8 does not contain any rule with a depth < RTE_LPM_MAX_DEPTH
	 *  (As they would affect all entries in a tbl8) and thus this table
	 *  can not be recycled.
	 */
	if (tbl8[tbl8_group_start].valid) {
		/*
		 * If first entry is valid check if the depth is less than 24
		 * and if so check the rest of the entries to verify that they
		 * are all of this depth.
		 */
		if (tbl8[tbl8_group_start].depth <= MAX_DEPTH_TBL24) {
			for (i = (tbl8_group_start + 1); i < tbl8_group_end;
					i++) {

				if (tbl8[i].depth !=
						tbl8[tbl8_group_start].depth) {

					return -EEXIST;
				}
			}
			/* If all entries are the same return the tb8 index */
			return tbl8_group_start;
		}

		return -EEXIST;
	}
	/*
	 * If the first entry is invalid check if the rest of the entries in
	 * the tbl8 are invalid.
	 */
	for (i = (tbl8_group_start + 1); i < tbl8_group_end; i++) {
		if (tbl8[i].valid)
			return -EEXIST;
	}
	/* If no valid entries are found then return -EINVAL. */
	return -EINVAL;
}

static int32_t
delete_depth_big(struct __rte_lpm *i_lpm, uint32_t ip_masked,
	uint8_t depth, int32_t sub_rule_index, uint8_t sub_rule_depth)
{
#define group_idx next_hop
	uint32_t tbl24_index, tbl8_group_index, tbl8_group_start, tbl8_index,
			tbl8_range, i;
	int32_t tbl8_recycle_index, status = 0;

	/*
	 * Calculate the index into tbl24 and range. Note: All depths larger
	 * than MAX_DEPTH_TBL24 are associated with only one tbl24 entry.
	 */
	tbl24_index = ip_masked >> 8;

	/* Calculate the index into tbl8 and range. */
	tbl8_group_index = i_lpm->lpm.tbl24[tbl24_index].group_idx;
	tbl8_group_start = tbl8_group_index * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
	tbl8_index = tbl8_group_start + (ip_masked & 0xFF);
	tbl8_range = depth_to_range(depth);

	if (sub_rule_index < 0) {
		/*
		 * Loop through the range of entries on tbl8 for which the
		 * rule_to_delete must be removed or modified.
		 */
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			if (i_lpm->lpm.tbl8[i].depth <= depth)
				i_lpm->lpm.tbl8[i].valid = INVALID;
		}
	} else {
		/* Set new tbl8 entry. */
		struct rte_lpm_tbl_entry new_tbl8_entry = {
			.valid = VALID,
			.depth = sub_rule_depth,
			.valid_group = i_lpm->lpm.tbl8[tbl8_group_start].valid_group,
			.next_hop = i_lpm->rules_tbl[sub_rule_index].next_hop,
		};

		/*
		 * Loop through the range of entries on tbl8 for which the
		 * rule_to_delete must be modified.
		 */
		for (i = tbl8_index; i < (tbl8_index + tbl8_range); i++) {
			if (i_lpm->lpm.tbl8[i].depth <= depth)
				__atomic_store(&i_lpm->lpm.tbl8[i], &new_tbl8_entry,
						__ATOMIC_RELAXED);
		}
	}

	/*
	 * Check if there are any valid entries in this tbl8 group. If all
	 * tbl8 entries are invalid we can free the tbl8 and invalidate the
	 * associated tbl24 entry.
	 */

	tbl8_recycle_index = tbl8_recycle_check(i_lpm->lpm.tbl8, tbl8_group_start);

	if (tbl8_recycle_index == -EINVAL) {
		/* Set tbl24 before freeing tbl8 to avoid race condition.
		 * Prevent the free of the tbl8 group from hoisting.
		 */
		i_lpm->lpm.tbl24[tbl24_index].valid = 0;
		__atomic_thread_fence(__ATOMIC_RELEASE);
		status = tbl8_free(i_lpm, tbl8_group_start);
	} else if (tbl8_recycle_index > -1) {
		/* Update tbl24 entry. */
		struct rte_lpm_tbl_entry new_tbl24_entry = {
			.next_hop = i_lpm->lpm.tbl8[tbl8_recycle_index].next_hop,
			.valid = VALID,
			.valid_group = 0,
			.depth = i_lpm->lpm.tbl8[tbl8_recycle_index].depth,
		};

		/* Set tbl24 before freeing tbl8 to avoid race condition.
		 * Prevent the free of the tbl8 group from hoisting.
		 */
		__atomic_store(&i_lpm->lpm.tbl24[tbl24_index], &new_tbl24_entry,
				__ATOMIC_RELAXED);
		__atomic_thread_fence(__ATOMIC_RELEASE);
		status = tbl8_free(i_lpm, tbl8_group_start);
	}
#undef group_idx
	return status;
}

/*
 * Deletes a rule
 */
int
rte_lpm_delete(struct rte_lpm *lpm, uint32_t ip, uint8_t depth)
{
	int32_t rule_to_delete_index, sub_rule_index;
	struct __rte_lpm *i_lpm;
	uint32_t ip_masked;
	uint8_t sub_rule_depth;
	/*
	 * Check input arguments. Note: IP must be a positive integer of 32
	 * bits in length therefore it need not be checked.
	 */
	if ((lpm == NULL) || (depth < 1) || (depth > RTE_LPM_MAX_DEPTH)) {
		return -EINVAL;
	}

	i_lpm = container_of(lpm, struct __rte_lpm, lpm);
	ip_masked = ip & depth_to_mask(depth);

	/*
	 * Find the index of the input rule, that needs to be deleted, in the
	 * rule table.
	 */
	rule_to_delete_index = rule_find(i_lpm, ip_masked, depth);

	/*
	 * Check if rule_to_delete_index was found. If no rule was found the
	 * function rule_find returns -EINVAL.
	 */
	if (rule_to_delete_index < 0)
		return -EINVAL;

	/* Delete the rule from the rule table. */
	rule_delete(i_lpm, rule_to_delete_index, depth);

	/*
	 * Find rule to replace the rule_to_delete. If there is no rule to
	 * replace the rule_to_delete we return -1 and invalidate the table
	 * entries associated with this rule.
	 */
	sub_rule_depth = 0;
	sub_rule_index = find_previous_rule(i_lpm, ip, depth, &sub_rule_depth);

	/*
	 * If the input depth value is less than 25 use function
	 * delete_depth_small otherwise use delete_depth_big.
	 */
	if (depth <= MAX_DEPTH_TBL24) {
		return delete_depth_small(i_lpm, ip_masked, depth,
				sub_rule_index, sub_rule_depth);
	} else { /* If depth > MAX_DEPTH_TBL24 */
		return delete_depth_big(i_lpm, ip_masked, depth, sub_rule_index,
				sub_rule_depth);
	}
}

/*
 * Delete all rules from the LPM table.
 */
void
rte_lpm_delete_all(struct rte_lpm *lpm)
{
	struct __rte_lpm *i_lpm;

	i_lpm = container_of(lpm, struct __rte_lpm, lpm);
	/* Zero rule information. */
	memset(i_lpm->rule_info, 0, sizeof(i_lpm->rule_info));

	/* Zero tbl24. */
	memset(i_lpm->lpm.tbl24, 0, sizeof(i_lpm->lpm.tbl24));

	/* Zero tbl8. */
	memset(i_lpm->lpm.tbl8, 0, sizeof(i_lpm->lpm.tbl8[0])
			* RTE_LPM_TBL8_GROUP_NUM_ENTRIES * i_lpm->number_tbl8s);

	/* Delete all rules form the rules table. */
	memset(i_lpm->rules_tbl, 0, sizeof(i_lpm->rules_tbl[0]) * i_lpm->max_rules);
}
