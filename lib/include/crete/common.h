#ifndef CRETE_INCLUDE_COMMON
#define CRETE_INCLUDE_COMMON

#ifdef __cplusplus
extern "C" {
#endif

const char *CRETE_RAMDISK_PATH = "/tmp/ramdisk";
const char *CRETE_PROC_MAPS_PATH = "/tmp/proc-maps.log";
const char *CRETE_CONFIG_SERIALIZED_PATH = "/tmp/harness.config.serialized";

const char *CRETE_SANDBOX_PATH = "/tmp/sandbox";

const char *CRETE_REPLAY_CURRENT_TC = "/tmp/crete.replay.current.tc.bin";
const char *CRETE_REPLAY_GCOV_PREFIX = "/tmp/gcov";

// CUSTOMIZED EXIT CODE
const int CRETE_EXIT_CODE_SIG_BASE = 30;

#ifdef __cplusplus
}
#endif

#endif
