# Documentation Organization

All documentation is organized into logical directories with professional lowercase filenames.

## Structure

```
d:\Git\aether\
├── docs/                           # Main project documentation
│   ├── GETTING_STARTED.md         # Beginner guide
│   ├── LANGUAGE_SPEC.md            # Language reference
│   ├── ROADMAP.md                  # Development roadmap
│   ├── CONCURRENCY_EXPERIMENTS.md  # High-level concurrency analysis
│   ├── PROJECT_STATUS.md           # Current project state
│   └── IMPLEMENTATION_PLAN.md      # Implementation timeline
│
├── experiments/                    # Concurrency experiments
│   ├── docs/                       # Experiment documentation
│   │   ├── README.md              # Documentation index
│   │   ├── evidence-summary.md    # Empirical performance data
│   │   ├── evidence-plan.md       # Testing methodology
│   │   ├── erlang-go-comparison.md # Industry comparison
│   │   └── summary.md             # Framework overview
│   │
│   ├── 01_pthread_baseline/       # Experiment implementations
│   ├── 02_state_machine/
│   ├── 03_work_stealing/
│   └── README.md                   # Experiments overview
│
├── examples/                       # Example programs
├── runtime/                        # Actor runtime system
├── src/                           # Compiler source
├── stdlib/                        # Standard library
├── tests/                         # Test suite
├── BUILD_INSTRUCTIONS.md          # Build guide
└── README.md                      # Project overview
```

## File Naming Conventions

- **ALL_CAPS.md**: Reserved for top-level project files (README.md, LICENSE, etc.)
- **lowercase-with-dashes.md**: All other documentation
- **PascalCase/**: Directory names use clear, readable names
- **snake_case/**: Source code and examples

## Finding Documentation

### For Users
- Start: `README.md`
- Getting started: `docs/GETTING_STARTED.md`
- Language reference: `docs/LANGUAGE_SPEC.md`

### For Contributors
- Project status: `docs/PROJECT_STATUS.md`
- Implementation plan: `docs/IMPLEMENTATION_PLAN.md`
- Experiments: `experiments/docs/evidence-summary.md`

### For Researchers
- Concurrency experiments: `experiments/README.md`
- Performance data: `experiments/docs/evidence-summary.md`
- Industry comparison: `experiments/docs/erlang-go-comparison.md`
