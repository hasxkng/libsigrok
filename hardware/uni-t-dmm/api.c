/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2012-2013 Uwe Hermann <uwe@hermann-uwe.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"
#include "protocol.h"

#define UNI_T_UT_D04_NEW "1a86.e008"

static const int32_t hwopts[] = {
	SR_CONF_CONN,
};

static const int32_t hwcaps[] = {
	SR_CONF_MULTIMETER,
	SR_CONF_LIMIT_SAMPLES,
	SR_CONF_LIMIT_MSEC,
	SR_CONF_CONTINUOUS,
};

SR_PRIV struct sr_dev_driver tecpel_dmm_8060_driver_info;
SR_PRIV struct sr_dev_driver tecpel_dmm_8061_driver_info;
SR_PRIV struct sr_dev_driver uni_t_ut61d_driver_info;
SR_PRIV struct sr_dev_driver uni_t_ut61e_driver_info;
SR_PRIV struct sr_dev_driver voltcraft_vc820_driver_info;
SR_PRIV struct sr_dev_driver voltcraft_vc840_driver_info;

SR_PRIV struct dmm_info udmms[] = {
	{
		"Tecpel", "DMM-8060", 2400,
		FS9721_PACKET_SIZE, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		NULL,
		&tecpel_dmm_8060_driver_info, receive_data_TECPEL_DMM_8060,
	},
	{
		"Tecpel", "DMM-8061", 2400,
		FS9721_PACKET_SIZE, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c,
		&tecpel_dmm_8061_driver_info, receive_data_TECPEL_DMM_8061,
	},
	{
		"UNI-T", "UT61D", 2400,
		FS9922_PACKET_SIZE, NULL,
		sr_fs9922_packet_valid, sr_fs9922_parse,
		NULL,
		&uni_t_ut61d_driver_info, receive_data_UNI_T_UT61D,
	},
	{
		"UNI-T", "UT61E", 19230,
		ES51922_PACKET_SIZE, NULL,
		sr_es51922_packet_valid, sr_es51922_parse,
		NULL,
		&uni_t_ut61e_driver_info, receive_data_UNI_T_UT61E,
	},
	{
		"Voltcraft", "VC-820", 2400,
		FS9721_PACKET_SIZE, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		NULL,
		&voltcraft_vc820_driver_info, receive_data_VOLTCRAFT_VC820,
	},
	{
		"Voltcraft", "VC-840", 2400,
		FS9721_PACKET_SIZE, NULL,
		sr_fs9721_packet_valid, sr_fs9721_parse,
		sr_fs9721_00_temp_c,
		&voltcraft_vc840_driver_info, receive_data_VOLTCRAFT_VC840,
	},
};

static int clear_instances(int dmm)
{
	(void)dmm;

	/* TODO: Use common code later. */

	return SR_OK;
}

static int hw_init(struct sr_context *sr_ctx, int dmm)
{
	sr_dbg("Selected '%s' subdriver.", udmms[dmm].di->name);

	return std_hw_init(sr_ctx, udmms[dmm].di, LOG_PREFIX);
}

static GSList *hw_scan(GSList *options, int dmm)
{
	GSList *usb_devices, *devices, *l;
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct drv_context *drvc;
	struct sr_usb_dev_inst *usb;
	struct sr_config *src;
	struct sr_probe *probe;
	const char *conn;

	drvc = udmms[dmm].di->priv;

	conn = NULL;
	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
			conn = g_variant_get_string(src->data, NULL);
			break;
		}
	}
	if (!conn)
		conn = UNI_T_UT_D04_NEW;

	devices = NULL;
	if (!(usb_devices = sr_usb_find(drvc->sr_ctx->libusb_ctx, conn))) {
		g_slist_free_full(usb_devices, g_free);
		return NULL;
	}

	for (l = usb_devices; l; l = l->next) {
		usb = l->data;

		if (!(devc = g_try_malloc0(sizeof(struct dev_context)))) {
			sr_err("Device context malloc failed.");
			return NULL;
		}

		devc->first_run = TRUE;

		if (!(sdi = sr_dev_inst_new(0, SR_ST_INACTIVE,
				udmms[dmm].vendor, udmms[dmm].device, NULL))) {
			sr_err("sr_dev_inst_new returned NULL.");
			return NULL;
		}
		sdi->priv = devc;
		sdi->driver = udmms[dmm].di;
		if (!(probe = sr_probe_new(0, SR_PROBE_ANALOG, TRUE, "P1")))
			return NULL;
		sdi->probes = g_slist_append(sdi->probes, probe);

		sdi->inst_type = SR_INST_USB;
		sdi->conn = usb;

		drvc->instances = g_slist_append(drvc->instances, sdi);
		devices = g_slist_append(devices, sdi);
	}

	return devices;
}

static GSList *hw_dev_list(int dmm)
{
	return ((struct drv_context *)(udmms[dmm].di->priv))->instances;
}

static int hw_dev_open(struct sr_dev_inst *sdi, int dmm)
{
	struct drv_context *drvc;
	struct sr_usb_dev_inst *usb;
	int ret;

	drvc = udmms[dmm].di->priv;
	usb = sdi->conn;

	if ((ret = sr_usb_open(drvc->sr_ctx->libusb_ctx, usb)) == SR_OK)
		sdi->status = SR_ST_ACTIVE;

	return ret;
}

static int hw_dev_close(struct sr_dev_inst *sdi)
{
	(void)sdi;

	/* TODO */

	sdi->status = SR_ST_INACTIVE;

	return SR_OK;
}

