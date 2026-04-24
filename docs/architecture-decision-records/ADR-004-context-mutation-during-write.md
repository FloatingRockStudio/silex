# ADR-004: Context Mutation During Path Writing

**Status:** Accepted
**Date:** 2025-08-29
**Deciders:** alex.telford
**Related:** PIPE-124

When generating paths from context (write mode), the system sometimes needs to derive additional context values based on path generation rules. For example:

- Inferring version numbers during versioned path generation
- Calculating default values for missing parameters
- Resolving references to other context values
- Applying transforms that produce intermediate values

Without context mutation, these derived values are lost and unavailable for subsequent segments or metadata.

## Decision

Allow write operations to update the context as path segments are generated:

1. **Mutable Context During Write**
   - Context passed by reference through write pipeline
   - Functors and expressions can add/update context values
   - Changes accumulate through segment graph traversal

2. **Controlled Mutation**
   - Only write operations mutate context
   - Read operations produce new context values (no mutation)
   - Clear documentation of which operations mutate

3. **Use Cases**
   - Version number assignment during publish
   - Default parameter resolution
   - Intermediate value calculation
   - Cross-segment data flow

## Rationale

### Problem Example

```python
# Context before write
context = {
    "project": "PROJ",
    "entity": "chrBob",
    "fragment": "model"
    # No version specified
}

# Path generation needs version
# Without mutation: version lost after generation
"/projects/PROJ/assets/chrBob/model/v001"

# With mutation: version available for metadata
context["version"] = 1  # Updated during write
```

### Alternative Approaches Considered

**1. Return Updated Context**
```python
path, updated_context = resolver.resolve_path(context, schema)
```
- **Pros**: Immutability preserved, explicit updates
- **Cons**: Cumbersome API, easy to forget updated context

**2. Separate Context Derivation Phase**
```python
enriched_context = resolver.derive_context(context, schema)
path = resolver.resolve_path(enriched_context, schema)
```
- **Pros**: Clear separation, no mutation
- **Cons**: Two passes required, performance cost, complex logic

**3. Context Mutation (Chosen)**
```python
path = resolver.resolve_path(context, schema)
# context now contains derived values
print(context["version"])  # 1
```
- **Pros**: Simple API, single pass, efficient
- **Cons**: Mutation side-effects, must be documented

### Why Mutation Works Here

1. **Controlled Scope**: Only write operations mutate
2. **Documented Behavior**: Clear documentation prevents surprises
3. **Performance**: Single-pass resolution
4. **Natural Flow**: Matches mental model of path generation
5. **Practical**: Derived values often needed (version, etc.)

## Implementation

### Write Expression Evaluation

```python
def evaluate_write_expression(
    expression: SilexExpression,
    inputs: List[Any],
    context: SilexFunctorContext,  # Mutable
    config: SilexConfig
) -> WriteResult:
    """
    Evaluate write expression and update context.

    Context is passed by reference and may be updated
    by functors during evaluation.
    """
    result = functor.write(inputs, context)

    # Functor may have updated context.context
    # Changes propagate to caller

    return result
```

### Functor Context Mutation

```python
class VersionFunctor(ISilexFunctor):
    def write(self, inputs, context):
        """Generate version path and update context."""

        # Calculate next version
        existing_versions = self._scan_versions(context.parent)
        next_version = max(existing_versions) + 1 if existing_versions else 1

        # Update context (mutation)
        context.context["version"] = next_version

        # Return formatted version
        return WriteResult(
            success=True,
            message="Version assigned",
            output=f"v{next_version:03d}"
        )
```

### Publisher Integration

```python
# Publisher resolves paths and uses derived context
batch = client.batch_publish(context, info)
# context: {"project": "PROJ", "entity": "chrBob"}

fragment = batch.create_fragment("model", template="asset_model")
paths = batch.reserve()  # Triggers path write

# After reserve, context enriched:
# context: {"project": "PROJ", "entity": "chrBob", "version": 1}

# Enriched context used for metadata
batch.finalize()  # Metadata includes version
```

## Consequences

### Positive

- **Single Pass**: Path generation and context enrichment in one step
- **Efficiency**: No separate derivation phase
- **Natural Flow**: Matches mental model
- **Derived Values Available**: Version numbers, defaults, etc. accessible
- **Simple API**: No complex return types or multiple calls

### Negative

- **Mutation Side-Effects**: Context changed as side-effect
- **Less Functional**: Not pure functional programming
- **Potential Confusion**: Developers may not expect mutation
- **Debugging**: Harder to trace context changes

### Mitigations

- **Documentation**: Clearly document mutation behavior
- **Naming**: Method names indicate mutation (`resolve_path` modifies context)
- **Logging**: Log context changes for debugging
- **Testing**: Test context state after operations
- **Guidelines**: Best practices for when to rely on mutation

## Guidelines for Use

### When to Mutate Context

- Version number assignment during publish
- Default parameter resolution from templates
- Derived values needed for metadata
- Cross-segment data flow

### When NOT to Mutate Context

- Read operations (path → context)
- Validation operations
- Query operations
- When immutability is important

### Documentation Requirements

```python
def resolve_path(self, context, schema):
    """
    Generate path from context.

    Args:
        context: Context dictionary (WILL BE MODIFIED)
        schema: Schema to use for resolution

    Returns:
        Generated path string

    Note:
        This method mutates the context to add derived values
        such as version numbers and default parameters.
    """
```

## Related Decisions

- ADR-001: Graph-Based Schema Architecture
- ADR-002: Expression Graph System with Functors
- ADR-003: URI Support for fr_asset_api

## References

- Write evaluation: `python/silex/_internal/expression_evaluator.py`
- Context struct: `python/silex/structs.py` (`SilexFunctorContext`)
- Publisher integration: `python/fr_asset_api/impl/publisher.py`

---

**© 2025 Floating Rock Studios. All rights reserved.**
