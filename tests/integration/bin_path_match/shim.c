/* Only gets compiled if [[bin]] extra_sources applies to the build
 * invocation. If the match logic fails to match the bin entry to the
 * ae_file path, this file is never compiled and the link step errors
 * on the missing ae_bin_probe_value symbol.
 */
int ae_bin_probe_value(void) { return 1; }
