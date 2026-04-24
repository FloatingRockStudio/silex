# ADR-001: Graph-Based Schema Architecture

**Status:** Accepted
**Date:** 2025-08-19
**Deciders:** alex.telford
**Related:** PIPE-124

## Context

Traditional path resolution systems use linear, sequential path parsing where each segment is processed in order without awareness of dependencies or relationships. This approach has limitations:

- No explicit representation of segment dependencies
- Difficult to model complex path structures with branches and conditionals
- Limited ability to validate path structure before resolution
- Hard to reason about path transformations

## Decision

Implement a graph-based schema architecture where:

1. **Path segments form a directed acyclic graph (DAG)**
   - Each segment can have multiple child segments
   - Segments can have conditional branches based on context
   - Graph structure explicitly models path hierarchy

2. **Schema loaded into structured objects**
   - `SilexSchema` contains complete segment graph
   - `SilexSegment` represents nodes with patterns and transformations
   - Weak references prevent circular dependencies

3. **Graph traversal for resolution**
   - Path parsing follows graph edges
   - Context building accumulates state through graph walk
   - Backtracking possible at branch points

## Rationale

### Graph vs Linear Benefits

**Graph-Based:**
- Explicit modeling of path structure
- Can handle conditionals and branches naturally
- Enables validation before execution
- Clear visualization of path hierarchy
- Supports multiple resolution strategies

**Linear (Traditional):**
- Simple sequential processing
- No structure representation
- Limited branching support
- Hard to validate structure

### Implementation Details

```python
# Schema graph structure
schema = SilexSchema(
    uuid="project-schema",
    root_segments={
        "project": SilexSegment(
            name="project",
            pattern=r"(FR[0-9]+_)?(.*)",
            children={
                "tree": SilexSegment(...),
                "production": SilexSegment(...)
            }
        )
    }
)

# Graph traversal for resolution
current_segment = schema.root_segments["project"]
while current_segment:
    match = current_segment.pattern.match(path_component)
    context.update(current_segment.extract_data(match))
    current_segment = current_segment.select_child(context)
```

### Segment Relationships

- **Parent-Child**: Hierarchical path structure
- **Sibling Branches**: Alternative paths at same level
- **Cross-References**: Segments can reference others (via names, not direct links)

## Consequences

### Positive

- **Explicit Structure**: Path schema is clearly defined and inspectable
- **Better Validation**: Can validate schema structure before use
- **Flexible Resolution**: Multiple strategies (depth-first, breadth-first, etc.)
- **Debugging**: Graph visualization aids troubleshooting
- **Extension**: Easy to add new segment types or branches

### Negative

- **Complexity**: More complex than linear processing
- **Memory**: Full schema loaded into memory
- **Learning Curve**: Developers need to understand graph concepts

### Mitigations

- Provide clear documentation with graph visualizations
- Implement schema validation to catch errors early
- Use weak references to manage memory efficiently
- Create debugging tools for graph inspection

## Related Decisions

- ADR-002: Expression Graph System with Functors
- ADR-003: URI Support for Multiple Path Types
- ADR-004: Context Mutation During Path Writing

## References

- Initial implementation: commit `3d54d71`
- Schema loader: `python/silex/_internal/schema_loader.py`
- Segment resolver: `python/silex/_internal/segment_resolver.py`

---

**© 2025 Floating Rock Studios. All rights reserved.**
