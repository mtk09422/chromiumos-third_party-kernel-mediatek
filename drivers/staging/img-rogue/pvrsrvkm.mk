pvrsrvkm-y += \
 allocmem.o \
 cache_generic.o \
 client_mm_bridge.o \
 client_pdumpmm_bridge.o \
 client_pvrtl_bridge.o \
 client_sync_bridge.o \
 connection_server.o \
 debugmisc_server.o \
 devicemem.o \
 devicemem_heapcfg.o \
 devicemem_mmap_stub.o \
 devicemem_server.o \
 devicemem_utils.o \
 dllist.o \
 event.o \
 handle.o \
 handle_idr.o \
 hash.o \
 lists.o \
 mm.o \
 mmap.o \
 mmu_common.o \
 module.o \
 osconnection_server.o \
 osfunc.o \
 ossecure_export.o \
 pdump.o \
 physheap.o \
 physmem.o \
 physmem_dmabuf.o \
 physmem_lma.o \
 physmem_osmem_linux.o \
 physmem_tdmetacode_linux.o \
 physmem_tdsecbuf_linux.o \
 pmr.o \
 power.o \
 process_stats.o \
 pvr_bridge_k.o \
 pvr_debug.o \
 pvr_debugfs.o \
 pvr_drm.o \
 pvr_drm_display.o \
 pvr_drm_gem.o \
 pvr_drm_prime.o \
 pvrsrv.o \
 ra.o \
 rgx_compat_bvnc.o \
 rgxbreakpoint.o \
 rgxccb.o \
 rgxcompute.o \
 rgxdebug.o \
 rgxfwutils.o \
 rgxhwperf.o \
 rgxinit.o \
 rgxmem.o \
 rgxmmuinit.o \
 rgxpower.o \
 rgxregconfig.o \
 rgxsync.o \
 rgxta3d.o \
 rgxtimecorr.o \
 rgxtimerquery.o \
 rgxtransfer.o \
 rgxutils.o \
 scp.o \
 server_breakpoint_bridge.o \
 server_cachegeneric_bridge.o \
 server_cmm_bridge.o \
 server_debugmisc_bridge.o \
 server_mm_bridge.o \
 server_pdump_bridge.o \
 server_pdumpctrl_bridge.o \
 server_pdumpmm_bridge.o \
 server_pvrtl_bridge.o \
 server_regconfig_bridge.o \
 server_rgxcmp_bridge.o \
 server_rgxhwperf_bridge.o \
 server_rgxinit_bridge.o \
 server_rgxpdump_bridge.o \
 server_rgxta3d_bridge.o \
 server_rgxtq_bridge.o \
 server_smm_bridge.o \
 server_srvcore_bridge.o \
 server_sync_bridge.o \
 server_syncexport_bridge.o \
 server_syncsexport_bridge.o \
 server_timerquery_bridge.o \
 srvcore.o \
 sync.o \
 sync_server.o \
 tlclient.o \
 tlintern.o \
 tlserver.o \
 tlstream.o \
 uniq_key_splay_tree.o
ifeq ($(CONFIG_POWERVR_ROGUE_DEBUG),y)
pvrsrvkm-y += \
 client_ri_bridge.o \
 ri_server.o \
 server_ri_bridge.o
endif

pvrsrvkm-y += \
 mt8173_sysconfig.o \
 mt8173_mfgsys.o \
 mt8173_mfgdvfs.o 
#pvrsrvkm-y += \
# clk-mt8135-pg.o \
# sysconfig.o \
# mtk_mfgsys.o \
# mtk_mfgdvfs.o 
#endif

pvrsrvkm-$(CONFIG_X86) += osfunc_x86.o
pvrsrvkm-$(CONFIG_ARM) += osfunc_arm.o
pvrsrvkm-$(CONFIG_ARM64) += osfunc_arm64.o
pvrsrvkm-$(CONFIG_METAG) += osfunc_metag.o
pvrsrvkm-$(CONFIG_MIPS) += osfunc_mips.o
pvrsrvkm-$(CONFIG_EVENT_TRACING) += trace_events.o
