#!/usr/bin/env python3
"""
Requirements-Driven Code Generator
Generates DTS, HIL tests, and documentation from requirements
"""

import sys
import argparse
from pathlib import Path
from typing import List, Dict, Any
from conflict_handler import ConflictHandler, ConflictSeverity
import json


class RequirementGenerator:
    """Main generator orchestrating requirement processing"""

    def __init__(self, req_dir: Path, config: Dict[str, Any] = None):
        self.req_dir = Path(req_dir)
        self.config = config or {}
        self.conflict_handler = ConflictHandler(
            strict_mode=self.config.get('strict_mode', False)
        )
        self.hlrs = []
        self.llrs = []

    def load_requirements(self):
        """Load all requirements from directory structure"""
        print("Loading requirements...")

        # Load high-level requirements (markdown)
        hlr_dir = self.req_dir / "high-level"
        if hlr_dir.exists():
            self.hlrs = self._load_hlr_files(hlr_dir)
            print(f"  Loaded {len(self.hlrs)} high-level requirements")

        # Load low-level requirements (YAML)
        llr_dir = self.req_dir / "low-level"
        if llr_dir.exists():
            self.llrs = self._load_llr_files(llr_dir)
            # Apply replaced_ids: treat files that are declared replaced as archived
            self._apply_replaced_ids()
            print(f"  Loaded {len(self.llrs)} low-level requirements")

    def validate(self, auto_fix: bool = False) -> bool:
        """Validate requirements and detect conflicts

        Args:
            auto_fix: Attempt to auto-resolve conflicts where possible

        Returns:
            True if validation passed (or only warnings), False if errors
        """
        print("\nValidating requirements...")

        # Check coverage (all HLRs have LLRs)
        self._check_coverage()

        # Check for conflicts
        self._detect_conflicts()

        # Auto-resolve if requested
        if auto_fix:
            resolved = self.conflict_handler.auto_resolve()
            if resolved:
                print(f"Auto-resolved {resolved} conflicts")

        # Print report
        print(self.conflict_handler.report(verbose=True))

        return not self.conflict_handler.should_fail()

    def generate_dts(self, output_file: Path, force: bool = False):
        """Generate app.dts from low-level requirements

        Args:
            output_file: Path to write generated DTS
            force: Generate even if validation fails
        """
        if not force and self.conflict_handler.should_fail():
            print("❌ Cannot generate DTS - validation failed")
            print("   Use --force to generate anyway (not recommended)")
            return False

        print(f"\nGenerating DTS: {output_file}")

        # Generate DTS content
        dts_content = self._generate_dts_content()

        # Write to file
        output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(output_file, 'w') as f:
            f.write(dts_content)

        print(f"✅ Generated {output_file}")
        return True

    def generate_tests(self, output_dir: Path):
        """Generate HIL test code from high-level requirements"""
        print(f"\nGenerating HIL tests: {output_dir}")

        output_dir.mkdir(parents=True, exist_ok=True)

        for hlr in self.hlrs:
            test_file = output_dir / f"test_{hlr['id'].lower().replace('-', '_')}.c"
            test_content = self._generate_test_content(hlr)

            with open(test_file, 'w') as f:
                f.write(test_content)

            print(f"  Generated {test_file.name}")

        print(f"✅ Generated {len(self.hlrs)} test files")

    def generate_documentation(self, output_file: Path):
        """Generate requirements traceability document"""
        print(f"\nGenerating documentation: {output_file}")

        doc_content = self._generate_documentation_content()

        output_file.parent.mkdir(parents=True, exist_ok=True)
        with open(output_file, 'w') as f:
            f.write(doc_content)

        print(f"✅ Generated {output_file}")

    # Internal methods
    def _load_hlr_files(self, hlr_dir: Path) -> List[Dict]:
        """Load high-level requirements from markdown files"""
        hlrs = []
        for md_file in sorted(hlr_dir.glob("*.md")):
            hlr = self._parse_hlr_markdown(md_file)
            if hlr:
                hlrs.append(hlr)
        return hlrs

    def _load_llr_files(self, llr_dir: Path) -> List[Dict]:
        """Load low-level requirements from YAML files"""
        import yaml

        llrs = []
        for yaml_file in sorted(llr_dir.glob("*.yaml")):
            with open(yaml_file) as f:
                llr = yaml.safe_load(f)
                llr['_file'] = yaml_file
                llrs.append(llr)
        return llrs

    def _apply_replaced_ids(self):
        """Handle `replaced_ids` metadata: remove replaced LLRs from active set.

        If an LLR declares `replaced_ids: [ID...]`, any LLRs with those IDs
        will be treated as archived/removed and excluded from further processing.
        This preserves provenance while avoiding duplicate/conflicting entries.
        """
        id_to_llr = {llr.get('id'): llr for llr in self.llrs}
        to_remove = set()

        for llr in list(self.llrs):
            replaced = llr.get('replaced_ids') or []
            if replaced:
                print(f"Info: {llr.get('id')} declares replaced_ids: {replaced}")
                for rid in replaced:
                    if rid in id_to_llr:
                        to_remove.add(rid)
                        # annotate replaced entry with provenance
                        id_to_llr[rid]['_replaced_by'] = llr.get('id')

        if to_remove:
            before = len(self.llrs)
            self.llrs = [llr for llr in self.llrs if llr.get('id') not in to_remove]
            after = len(self.llrs)
            print(f"Info: Archived {before-after} replaced LLR(s): {sorted(list(to_remove))}")

    def _parse_hlr_markdown(self, md_file: Path) -> Dict:
        """Parse high-level requirement from markdown"""
        with open(md_file) as f:
            content = f.read()

        # Simple parser - extract ID and content
        import re
        id_match = re.search(r'\*\*ID:\*\*\s+(\S+)', content)
        priority_match = re.search(r'\*\*Priority:\*\*\s+(\w+)', content)

        if id_match:
            return {
                'id': id_match.group(1),
                'file': md_file,
                'text': content,
                'priority': priority_match.group(1) if priority_match else 'Normal'
            }
        return None

    def _check_coverage(self):
        """Check that all HLRs are implemented by LLRs"""
        for hlr in self.hlrs:
            children = [llr for llr in self.llrs if llr.get('parent') == hlr['id']]
            if not children:
                print(f"⚠️  {hlr['id']} has no implementing LLRs")

    def _detect_conflicts(self):
        """Run all conflict detection checks"""
        # Resource conflicts
        resource_conflicts = self.conflict_handler.check_resource_conflict(
            [self._llr_to_object(llr) for llr in self.llrs]
        )
        for conflict in resource_conflicts:
            self.conflict_handler.add_conflict(conflict)

        # Timing conflicts
        timing_conflicts = self.conflict_handler.check_timing_conflict(
            [self._hlr_to_object(hlr) for hlr in self.hlrs],
            [self._llr_to_object(llr) for llr in self.llrs]
        )
        for conflict in timing_conflicts:
            self.conflict_handler.add_conflict(conflict)

        # Dependency conflicts
        dep_conflicts = self.conflict_handler.check_dependency_conflict(
            [self._llr_to_object(llr) for llr in self.llrs]
        )
        for conflict in dep_conflicts:
            self.conflict_handler.add_conflict(conflict)

        # Budget conflicts (example limits)
        budget_conflicts = self.conflict_handler.check_budget_conflict(
            [self._llr_to_object(llr) for llr in self.llrs],
            limits={'signals': 32, 'cyclic_outputs': 16, 'merges': 8}
        )
        for conflict in budget_conflicts:
            self.conflict_handler.add_conflict(conflict)

    def _generate_dts_content(self) -> str:
        """Generate complete DTS file content"""
        lines = []
        lines.append("// Auto-generated from requirements")
        lines.append("// DO NOT EDIT - Regenerate with: reqgen generate-dts\n")
        lines.append("/ {")

        # Merge LLRs that configure the same node
        merged_nodes = self._merge_llr_nodes()

        # Generate engine node first
        if 'engine' in merged_nodes:
            lines.append(self._merged_node_to_dts(merged_nodes['engine'], indent=1))

        # Generate all other nodes in dependency order
        sorted_nodes = self._topological_sort_nodes(merged_nodes)
        for node_name, node_data in sorted_nodes.items():
            if node_name != 'engine':  # Skip engine (already added)
                lines.append(self._merged_node_to_dts(node_data, indent=1))

        lines.append("};")
        return '\n'.join(lines) + '\n'

    def _llr_to_dts_node(self, llr: Dict, indent: int = 0) -> str:
        """Convert LLR to DTS node syntax"""
        impl = llr['implementation']
        node_name = impl['node_name']
        node_type = impl['node_type']
        props = impl.get('properties', {})

        ind = '    ' * indent
        lines = []

        # Node header with label
        lines.append(f"{ind}/* {llr['title']} - {llr['id']} */")
        lines.append(f"{ind}{node_name}: {node_name.replace('_', '-')} {{")

        # Compatible
        lines.append(f"{ind}    compatible = \"{node_type}\";")

        # Properties
        for key, value in props.items():
            lines.append(f"{ind}    {key} = {self._format_dts_value(value)};")

        lines.append(f"{ind}}}\n")
        return '\n'.join(lines)

    def _format_dts_value(self, value: Any) -> str:
        """Format value for DTS syntax"""
        if isinstance(value, str):
            if value.startswith('<&'):
                return value  # Phandle reference
            return f'"{value}"'
        elif isinstance(value, int):
            return f"<{value}>"
        elif isinstance(value, list):
            return f"<{' '.join(str(v) for v in value)}>"
        return str(value)

    def _generate_test_content(self, hlr: Dict) -> str:
        """Generate HIL test skeleton from HLR"""
        test_id = hlr['id'].replace('-', '_')

        content = f"""/*
 * Auto-generated HIL test for {hlr['id']}
 * Generated from: {hlr['file'].name}
 */

#include <zephyr/ztest.h>
#include "hil_test_framework.h"

/**
 * Test: {hlr['id']}
 * {hlr['text'].split('## Requirement')[1].split('##')[0].strip() if '## Requirement' in hlr['text'] else 'TODO: Add description'}
 */
ZTEST(hlr_tests, test_{test_id.lower()})
{{
    // TODO: Implement test based on verification criteria
    // See {hlr['file'].name} for details

    zassert_true(false, "Test not yet implemented");
}}

ZTEST_SUITE(hlr_tests, NULL, NULL, NULL, NULL, NULL);
"""
        return content

    def _generate_documentation_content(self) -> str:
        """Generate requirements traceability document"""
        lines = []
        lines.append("# Requirements Traceability Matrix\n")
        lines.append("Auto-generated requirements documentation\n")

        for hlr in self.hlrs:
            lines.append(f"## {hlr['id']}\n")

            # Find implementing LLRs
            children = [llr for llr in self.llrs if llr.get('parent') == hlr['id']]

            lines.append(f"**Implemented by:** {', '.join(llr['id'] for llr in children)}\n")

            for llr in children:
                lines.append(f"### {llr['id']}: {llr['title']}\n")
                lines.append(f"- **Type:** {llr['implementation']['node_type']}")
                lines.append(f"- **Node:** `{llr['implementation']['node_name']}`")
                lines.append(f"- **File:** `{llr.get('_file', 'unknown')}`\n")

        return '\n'.join(lines)

    def _find_engine_config(self) -> Dict:
        """Find engine configuration LLR"""
        for llr in self.llrs:
            if llr['implementation']['node_type'] == 'lq,engine':
                return llr
        return None

    def _merge_llr_nodes(self) -> Dict[str, Dict]:
        """Merge multiple LLRs that configure the same DTS node

        Returns:
            Dictionary mapping node_name -> merged node data
            Each merged node contains:
                - node_name: str
                - node_type: str
                - properties: dict (merged from all LLRs)
                - llr_ids: list (which LLRs contributed)
                - comments: list (description from each LLR)
        """
        nodes = {}

        for llr in self.llrs:
            impl = llr['implementation']
            node_name = impl['node_name']
            node_type = impl['node_type']
            props = impl.get('properties', {})

            if node_name not in nodes:
                # First LLR for this node
                nodes[node_name] = {
                    'node_name': node_name,
                    'node_type': node_type,
                    'properties': {},
                    'llr_ids': [],
                    'comments': []
                }

            # Check node type matches
            if nodes[node_name]['node_type'] != node_type:
                print(f"⚠️  WARNING: {llr['id']} has different node_type for {node_name}")
                print(f"   Expected: {nodes[node_name]['node_type']}, Got: {node_type}")

            # Merge properties
            for key, value in props.items():
                if key in nodes[node_name]['properties']:
                    # Property already exists - check for conflicts
                    existing = nodes[node_name]['properties'][key]
                    if existing != value:
                        print(f"⚠️  WARNING: Property conflict on {node_name}.{key}")
                        print(f"   {nodes[node_name]['llr_ids'][-1]}: {existing}")
                        print(f"   {llr['id']}: {value}")
                        print(f"   Using value from {llr['id']}")

                # Add or overwrite property
                nodes[node_name]['properties'][key] = value

            # Track which LLRs contributed
            nodes[node_name]['llr_ids'].append(llr['id'])
            nodes[node_name]['comments'].append(f"{llr['title']} - {llr['id']}")

        return nodes

    def _transform_to_v2_api(self, node_type: str, props: Dict) -> tuple:
        """Transform old hardware-specific nodes to new generic API (v2.0)

        Transforms:
        - lq,hw-adc-input → lq,input with input = <&adc ...>
        - lq,hw-spi-input → lq,input with input = <&spi ...>
        - lq,hw-sensor-input → lq,input with input = <&sensor ...>
        - lq,cyclic-output → lq,output with output = <...>

        Returns:
            Tuple of (new_node_type, new_props)
        """
        new_props = props.copy()

        # Transform hardware inputs to generic lq,input
        if node_type == 'lq,hw-adc-input':
            # io-channels = <&adc1 1> → input = <&adc1 1>
            if 'io-channels' in new_props:
                new_props['input'] = new_props.pop('io-channels')
            # Remove hardware-specific properties (handled by hardware node)
            new_props.pop('min-raw', None)
            new_props.pop('max-raw', None)
            new_props.pop('isr-priority', None)
            new_props.pop('init-priority', None)
            return 'lq,input', new_props

        elif node_type == 'lq,hw-spi-input':
            # spi-device = <&spi0> → input = <&spi0>
            if 'spi-device' in new_props:
                new_props['input'] = new_props.pop('spi-device')
            # Remove hardware-specific properties
            new_props.pop('isr-priority', None)
            new_props.pop('init-priority', None)
            return 'lq,input', new_props

        elif node_type == 'lq,hw-sensor-input':
            # sensor-device = <&tmp117> → input = <&tmp117 channel>
            if 'sensor-device' in new_props and 'sensor-channel' in new_props:
                sensor = new_props.pop('sensor-device')
                channel = new_props.pop('sensor-channel')
                # Combine into phandle-array: <&sensor channel>
                new_props['input'] = f"{sensor} {channel}"
            # Keep stale-us, remove other hw-specific props
            new_props.pop('trigger-type', None)
            new_props.pop('poll-interval-ms', None)
            new_props.pop('scale-factor', None)
            new_props.pop('offset', None)
            new_props.pop('init-priority', None)
            return 'lq,input', new_props

        # Transform outputs to generic lq,output
        elif node_type == 'lq,cyclic-output':
            # Extract hardware reference from output-type
            output_type = new_props.get('output-type')
            if output_type:
                # For now, keep lq,cyclic-output as-is
                # Full transformation would require knowing which hardware peripheral to reference
                # This requires more context from the DTS or requirements
                pass
            return node_type, new_props

        # No transformation needed
        return node_type, new_props

    def _merged_node_to_dts(self, node_data: Dict, indent: int = 0) -> str:
        """Convert merged node data to DTS node syntax"""
        node_name = node_data['node_name']
        node_type = node_data['node_type']
        props = node_data['properties'].copy()  # Copy to avoid modifying original
        comments = node_data['comments']
        llr_ids = node_data['llr_ids']

        # Transform old API to new API (v2.0)
        node_type, props = self._transform_to_v2_api(node_type, props)

        ind = '    ' * indent
        lines = []

        # Node header with comments showing all contributing LLRs
        if len(llr_ids) > 1:
            lines.append(f"{ind}/* {node_name} - configured by: {', '.join(llr_ids)} */")
            for comment in comments:
                lines.append(f"{ind}/*   - {comment} */")
        else:
            lines.append(f"{ind}/* {comments[0]} */")

        lines.append(f"{ind}{node_name}: {node_name.replace('_', '-')} {{")

        # Compatible
        lines.append(f"{ind}    compatible = \"{node_type}\";")

        # Properties (sorted for consistency, exclude 'compatible' as it's handled above)
        for key in sorted(props.keys()):
            if key == 'compatible':
                continue  # Already added above
            value = props[key]
            lines.append(f"{ind}    {key} = {self._format_dts_value(value)};")

        lines.append(f"{ind}}}\n")
        return '\n'.join(lines)

    def _topological_sort_nodes(self, nodes: Dict[str, Dict]) -> Dict[str, Dict]:
        """Sort nodes in dependency order"""
        # Simplified - just return in sorted order by name
        # Proper implementation would analyze dependencies
        return {k: nodes[k] for k in sorted(nodes.keys()) if k != 'engine'}

    def _topological_sort(self) -> List[Dict]:
        """Sort LLRs in dependency order (deprecated - use _merge_llr_nodes)"""
        # Simplified - proper implementation would use graph traversal
        return sorted(self.llrs, key=lambda x: x.get('id', ''))

    def _llr_to_object(self, llr: Dict):
        """Convert LLR dict to object for conflict handler"""
        class LLRObject:
            def __init__(self, data):
                self.id = data['id']
                self.parent = data.get('parent')
                self.implementation = data['implementation']

        return LLRObject(llr)

    def _hlr_to_object(self, hlr: Dict):
        """Convert HLR dict to object for conflict handler"""
        class HLRObject:
            def __init__(self, data):
                self.id = data['id']
                self.text = data['text']

        return HLRObject(hlr)

    def discover_board(self) -> Dict[str, Any]:
        """Discover target board and optional overlay from low-level requirements.

        Searches for a `board` or `overlay` declaration in several places:
        - top-level `board` key in an LLR
        - `implementation.properties.board` or `implementation.properties.overlay`
        - `hardware.board` or `hardware.overlay`

        Returns a dict: { 'board': str or None, 'overlay': str or None }
        """
        board = None
        overlay = None

        for llr in self.llrs:
            # Top-level board
            if 'board' in llr and llr.get('board'):
                board = llr.get('board')
            # Implementation properties
            impl = llr.get('implementation', {})
            props = impl.get('properties', {}) if impl else {}
            if isinstance(props, dict):
                if not board and props.get('board'):
                    board = props.get('board')
                if not overlay and props.get('overlay'):
                    overlay = props.get('overlay')
            # Hardware section
            hw = llr.get('hardware', {})
            if isinstance(hw, dict):
                if not board and hw.get('board'):
                    board = hw.get('board')
                if not overlay and hw.get('overlay'):
                    overlay = hw.get('overlay')

        return {'board': board, 'overlay': overlay}


