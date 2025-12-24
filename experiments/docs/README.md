# Experiments Documentation

This directory contains detailed analysis and evidence for concurrency model decisions.

## Contents

### Evidence and Analysis

- **[evidence-summary.md](evidence-summary.md)** - Empirical performance data comparing state machine actors vs pthreads. Shows 6,000x-50,000x memory improvement and 1,000x-10,000x throughput improvement.

- **[evidence-plan.md](evidence-plan.md)** - Safe testing methodology for gathering empirical evidence before implementation. Includes incremental scaling tests and decision criteria.

- **[erlang-go-comparison.md](erlang-go-comparison.md)** - Technical comparison to industry standards (Erlang BEAM and Go). Explains what we learned, what we're copying, and where we differ.

### Implementation

- **[summary.md](summary.md)** - High-level overview of the entire experiments framework, key findings, and recommendations.

## Quick Links

- **Start Here**: [evidence-summary.md](evidence-summary.md) - Review the empirical data
- **Next Steps**: See `../../docs/IMPLEMENTATION_PLAN.md` for implementation roadmap
- **Benchmarks**: Run `../run_safe_benchmarks.sh` to reproduce tests

## Status

Evidence collection complete. All criteria exceeded. Safe to proceed with implementation of:
1. Struct types
2. Pattern matching
3. Actor syntax
4. State machine codegen
