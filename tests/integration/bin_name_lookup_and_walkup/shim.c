/* Sentinel value the probe prints to confirm `extra_sources` was
 * applied. If the [[bin]] entry's extra_sources weren't picked up,
 * the link step fails on `shim_value` undefined. */
int shim_value(void) { return 42; }
