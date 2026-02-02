"""Core code generator (lq_generated.h and lq_generated.c)."""

from typing import Dict, List
from generators.base import Generator
from dts_parser import DTSNode


class CoreGenerator(Generator):
    """Generates lq_generated.h and lq_generated.c with engine struct and ISRs."""
    
    def generate(self, nodes: List[DTSNode], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate core header and implementation.
        
        Args:
            nodes: List of DTSNode objects
            counts: Resource counts dict
            
        Returns:
            {'lq_generated.h': header_content, 'lq_generated.c': source_content}
        """
        header = self._generate_header(nodes)
        source = self._generate_source(nodes, counts)
        
        return {
            'lq_generated.h': header,
            'lq_generated.c': source,
        }
    
    def _generate_header(self, nodes: List[DTSNode]) -> str:
        """Generate lq_generated.h with declarations."""
        # Collect hardware inputs for ISR declarations
        hw_inputs = [n for n in nodes if n.compatible.startswith('lq,hw-')]
        
        # Collect fault monitors for wake function declarations
        fault_monitors = [n for n in nodes if n.compatible == 'lq,fault-monitor']
        
        content = self._header("Forward declarations and extern definitions")
        content += """#ifndef LQ_GENERATED_H_
#define LQ_GENERATED_H_

#include "lq_engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Engine instance */
extern struct lq_engine g_lq_engine;

/* Initialization function */
int lq_generated_init(void);

/* Output event dispatcher */
void lq_generated_dispatch_outputs(void);

"""
        
        # Add ISR handler declarations
        if hw_inputs:
            content += "/* Hardware ISR handlers */\n"
            for hw in hw_inputs:
                if 'adc' in hw.compatible:
                    content += f"void lq_adc_isr_{hw.label}(uint16_t value);\n"
                elif 'spi' in hw.compatible:
                    content += f"void lq_spi_isr_{hw.label}(int32_t value);\n"
            content += "\n"
        
        # Add fault wake function declarations
        if fault_monitors:
            wake_functions = set()
            for fm in fault_monitors:
                wake_fn = fm.properties.get('wake_function')
                if wake_fn:
                    wake_functions.add(wake_fn)
            
            if wake_functions:
                content += "/* Fault monitor wake callbacks */\n"
                for wake_fn in sorted(wake_functions):
                    content += f"void {wake_fn}(uint8_t monitor_id, int32_t input_value, enum lq_fault_level fault_level);\n"
                content += "\n"
        
        content += """#ifdef __cplusplus
}
#endif

#endif /* LQ_GENERATED_H_ */
"""
        
        return content
    
    def _generate_source(self, nodes: List[DTSNode], counts: Dict[str, int]) -> str:
        """Generate lq_generated.c with engine struct and ISRs."""
        # TODO: Extract full implementation from dts_gen.py
        # For now, return a minimal stub that will be expanded
        
        content = self._header("Complete engine struct initialization and ISR handlers")
        content += """#include "lq_generated.h"
#include "lq_hw_input.h"
#include "lq_common.h"
#include "lq_event.h"
#include "lq_hil.h"
#include <stdlib.h>
#include <string.h>

/* TODO: Full implementation to be extracted from dts_gen.py */
/* This includes:
 * - Engine instance with inline array initialization
 * - ISR handlers for hardware inputs
 * - Output dispatch function
 * - Initialization function
 */

struct lq_engine g_lq_engine = {
    .num_signals = """ + str(counts.get('num_signals', 0) if counts else 0) + """,
    /* TODO: Complete initialization */
};

int lq_generated_init(void) {
    /* TODO: Full initialization */
    return lq_engine_init(&g_lq_engine);
}

void lq_generated_dispatch_outputs(void) {
    /* TODO: Output dispatch */
}
"""
        
        return content