def main():
    parser = argparse.ArgumentParser(description='Requirements-driven code generator')
    parser.add_argument('req_dir', type=Path, help='Requirements directory')
    parser.add_argument('command', choices=['validate', 'generate-dts', 'generate-tests', 'generate-docs', 'all', 'metadata'])
    parser.add_argument('-o', '--output', type=Path, help='Output file/directory')
    parser.add_argument('--force', action='store_true', help='Generate even if validation fails')
    parser.add_argument('--auto-fix', action='store_true', help='Auto-resolve conflicts where possible')
    parser.add_argument('--strict', action='store_true', help='Treat warnings as errors')

    args = parser.parse_args()

    # Create generator
    gen = RequirementGenerator(args.req_dir, config={'strict_mode': args.strict})

    # Handle metadata command with silent low-level load (avoid extra stdout)
    if args.command == 'metadata':
        llr_dir = Path(args.req_dir) / 'low-level'
        if llr_dir.exists():
            gen.llrs = gen._load_llr_files(llr_dir)
            gen._apply_replaced_ids()
        meta = gen.discover_board()
        print(json.dumps(meta))
        sys.exit(0 if meta.get('board') else 2)

    # Load requirements for other commands
    gen.load_requirements()

    # Support metadata discovery command (load requirements but skip validation)
    if args.command == 'metadata':
        meta = gen.discover_board()
        print(json.dumps(meta))
        sys.exit(0 if meta.get('board') else 2)

    # Validate
    if args.command == 'validate':
        valid = gen.validate(auto_fix=args.auto_fix)
        sys.exit(0 if valid else 1)

    # Validate before generating
    valid = gen.validate(auto_fix=args.auto_fix)

    # Generate based on command
    if args.command == 'generate-dts' or args.command == 'all':
        output = args.output or Path('app.dts')
        gen.generate_dts(output, force=args.force)

    if args.command == 'generate-tests' or args.command == 'all':
        output = args.output or Path('tests/hil')
        gen.generate_tests(output)

    if args.command == 'generate-docs' or args.command == 'all':
        output = args.output or Path('REQUIREMENTS.md')
        gen.generate_documentation(output)

    sys.exit(0 if valid else 1)


if __name__ == '__main__':
    main()
