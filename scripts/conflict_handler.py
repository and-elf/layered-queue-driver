#!/usr/bin/env python3
"""
Requirement Conflict Detection and Resolution
Handles validation of requirements and provides resolution strategies
"""

from enum import Enum
from dataclasses import dataclass
from typing import List, Optional, Dict, Any


class ConflictSeverity(Enum):
    """Severity levels for requirement conflicts"""
    ERROR = "error"      # Cannot generate valid code - MUST resolve
    WARNING = "warning"  # Can generate code but may not meet requirements
    INFO = "info"        # Potential issue - review recommended


class ConflictType(Enum):
    """Types of conflicts that can occur"""
    RESOURCE = "resource"           # Hardware resource conflict (ADC, GPIO, etc)
    TIMING = "timing"               # Timing requirements cannot be met
    DEPENDENCY = "dependency"       # Circular or missing dependencies
    INCOMPATIBLE = "incompatible"   # Mutually exclusive requirements
    BUDGET = "budget"               # Resource budget exceeded (memory, CPU, etc)


@dataclass
class Conflict:
    """Represents a single requirement conflict"""
    severity: ConflictSeverity
    type: ConflictType
    description: str
    affected_requirements: List[str]
    resolution_strategies: List[str]
    auto_resolvable: bool = False
    metadata: Dict[str, Any] = None

    def __post_init__(self):
        if self.metadata is None:
            self.metadata = {}


