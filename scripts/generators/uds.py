"""UDS code generator (lq_generated_uds.h and lq_generated_uds.c)."""

from typing import Dict, List
from generators.base import Generator
from dts_parser import DTSNode


class UDSGenerator(Generator):
    """Generates UDS DID handler table and read/write functions."""
    
    def generate(self, nodes: List[DTSNode], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate UDS handler code.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts dict (unused)
            
        Returns:
            {'lq_generated_uds.h': header_content, 'lq_generated_uds.c': source_content}
            or {} if no UDS exposures exist
        """
        exposures = self._parse_uds_exposures(nodes)
        
        if not exposures:
            return {}  # No UDS exposures, don't generate files
        
        header = self._generate_header(exposures)
        source = self._generate_source(exposures, nodes)
        
        return {
            'lq_generated_uds.h': header,
            'lq_generated_uds.c': source,
        }
    
    def _parse_uds_exposures(self, nodes: List[DTSNode]) -> List[Dict]:
        """Parse UDS protocol nodes and extract what should be exposed via DIDs."""
        uds_nodes = [n for n in nodes if n.compatible == 'lq,protocol-uds']
        
        if not uds_nodes:
            return []
        
        # Build label->node map for phandle resolution
        label_map = {node.label: node for node in nodes}
        
        exposures = []
        
        for uds_node in uds_nodes:
            for prop_name, prop_value in uds_node.properties.items():
                # Skip non-exposure properties
                if prop_name in ['compatible', 'can_device', 'label']:
                    continue
                
                # Property name pattern: expose_<label>_<type>
                if prop_name.startswith('expose_'):
                    parts = prop_name.split('_')
                    if len(parts) < 3:
                        continue
                    
                    # Extract target label and operation
                    target_label = '_'.join(parts[1:-1])
                    operation = parts[-1]  # 'read' or 'write'
                    
                    if target_label not in label_map:
                        print(f"Warning: UDS exposes unknown target: {target_label}")
                        continue
                    
                    # Find or create exposure entry
                    exposure = None
                    for exp in exposures:
                        if exp['target_label'] == target_label:
                            exposure = exp
                            break
                    
                    if not exposure:
                        exposure = {
                            'target_label': target_label,
                            'target_node': label_map[target_label],
                            'did_read': None,
                            'did_write': None,
                            'writable': False,
                            'read_only': False,
                            'security_level': 0,
                            'description': f"UDS access to {target_label}",
                        }
                        exposures.append(exposure)
                    
                    # Set DID based on operation
                    if operation == 'read':
                        exposure['did_read'] = prop_value
                    elif operation == 'write':
                        exposure['did_write'] = prop_value
                        exposure['writable'] = True
        
        return exposures
    
    def _generate_header(self, exposures: List[Dict]) -> str:
        """Generate UDS header with DID definitions."""
        content = self._header("UDS DID handler table for accessing driver parameters")
        content += """#ifndef LQ_GENERATED_UDS_H_
#define LQ_GENERATED_UDS_H_

#include "lq_uds.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* UDS DID definitions (auto-generated from DTS) */
"""
        
        # Generate DID constants
        for exp in exposures:
            label_upper = exp['target_label'].upper()
            if exp['did_read'] is not None:
                content += f"#define UDS_DID_{label_upper}_READ  0x{exp['did_read']:04X}\n"
            if exp['did_write'] is not None:
                content += f"#define UDS_DID_{label_upper}_WRITE 0x{exp['did_write']:04X}\n"
        
        content += """
/* UDS handler function */
int lq_generated_uds_handler(uint16_t did, bool is_write,
                             const uint8_t *in_data, size_t in_len,
                             uint8_t *out_data, size_t *out_len);

#ifdef __cplusplus
}
#endif

#endif /* LQ_GENERATED_UDS_H_ */
"""
        
        return content
    
    def _generate_source(self, exposures: List[Dict], nodes: List[DTSNode]) -> str:
        """Generate UDS implementation with DID handlers."""
        content = self._header("UDS DID handler implementation")
        content += """#include "lq_generated_uds.h"
#include "lq_generated.h"
#include "lq_scale.h"
#include "lq_remap.h"
#include "lq_pid.h"
#include "lq_engine.h"
#include <string.h>

/* External driver references */
extern struct lq_engine g_lq_engine;

int lq_generated_uds_handler(uint16_t did, bool is_write,
                              const uint8_t *in_data, size_t in_len,
                              uint8_t *out_data, size_t *out_len)
{
    (void)in_data;
    (void)in_len;
    
    switch (did) {
"""
        
        # Generate cases for each exposure
        for exp in exposures:
            target_node = exp['target_node']
            compatible = target_node.compatible
            label_upper = exp['target_label'].upper()
            
            # Read handler
            if exp['did_read'] is not None:
                content += f"        case UDS_DID_{label_upper}_READ:\n"
                content += f"            if (is_write) return -1;  /* Read-only DID */\n"
                content += f"            /* TODO: Implement read for {exp['target_label']} ({compatible}) */\n"
                content += f"            return -1;\n\n"
            
            # Write handler
            if exp['did_write'] is not None and exp['writable']:
                content += f"        case UDS_DID_{label_upper}_WRITE:\n"
                content += f"            if (!is_write) return -1;  /* Write-only DID */\n"
                content += f"            /* TODO: Implement write for {exp['target_label']} */\n"
                content += f"            return -1;\n\n"
        
        content += """        default:
            return -1;  /* Unknown DID */
    }
}
"""
        
        return content
