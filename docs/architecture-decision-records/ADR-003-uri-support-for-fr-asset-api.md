# ADR-003: URI Support for fr_asset_api

**Status:** Accepted
**Date:** 2025-09-01
**Deciders:** alex.telford
**Related:** PIPE-124

`fr_asset_api` needed a standardized way to reference published assets across the pipeline. File paths alone are insufficient because they:

- Don't capture semantic information (entity, fragment, component, tags)
- Are environment-specific (local vs network paths)
- Can't encode query parameters (version, LOD, etc.)
- Difficult to parse and validate

## Decision

Implement URI-based asset addressing with the `FRA` scheme:

```
fra://<endpoint>/<project>/<entity>/<fragment>/<component>?<parameters>#<tags>
```

### URI Components

- **Scheme**: `fra://` - Floating Rock Asset protocol
- **Endpoint**: Asset type (`asset_entity`, `asset_fragment`, `asset_component`)
- **Path**: Hierarchical asset address (`PROJECT/entity/fragment/component`)
- **Query**: Parameters (`ext=ma`, `lod=high`, `udim=1001`)
- **Fragment**: Tags (`tag=latest`, `tag=published`)

### Examples

```python
# Entity URI
"fra://asset_entity/TESTPIPE/chrBob"

# Fragment URI with tag
"fra://asset_fragment/TESTPIPE/chrBob/model?tag=latest"

# Component URI with parameters
"fra://asset_component/TESTPIPE/chrBob/model/scene?ext=ma&lod=high"
```

## Rationale

### Why URIs?

**Standardized Format:**
- Industry-standard URI syntax
- Parsers readily available
- Well-understood by developers

**Semantic Addressing:**
- Encodes asset hierarchy
- Query parameters for variations
- Tags for version selection

**Environment Independent:**
- Same URI works across machines
- Resolver handles path mapping
- Platform agnostic

**Integration Friendly:**
- Can be stored in metadata
- Passed between tools
- Used in Maya references, USD references, etc.

### FRA Scheme Choice

- **fra**: Floating Rock Asset (studio-specific)
- Lowercase for consistency with URI standards
- Clearly identifies asset protocol vs generic file://

### Silex Integration

Silex URI segmenter enables URI path resolution:

```python
# URI segmenter splits on ://
"fra://asset_fragment/PROJ/entity/model"
→ ["fra://", "asset_fragment", "PROJ", "entity", "model"]

# Graph-based schema resolves to context
{
    "endpoint": "asset_fragment",
    "project": "PROJ",
    "entity": "entity",
    "fragment": "model"
}

# Context resolves to filesystem path
"/projects/PROJ/assets/entity/model/v001"
```

## Implementation

### Context Types

```python
@dataclass
class EntityContext(BaseContext):
    context_type: str = "asset_entity"
    endpoint: str = "entity"
    entity_name: str
    entity_context: Dict[str, Any]

@dataclass
class FragmentContext(BaseContext):
    context_type: str = "asset_fragment"
    endpoint: str = "fragment"
    entity_name: str
    fragment_name: str
    tags: List[str] = field(default_factory=list)
    entity_context: Dict[str, Any]

@dataclass
class ComponentContext(BaseContext):
    context_type: str = "asset_component"
    endpoint: str = "component"
    entity_name: str
    fragment_name: str
    component_name: str
    parameters: Dict[str, Any] = field(default_factory=dict)
    tags: List[str] = field(default_factory=list)
    entity_context: Dict[str, Any]
```

### FRAResolver Methods

```python
class FRAResolver(IResolver):
    def uri_from_context(self, context: BaseContext) -> str:
        """Convert context to URI."""
        # fra://asset_fragment/PROJ/entity/model?tag=latest
        pass

    def context_from_uri(self, uri: str) -> BaseContext:
        """Parse URI to context."""
        # Resolves tags to specific versions
        pass

    def resolve_fragment(self, context: FragmentContext) -> FragmentMetaData:
        """Resolve fragment URI to filesystem metadata."""
        pass

    def resolve_component(self, context: ComponentContext) -> ResolvedComponent:
        """Resolve component URI to file path."""
        pass
```

### Tag Resolution

Tags in URIs resolve to specific versions:

```python
# URI with tag
"fra://asset_fragment/PROJ/chrBob/model?tag=latest"

# Resolves to specific version
context.entity_context["version"] = 3

# Maps to filesystem path
"/projects/PROJ/assets/chrBob/model/v003"
```

## Consequences

### Positive

- **Standardized References**: Consistent asset addressing across tools
- **Semantic Clarity**: URIs are self-documenting
- **Version Management**: Tags abstract version numbers
- **Tool Integration**: URIs work in Maya, USD, databases, etc.
- **Environment Portability**: Same URI works everywhere
- **Query Flexibility**: Parameters encode variations

### Negative

- **URI Parsing Overhead**: Slight performance cost vs direct paths
- **Learning Curve**: Users must understand URI syntax
- **Scheme Registration**: Studio-specific scheme needs documentation

### Mitigations

- Cache parsed URIs for repeated lookups
- Provide helper functions for URI construction
- Document URI syntax prominently
- Implement URI validation and error messages

## Usage Example

### Maya Workflow

```python
# Publish with automatic URI generation
workflow = ModelWorkflow(client)
uri = workflow.do_publish(context, nodes)
# Returns: "fra://asset_fragment/PROJ/chrBob/model?tag=latest"

# Reference using URI
workflow.do_import(context, namespace="MODEL", component_name="scene")
# Internally resolves URI to path for Maya reference
```

### Resolution Pipeline

```
URI → Context → Path → File
↓      ↓        ↓      ↓
fra:// parse    silex  exists
       tags     resolve check
```

## Related Decisions

- ADR-001: Graph-Based Schema Architecture (Silex)
- ADR-002: Expression Graph System with Functors (Silex)
- ADR-004: Context Mutation During Path Writing

## References

- URI support commit: `a823919`
- FRAResolver: `python/fr_asset_api/impl/resolver.py`
- URI segmenter: `python/silex/impl/segmenters/uri.py`
- Context structs: `python/fr_asset_api/structs.py`

---

**© 2025 Floating Rock Studios. All rights reserved.**