static int hw_cleanup(int dmm)
{
	clear_instances(dmm);

	return SR_OK;
}

static int config_set(int id, GVariant *data, const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;

	devc = sdi->priv;

	switch (id) {
	case SR_CONF_LIMIT_MSEC:
		if (g_variant_get_uint64(data) == 0) {
			sr_err("Time limit cannot be 0.");
			return SR_ERR;
		}
		devc->limit_msec = g_variant_get_uint64(data);
		sr_dbg("Setting time limit to %" PRIu64 "ms.",
		       devc->limit_msec);
		break;
	case SR_CONF_LIMIT_SAMPLES:
		if (g_variant_get_uint64(data) == 0) {
			sr_err("Sample limit cannot be 0.");
			return SR_ERR;
		}
		devc->limit_samples = g_variant_get_uint64(data);
		sr_dbg("Setting sample limit to %" PRIu64 ".",
		       devc->limit_samples);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(int key, GVariant **data, const struct sr_dev_inst *sdi)
{
	(void)sdi;

	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				hwopts, ARRAY_SIZE(hwopts), sizeof(int32_t));
		break;
	case SR_CONF_DEVICE_OPTIONS:
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_INT32,
				hwcaps, ARRAY_SIZE(hwcaps), sizeof(int32_t));
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int hw_dev_acquisition_start(const struct sr_dev_inst *sdi,
				    void *cb_data, int dmm)
{
	struct dev_context *devc;

	devc = sdi->priv;

	devc->cb_data = cb_data;

	devc->starttime = g_get_monotonic_time();

	/* Send header packet to the session bus. */
	std_session_send_df_header(cb_data, LOG_PREFIX);

	sr_source_add(0, 0, 10 /* poll_timeout */,
		      udmms[dmm].receive_data, (void *)sdi);

	return SR_OK;
}

static int hw_dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	struct sr_datafeed_packet packet;

	(void)sdi;

	sr_dbg("Stopping acquisition.");

	/* Send end packet to the session bus. */
	sr_dbg("Sending SR_DF_END.");
	packet.type = SR_DF_END;
	sr_session_send(cb_data, &packet);

	/* TODO? */
	sr_source_remove(0);

	return SR_OK;
}

/* Driver-specific API function wrappers */
#define HW_INIT(X) \
static int hw_init_##X(struct sr_context *sr_ctx) { return hw_init(sr_ctx, X); }
#define HW_CLEANUP(X) \
static int hw_cleanup_##X(void) { return hw_cleanup(X); }
#define HW_SCAN(X) \
static GSList *hw_scan_##X(GSList *options) { return hw_scan(options, X); }
#define HW_DEV_LIST(X) \
static GSList *hw_dev_list_##X(void) { return hw_dev_list(X); }
#define CLEAR_INSTANCES(X) \
static int clear_instances_##X(void) { return clear_instances(X); }
#define HW_DEV_ACQUISITION_START(X) \
static int hw_dev_acquisition_start_##X(const struct sr_dev_inst *sdi, \
void *cb_data) { return hw_dev_acquisition_start(sdi, cb_data, X); }
#define HW_DEV_OPEN(X) \
static int hw_dev_open_##X(struct sr_dev_inst *sdi) { return hw_dev_open(sdi, X); }

/* Driver structs and API function wrappers */
#define DRV(ID, ID_UPPER, NAME, LONGNAME) \
HW_INIT(ID_UPPER) \
HW_CLEANUP(ID_UPPER) \
HW_SCAN(ID_UPPER) \
HW_DEV_LIST(ID_UPPER) \
CLEAR_INSTANCES(ID_UPPER) \
HW_DEV_ACQUISITION_START(ID_UPPER) \
HW_DEV_OPEN(ID_UPPER) \
SR_PRIV struct sr_dev_driver ID##_driver_info = { \
	.name = NAME, \
	.longname = LONGNAME, \
	.api_version = 1, \
	.init = hw_init_##ID_UPPER, \
	.cleanup = hw_cleanup_##ID_UPPER, \
	.scan = hw_scan_##ID_UPPER, \
	.dev_list = hw_dev_list_##ID_UPPER, \
	.dev_clear = clear_instances_##ID_UPPER, \
	.config_get = NULL, \
	.config_set = config_set, \
	.config_list = config_list, \
	.dev_open = hw_dev_open_##ID_UPPER, \
	.dev_close = hw_dev_close, \
	.dev_acquisition_start = hw_dev_acquisition_start_##ID_UPPER, \
	.dev_acquisition_stop = hw_dev_acquisition_stop, \
	.priv = NULL, \
};

DRV(tecpel_dmm_8060, TECPEL_DMM_8060, "tecpel-dmm-8060", "Tecpel DMM-8060")
DRV(tecpel_dmm_8061, TECPEL_DMM_8061, "tecpel-dmm-8061", "Tecpel DMM-8061")
DRV(uni_t_ut61d, UNI_T_UT61D, "uni-t-ut61d", "UNI-T UT61D")
DRV(uni_t_ut61e, UNI_T_UT61E, "uni-t-ut61e", "UNI-T UT61E")
DRV(voltcraft_vc820, VOLTCRAFT_VC820, "voltcraft-vc820", "Voltcraft VC-820")
DRV(voltcraft_vc840, VOLTCRAFT_VC840, "voltcraft-vc840", "Voltcraft VC-840")
