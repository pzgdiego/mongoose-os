/*
 * Copyright (c) 2014-2016 Cesanta Software Limited
 * All rights reserved
 */

#include "common/clubby/clubby_channel_ws.h"

#if MG_ENABLE_CLUBBY

#include "fw/src/miot_clubby.h"
#include "fw/src/miot_clubby_channel_uart.h"
#include "fw/src/miot_config.h"
#include "fw/src/miot_mongoose.h"
#include "fw/src/miot_sys_config.h"
#include "fw/src/miot_uart.h"
#include "fw/src/miot_wifi.h"

static struct clubby *s_global_clubby;

static void clubby_wifi_ready(enum miot_wifi_status event, void *arg) {
  if (event != MIOT_WIFI_IP_ACQUIRED) return;
  struct clubby_channel *ch = (struct clubby_channel *) arg;
  ch->connect(ch);
}

struct clubby_cfg *miot_clubby_cfg_from_sys(const struct sys_config *scfg) {
  struct clubby_cfg *ccfg = (struct clubby_cfg *) calloc(1, sizeof(*ccfg));
  miot_conf_set_str(&ccfg->id, scfg->device.id);
  miot_conf_set_str(&ccfg->psk, scfg->device.password);
  ccfg->max_queue_size = scfg->clubby.max_queue_size;
  return ccfg;
}

enum miot_init_result miot_clubby_init(void) {
  const struct sys_config_clubby *sccfg = &get_cfg()->clubby;
  if (!sccfg->enable) return MIOT_INIT_OK;
  struct clubby_cfg *ccfg = miot_clubby_cfg_from_sys(get_cfg());
  struct clubby *c = clubby_create(ccfg);
  if (sccfg->server_address != NULL) {
    struct clubby_channel_ws_out_cfg *chcfg =
        miot_clubby_channel_ws_out_cfg_from_sys(get_cfg());
    struct clubby_channel *ch = clubby_channel_ws_out(miot_get_mgr(), chcfg);
    if (ch == NULL) {
      return MIOT_INIT_CLUBBY_FAILED;
    }
    clubby_add_channel(c, mg_mk_str(MG_CLUBBY_DST_DEFAULT), ch,
                       false /* is_trusted */, true /* send_hello */);
    if (sccfg->connect_on_boot) {
      if (get_cfg()->wifi.sta.enable) {
        miot_wifi_add_on_change_cb(clubby_wifi_ready, ch);
      } else {
        clubby_connect(c);
      }
    }
  }
#if MG_ENABLE_CLUBBY_UART
  if (sccfg->uart.uart_no >= 0) {
    const struct sys_config_clubby_uart *scucfg = &get_cfg()->clubby.uart;
    struct miot_uart_config *ucfg = miot_uart_default_config();
    ucfg->baud_rate = scucfg->baud_rate;
    ucfg->rx_fc_ena = ucfg->tx_fc_ena = scucfg->fc_enable;
    if (miot_uart_init(scucfg->uart_no, ucfg, NULL, NULL) != NULL) {
      struct clubby_channel *uch =
          clubby_channel_uart(scucfg->uart_no, scucfg->wait_for_start_frame);
      clubby_add_channel(c, mg_mk_str(""), uch, true /* is_trusted */,
                         false /* send_hello */);
      if (sccfg->connect_on_boot) uch->connect(uch);
    }
  }
#endif
  s_global_clubby = c;
  return MIOT_INIT_OK;
}

struct clubby_channel_ws_out_cfg *miot_clubby_channel_ws_out_cfg_from_sys(
    const struct sys_config *sccfg) {
  struct clubby_channel_ws_out_cfg *chcfg =
      (struct clubby_channel_ws_out_cfg *) calloc(1, sizeof(*chcfg));
  miot_conf_set_str(&chcfg->server_address, sccfg->clubby.server_address);
  miot_conf_set_str(&chcfg->ssl_ca_file, sccfg->clubby.ssl_ca_file);
  miot_conf_set_str(&chcfg->ssl_client_cert_file,
                    sccfg->clubby.ssl_client_cert_file);
  miot_conf_set_str(&chcfg->ssl_server_name, sccfg->clubby.ssl_server_name);
  chcfg->reconnect_interval_min = sccfg->clubby.reconnect_timeout_min;
  chcfg->reconnect_interval_max = sccfg->clubby.reconnect_timeout_max;
  return chcfg;
}

struct clubby *miot_clubby_get_global(void) {
  return s_global_clubby;
}
#endif /* MG_ENABLE_CLUBBY */