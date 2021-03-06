MODULE = nimble_host

SRC += ble_att.c
SRC += ble_att_clt.c
SRC += ble_att_cmd.c
SRC += ble_att_svr.c
SRC += ble_eddystone.c
SRC += ble_gap.c
SRC += ble_gattc.c
SRC += ble_gatts.c
SRC += ble_hs_adv.c
SRC += ble_hs_atomic.c
SRC += ble_hs.c
SRC += ble_hs_cfg.c
SRC += ble_hs_conn.c
SRC += ble_hs_dbg.c
SRC += ble_hs_flow.c
SRC += ble_hs_hci.c
SRC += ble_hs_hci_cmd.c
SRC += ble_hs_hci_evt.c
SRC += ble_hs_hci_util.c
SRC += ble_hs_id.c
SRC += ble_hs_log.c
SRC += ble_hs_mbuf.c
SRC += ble_hs_mqueue.c
SRC += ble_hs_misc.c
SRC += ble_hs_pvcy.c
SRC += ble_hs_startup.c
SRC += ble_ibeacon.c
SRC += ble_l2cap.c
SRC += ble_l2cap_coc.c
SRC += ble_l2cap_sig.c
SRC += ble_l2cap_sig_cmd.c
SRC += ble_monitor.c
SRC += ble_sm_alg.c
SRC += ble_sm.c
SRC += ble_sm_cmd.c
SRC += ble_sm_lgcy.c
SRC += ble_sm_sc.c
SRC += ble_store.c
SRC += ble_store_util.c
SRC += ble_uuid.c

include $(RIOTBASE)/Makefile.base