class ConflictHandler:
    """Handles detection and resolution of requirement conflicts"""

    def __init__(self, strict_mode: bool = False):
        """
        Args:
            strict_mode: If True, treat warnings as errors
        """
        self.strict_mode = strict_mode
        self.conflicts: List[Conflict] = []

    def add_conflict(self, conflict: Conflict):
        """Add a detected conflict"""
        self.conflicts.append(conflict)

    def has_errors(self) -> bool:
        """Check if any ERROR level conflicts exist"""
        return any(c.severity == ConflictSeverity.ERROR for c in self.conflicts)

    def has_warnings(self) -> bool:
        """Check if any WARNING level conflicts exist"""
        return any(c.severity == ConflictSeverity.WARNING for c in self.conflicts)

    def should_fail(self) -> bool:
        """Determine if code generation should be blocked"""
        if self.has_errors():
            return True
        if self.strict_mode and self.has_warnings():
            return True
        return False

    def check_resource_conflict(self, llrs: List[Any]) -> List[Conflict]:
        """Detect hardware resource conflicts between LLRs"""
        conflicts = []
        resource_map = {}

        for llr in llrs:
            resources = self._extract_resources(llr)

            for resource_type, resource_id in resources:
                key = f"{resource_type}:{resource_id}"

                if key in resource_map:
                    # Hard conflict - two LLRs want same exclusive resource
                    conflicts.append(Conflict(
                        severity=ConflictSeverity.ERROR,
                        type=ConflictType.RESOURCE,
                        description=f"Multiple requirements claim {resource_type} {resource_id}",
                        affected_requirements=[resource_map[key], llr.id],
                        resolution_strategies=[
                            f"Assign different {resource_type} to one requirement",
                            "Add hardware multiplexer/sharing mechanism",
                            "Mark one requirement as superseded"
                        ],
                        auto_resolvable=False,
                        metadata={
                            'resource_type': resource_type,
                            'resource_id': resource_id
                        }
                    ))
                else:
                    resource_map[key] = llr.id

        return conflicts

    def check_timing_conflict(self, hlrs: List[Any], llrs: List[Any]) -> List[Conflict]:
        """Detect timing requirement violations"""
        conflicts = []

        for hlr in hlrs:
            # Extract timing constraint from HLR (e.g., "< 50ms")
            required_time = self._extract_timing_requirement(hlr)
            if not required_time:
                continue

            # Find all LLRs implementing this HLR
            child_llrs = [llr for llr in llrs if llr.parent == hlr.id]

            # Calculate worst-case latency from LLR implementation
            actual_time = self._calculate_latency(child_llrs)

            if actual_time > required_time:
                # Timing cannot be met
                severity = ConflictSeverity.ERROR if actual_time > required_time * 1.5 else ConflictSeverity.WARNING

                conflicts.append(Conflict(
                    severity=severity,
                    type=ConflictType.TIMING,
                    description=f"{hlr.id} requires <{required_time}ms but implementation takes ~{actual_time}ms",
                    affected_requirements=[hlr.id] + [llr.id for llr in child_llrs],
                    resolution_strategies=[
                        f"Reduce engine cycle time to achieve <{required_time}ms",
                        f"Relax {hlr.id} timing requirement to <{actual_time * 1.2}ms",
                        "Use interrupt-driven processing for critical path",
                        "Optimize implementation to reduce latency"
                    ],
                    auto_resolvable=False,
                    metadata={
                        'required_ms': required_time,
                        'actual_ms': actual_time,
                        'margin_pct': ((actual_time - required_time) / required_time) * 100
                    }
                ))

        return conflicts

    def check_dependency_conflict(self, llrs: List[Any]) -> List[Conflict]:
        """Detect circular or missing dependencies"""
        conflicts = []

        # Build node_name to LLR ID mapping
        node_to_llr = {}
        for llr in llrs:
            node_name = llr.implementation.get('node_name')
            if node_name:
                node_to_llr[node_name] = llr.id

        # Build dependency graph
        graph = {}
        for llr in llrs:
            graph[llr.id] = self._extract_dependencies(llr, node_to_llr)

        # Check for circular dependencies
        visited = set()
        rec_stack = set()

        def has_cycle(node: str, path: List[str]) -> Optional[List[str]]:
            visited.add(node)
            rec_stack.add(node)

            for neighbor in graph.get(node, []):
                if neighbor not in visited:
                    cycle = has_cycle(neighbor, path + [neighbor])
                    if cycle:
                        return cycle
                elif neighbor in rec_stack:
                    # Found cycle
                    return path[path.index(neighbor):] + [neighbor]

            rec_stack.remove(node)
            return None

        for llr_id in graph:
            if llr_id not in visited:
                cycle = has_cycle(llr_id, [llr_id])
                if cycle:
                    conflicts.append(Conflict(
                        severity=ConflictSeverity.ERROR,
                        type=ConflictType.DEPENDENCY,
                        description=f"Circular dependency detected: {' → '.join(cycle)}",
                        affected_requirements=cycle[:-1],  # Remove duplicate at end
                        resolution_strategies=[
                            "Break cycle by removing one dependency",
                            "Add intermediate signal to resolve cycle",
                            "Restructure requirements to avoid circular reference"
                        ],
                        auto_resolvable=False,
                        metadata={'cycle': cycle}
                    ))

        # Check for missing dependencies
        all_llr_ids = {llr.id for llr in llrs}
        for llr in llrs:
            for dep in self._extract_dependencies(llr, node_to_llr):
                if dep not in all_llr_ids:
                    conflicts.append(Conflict(
                        severity=ConflictSeverity.ERROR,
                        type=ConflictType.DEPENDENCY,
                        description=f"{llr.id} depends on {dep} which does not exist",
                        affected_requirements=[llr.id],
                        resolution_strategies=[
                            f"Create missing requirement {dep}",
                            f"Update {llr.id} to reference correct requirement",
                            f"Remove dependency on {dep}"
                        ],
                        auto_resolvable=False
                    ))

        return conflicts

    def check_budget_conflict(self, llrs: List[Any], limits: Dict[str, int]) -> List[Conflict]:
        """Check if resource budgets are exceeded"""
        conflicts = []

        # Count resources
        counts = {
            'signals': 0,
            'cyclic_outputs': 0,
            'merges': 0
        }

        for llr in llrs:
            node_type = llr.implementation.get('node_type', '')
            if 'input' in node_type or 'scale' in node_type or 'verified' in node_type:
                counts['signals'] += 1
            if 'cyclic-output' in node_type:
                counts['cyclic_outputs'] += 1
            if 'merge' in node_type or 'voter' in node_type:
                counts['merges'] += 1

        # Check against limits
        for resource, count in counts.items():
            limit = limits.get(resource, float('inf'))
            if count > limit:
                conflicts.append(Conflict(
                    severity=ConflictSeverity.WARNING,
                    type=ConflictType.BUDGET,
                    description=f"Resource budget exceeded: {count} {resource} used, limit is {limit}",
                    affected_requirements=[llr.id for llr in llrs],  # All LLRs contribute
                    resolution_strategies=[
                        f"Increase max-{resource.replace('_', '-')} in engine configuration",
                        f"Reduce number of {resource} by combining requirements",
                        "Use Kconfig to increase resource limits"
                    ],
                    auto_resolvable=True,  # Can auto-increase limits
                    metadata={
                        'resource': resource,
                        'count': count,
                        'limit': limit,
                        'suggested_limit': count + 8  # Add headroom
                    }
                ))

        return conflicts

    def report(self, verbose: bool = False) -> str:
        """Generate human-readable conflict report"""
        if not self.conflicts:
            return "✅ No conflicts detected\n"

        lines = []
        lines.append(f"\n{'='*60}")
        lines.append("REQUIREMENT CONFLICT REPORT")
        lines.append(f"{'='*60}\n")

        errors = [c for c in self.conflicts if c.severity == ConflictSeverity.ERROR]
        warnings = [c for c in self.conflicts if c.severity == ConflictSeverity.WARNING]
        infos = [c for c in self.conflicts if c.severity == ConflictSeverity.INFO]

        if errors:
            lines.append(f"🚫 ERRORS: {len(errors)} (code generation blocked)")
            for i, conflict in enumerate(errors, 1):
                lines.append(f"\n[E{i}] {conflict.type.value.upper()}")
                lines.append(f"    {conflict.description}")
                lines.append(f"    Affects: {', '.join(conflict.affected_requirements)}")
                if verbose:
                    lines.append(f"    Resolutions:")
                    for j, strategy in enumerate(conflict.resolution_strategies, 1):
                        lines.append(f"      {j}) {strategy}")

        if warnings:
            lines.append(f"\n⚠️  WARNINGS: {len(warnings)}")
            for i, conflict in enumerate(warnings, 1):
                lines.append(f"\n[W{i}] {conflict.type.value.upper()}")
                lines.append(f"    {conflict.description}")
                lines.append(f"    Affects: {', '.join(conflict.affected_requirements)}")
                if verbose:
                    lines.append(f"    Resolutions:")
                    for j, strategy in enumerate(conflict.resolution_strategies, 1):
                        lines.append(f"      {j}) {strategy}")

        if infos:
            lines.append(f"\nℹ️  INFO: {len(infos)}")
            if verbose:
                for i, conflict in enumerate(infos, 1):
                    lines.append(f"\n[I{i}] {conflict.type.value.upper()}")
                    lines.append(f"    {conflict.description}")

        lines.append(f"\n{'='*60}")

        if self.should_fail():
            lines.append("❌ Code generation BLOCKED - resolve errors first")
        else:
            lines.append("✅ Code generation allowed (review warnings)")

        lines.append(f"{'='*60}\n")

        return '\n'.join(lines)

    def auto_resolve(self) -> int:
        """Attempt to automatically resolve conflicts where possible

        Returns:
            Number of conflicts auto-resolved
        """
        resolved_count = 0

        for conflict in self.conflicts[:]:  # Copy list since we'll modify it
            if conflict.auto_resolvable:
                if conflict.type == ConflictType.BUDGET:
                    # Auto-increase resource limits
                    print(f"Auto-resolving: Increasing {conflict.metadata['resource']} "
                          f"limit to {conflict.metadata['suggested_limit']}")
                    self.conflicts.remove(conflict)
                    resolved_count += 1

        return resolved_count

    # Helper methods
    def _extract_resources(self, llr: Any) -> List[tuple]:
        """Extract hardware resources from LLR"""
        resources = []
        props = llr.implementation.get('properties', {})

        # ADC channels
        if 'io-channels' in props:
            channel = props['io-channels']
            # Parse "<&adc1 1>" to extract "adc1" and channel number
            resources.append(('adc', channel))

        # GPIO pins
        if 'gpios' in props:
            gpio = props['gpios']
            resources.append(('gpio', gpio))

        # CAN COB-IDs
        if 'target-id' in props and llr.implementation.get('node_type') == 'lq,cyclic-output':
            resources.append(('can_cobid', props['target-id']))

        return resources

    def _extract_timing_requirement(self, hlr: Any) -> Optional[float]:
        """Extract timing requirement from HLR text (e.g., '< 50ms' -> 50.0)"""
        import re
        # Look for patterns like "< 50ms", "within 100 milliseconds", etc.
        text = hlr.text if hasattr(hlr, 'text') else str(hlr)

        patterns = [
            r'<\s*(\d+)\s*ms',
            r'within\s+(\d+)\s*(?:ms|milliseconds)',
            r'less\s+than\s+(\d+)\s*(?:ms|milliseconds)'
        ]

        for pattern in patterns:
            match = re.search(pattern, text, re.IGNORECASE)
            if match:
                return float(match.group(1))

        return None

    def _calculate_latency(self, llrs: List[Any]) -> float:
        """Calculate worst-case latency from LLR chain"""
        # Simplified - would need proper dependency analysis
        latency = 0.0

        for llr in llrs:
            # Add engine cycle time for each processing step
            latency += 10.0  # Assume 10ms cycle time

            # Add specific delays
            if 'period-us' in llr.implementation.get('properties', {}):
                period_us = llr.implementation['properties']['period-us']
                latency += period_us / 1000.0  # Convert to ms

        return latency

    def _extract_dependencies(self, llr: Any, node_to_llr: Dict[str, str] = None) -> List[str]:
        """Extract requirement dependencies from LLR"""
        deps = []
        props = llr.implementation.get('properties', {})

        # Check for phandle references
        for key in ['source', 'input', 'command', 'verification', 'control-signal']:
            if key in props:
                # Parse "<&node_name>" to extract dependency
                dep = props[key]
                if isinstance(dep, str) and '<&' in dep:
                    # Extract node name and map to LLR ID
                    node_name = dep.strip('<&>')
                    if node_to_llr and node_name in node_to_llr:
                        deps.append(node_to_llr[node_name])
                    else:
                        # Fallback: use node name as-is (will trigger missing dependency error)
                        deps.append(node_name)

        return deps


if __name__ == "__main__":
    # Example usage
    handler = ConflictHandler(strict_mode=False)

    # Simulate some conflicts
    handler.add_conflict(Conflict(
        severity=ConflictSeverity.ERROR,
        type=ConflictType.RESOURCE,
        description="LLR-1.1 and LLR-2.1 both use ADC1 channel 1",
        affected_requirements=["LLR-1.1", "LLR-2.1"],
        resolution_strategies=[
            "Change LLR-2.1 to use ADC1 channel 2",
            "Add multiplexer"
        ]
    ))

    handler.add_conflict(Conflict(
        severity=ConflictSeverity.WARNING,
        type=ConflictType.TIMING,
        description="HLR-1 requires <50ms but implementation takes ~80ms",
        affected_requirements=["HLR-1", "LLR-1.1", "LLR-1.2"],
        resolution_strategies=[
            "Reduce cycle time to 5ms",
            "Relax HLR-1 to <100ms"
        ]
    ))

    print(handler.report(verbose=True))
    print(f"\nShould fail: {handler.should_fail()}")
