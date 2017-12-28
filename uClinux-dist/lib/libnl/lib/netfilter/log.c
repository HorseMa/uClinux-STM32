/*
 * lib/netfilter/log.c	Netfilter Log
 *
 *	This library is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU Lesser General Public
 *	License as published by the Free Software Foundation version 2.1
 *	of the License.
 *
 * Copyright (c) 2003-2006 Thomas Graf <tgraf@suug.ch>
 * Copyright (c) 2007 Philip Craig <philipc@snapgear.com>
 * Copyright (c) 2007 Secure Computing Corporation
 */

/**
 * @ingroup nfnl
 * @defgroup log Log
 * @brief
 * @{
 */

#include <sys/types.h>
#include <linux/netfilter/nfnetlink_log.h>

#include <netlink-local.h>
#include <netlink/attr.h>
#include <netlink/netfilter/nfnl.h>
#include <netlink/netfilter/log.h>

/**
 * @name Log Commands
 * @{
 */

static struct nl_msg *build_log_cmd_request(uint8_t family, uint16_t queuenum,
					    uint8_t command)
{
	struct nl_msg *msg;
	struct nfulnl_msg_config_cmd cmd;

	msg = nfnlmsg_alloc_simple(NFNL_SUBSYS_ULOG, NFULNL_MSG_CONFIG, 0,
				   family, queuenum);
	if (msg == NULL)
		return NULL;

	cmd.command = command;
	if (nla_put(msg, NFULA_CFG_CMD, sizeof(cmd), &cmd) < 0)
		goto nla_put_failure;

	return msg;

nla_put_failure:
	nlmsg_free(msg);
	return NULL;
}

static int send_log_request(struct nl_handle *handle, struct nl_msg *msg)
{
	int err;

	err = nl_send_auto_complete(handle, msg);
	nlmsg_free(msg);
	if (err < 0)
		return err;

	return nl_wait_for_ack(handle);
}

struct nl_msg *nfnl_log_build_pf_bind(uint8_t pf)
{
	return build_log_cmd_request(pf, 0, NFULNL_CFG_CMD_PF_BIND);
}

int nfnl_log_pf_bind(struct nl_handle *nlh, uint8_t pf)
{
	struct nl_msg *msg;

	msg = nfnl_log_build_pf_bind(pf);
	if (!msg)
		return nl_get_errno();

	return send_log_request(nlh, msg);
}

struct nl_msg *nfnl_log_build_pf_unbind(uint8_t pf)
{
	return build_log_cmd_request(pf, 0, NFULNL_CFG_CMD_PF_UNBIND);
}

int nfnl_log_pf_unbind(struct nl_handle *nlh, uint8_t pf)
{
	struct nl_msg *msg;

	msg = nfnl_log_build_pf_unbind(pf);
	if (!msg)
		return nl_get_errno();

	return send_log_request(nlh, msg);
}

static struct nl_msg *nfnl_log_build_request(const struct nfnl_log *log)
{
	struct nl_msg *msg;

	if (!nfnl_log_test_group(log))
		return NULL;

	msg = nfnlmsg_alloc_simple(NFNL_SUBSYS_ULOG, NFULNL_MSG_CONFIG, 0,
				   0, nfnl_log_get_group(log));
	if (msg == NULL)
		return NULL;

	/* This sucks. The nfnetlink_log interface always expects both
	 * parameters to be present. Needs to be done properly.
	 */
	if (nfnl_log_test_copy_mode(log)) {
		struct nfulnl_msg_config_mode mode;

		switch (nfnl_log_get_copy_mode(log)) {
		case NFNL_LOG_COPY_NONE:
			mode.copy_mode = NFULNL_COPY_NONE;
			break;
		case NFNL_LOG_COPY_META:
			mode.copy_mode = NFULNL_COPY_META;
			break;
		case NFNL_LOG_COPY_PACKET:
			mode.copy_mode = NFULNL_COPY_PACKET;
			break;
		}
		mode.copy_range = htonl(nfnl_log_get_copy_range(log));
		mode._pad = 0;

		if (nla_put(msg, NFULA_CFG_MODE, sizeof(mode), &mode) < 0)
			goto nla_put_failure;
	}

	if (nfnl_log_test_flush_timeout(log) &&
	    nla_put_u32(msg, NFULA_CFG_TIMEOUT,
	    		htonl(nfnl_log_get_flush_timeout(log))) < 0)
		goto nla_put_failure;

	if (nfnl_log_test_alloc_size(log) &&
	    nla_put_u32(msg, NFULA_CFG_NLBUFSIZ,
	    		htonl(nfnl_log_get_alloc_size(log))) < 0)
		goto nla_put_failure;

	if (nfnl_log_test_queue_threshold(log) &&
	    nla_put_u32(msg, NFULA_CFG_QTHRESH,
	    		htonl(nfnl_log_get_queue_threshold(log))) < 0)
		goto nla_put_failure;

	return msg;

nla_put_failure:
	nlmsg_free(msg);
	return NULL;
}

struct nl_msg *nfnl_log_build_create_request(const struct nfnl_log *log)
{
	struct nl_msg *msg;
	struct nfulnl_msg_config_cmd cmd;

	msg = nfnl_log_build_request(log);
	if (msg == NULL)
		return NULL;

	cmd.command = NFULNL_CFG_CMD_BIND;

	if (nla_put(msg, NFULA_CFG_CMD, sizeof(cmd), &cmd) < 0)
		goto nla_put_failure;

	return msg;

nla_put_failure:
	nlmsg_free(msg);
	return NULL;
}

int nfnl_log_create(struct nl_handle *nlh, const struct nfnl_log *log)
{
	struct nl_msg *msg;

	msg = nfnl_log_build_create_request(log);
	if (msg == NULL)
		return nl_errno(ENOMEM);

	return send_log_request(nlh, msg);
}

struct nl_msg *nfnl_log_build_change_request(const struct nfnl_log *log)
{
	return nfnl_log_build_request(log);
}

int nfnl_log_change(struct nl_handle *nlh, const struct nfnl_log *log)
{
	struct nl_msg *msg;

	msg = nfnl_log_build_change_request(log);
	if (msg == NULL)
		return nl_errno(ENOMEM);

	return send_log_request(nlh, msg);
}

struct nl_msg *nfnl_log_build_delete_request(const struct nfnl_log *log)
{
	if (!nfnl_log_test_group(log))
		return NULL;

	return build_log_cmd_request(0, nfnl_log_get_group(log),
				     NFULNL_CFG_CMD_UNBIND);
}

int nfnl_log_delete(struct nl_handle *nlh, const struct nfnl_log *log)
{
	struct nl_msg *msg;

	msg = nfnl_log_build_delete_request(log);
	if (msg == NULL)
		return nl_errno(ENOMEM);

	return send_log_request(nlh, msg);
}

/** @} */

static struct nl_cache_ops nfnl_log_ops = {
	.co_name		= "netfilter/log",
	.co_obj_ops		= &log_obj_ops,
};

static void __init log_init(void)
{
	nl_cache_mngt_register(&nfnl_log_ops);
}

static void __exit log_exit(void)
{
	nl_cache_mngt_unregister(&nfnl_log_ops);
}

/** @} */
