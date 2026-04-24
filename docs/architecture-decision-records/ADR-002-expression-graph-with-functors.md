# ADR-002: Expression Graph System with Functors

**Status:** Accepted
**Date:** 2025-08-25
**Deciders:** alex.telford
**Related:** PIPE-124

Path resolution requires transforming raw path segments into structured context data (read) and generating path segments from context (write). Traditional approaches process expressions sequentially, which creates problems:

- No dependency tracking between transformations
- Difficult to reuse transformation logic
- Limited composability
- Hard to extend with custom transformations

## Decision

Implement an expression graph system with pluggable functors:

1. **Expression Graphs**
   - Expressions analyzed for dependencies
   - Topological sorting determines execution order
   - Graph ensures dependencies satisfied before execution

2. **Functor System**
   - Pluggable functions for transformations
   - Registry-based discovery and instantiation
   - Built-in functors: lexicon, case conversion, split/join
   - Custom functors via interface implementation

3. **Read/Write Modes**
   - **Read**: Extract data from paths (`$functor(input)->output`)
   - **Write**: Generate path components (`$functor(input)`)

## Rationale

### Expression Dependencies

Complex resolutions require multiple transformations where later steps depend on earlier results:

```python
# Dependencies: variant depends on asset_name
expressions = [
    "$split({group[1]}, '_')->asset_name, variant",  # Produces 2 outputs
    "$upper({asset_name})->asset_upper",             # Depends on asset_name
    "$lexicon({variant}, variant_type)->variant"     # Depends on variant
]

# Execution order automatically determined: [0, 1, 2]
```

Without dependency tracking, these must be manually ordered and errors are common.

### Functor Benefits

**Pluggable:**
- Studios can add custom transformations
- No core code modification required
- Register functors via registry

**Reusable:**
- Common logic centralized (lexicon, case, etc.)
- Consistent behavior across schemas
- Testable in isolation

**Composable:**
- Chain functors in expressions
- Mix built-in and custom functors

## Implementation

### Expression Parser

```python
# Parse expression string
"$lexicon($lower({group[1]}), classification)->classification"

# Into structured expression
SilexExpression(
    functor_uid="lexicon",
    inputs=[
        SilexExpression(
            functor_uid="lower",
            inputs=["{group[1]}"]
        ),
        "classification"
    ],
    outputs=[
        FunctorOutput(name="classification")
    ]
)
```

### Expression Graph

```python
# Build dependency graph
graph = SilexExpressionGraph(
    expressions=[expr1, expr2, expr3],
    dependencies={
        "asset_upper": ["asset_name"],
        "variant": ["variant"]
    },
    execution_order=["expr1", "expr2", "expr3"]
)

# Evaluate in order
evaluator.evaluate_graph(graph, context, config)
```

### Functor Interface

```python
class ISilexFunctor(abc.ABC):
    @abc.abstractmethod
    def read(self, inputs, outputs, context) -> ReadResult:
        """Transform inputs during path->context."""
        pass

    @abc.abstractmethod
    def write(self, inputs, context) -> WriteResult:
        """Generate path component from context."""
        pass
```

### Built-in Functors

| Functor | Purpose | Example |
|---------|---------|---------|
| `lexicon` | Vocabulary mapping | `$lexicon({group[1]}, classification)` |
| `lower` | Lowercase conversion | `$lower({entity})` |
| `upper` | Uppercase conversion | `$upper({project})` |
| `split` | String splitting | `$split({group[1]}, '_')->a, b` |
| `camelcase` | CamelCase conversion | `$camelcase({name})` |

## Consequences

### Positive

- **Automatic Dependency Resolution**: No manual expression ordering
- **Extensibility**: Studios add functors without core changes
- **Reusability**: Common logic centralized in functors
- **Composability**: Chain functors in expressions
- **Testability**: Functors tested independently
- **Clear Semantics**: Explicit inputs/outputs

### Negative

- **Complexity**: Graph building and evaluation adds overhead
- **Learning Curve**: Users must understand expression syntax
- **Debugging**: Graph execution harder to trace than sequential

### Mitigations

- Provide clear expression documentation and examples
- Implement detailed logging for graph evaluation
- Create debugging tools to visualize expression graphs
- Build comprehensive functor library to reduce custom development

## Related Decisions

- ADR-001: Graph-Based Schema Architecture
- ADR-003: URI Support for Multiple Path Types
- ADR-004: Context Mutation During Path Writing

## References

- Expression parser: `python/silex/_internal/expression_parser.py`
- Expression evaluator: `python/silex/_internal/expression_evaluator.py`
- Expression graph: `python/silex/_internal/expression_graph.py`
- Functor registry: `python/silex/_internal/functor_registry.py`
- Built-in functors: `python/silex/impl/functors/`
- Initial implementation: commits `55cb90b`, `02ad3bd`, `350b19c`

---

**© 2025 Floating Rock Studios. All rights reserved.**
