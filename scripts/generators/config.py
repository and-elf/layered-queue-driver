"""Configuration header generator (lq_config.h)."""

from typing import Dict, List
from generators.base import Generator
from dts_parser import DTSNode


class ConfigGenerator(Generator):
    """Generates lq_config.h with exact resource counts from devicetree."""
    
    def generate(self, nodes: List[DTSNode], counts: Dict[str, int] = None) -> Dict[str, str]:
        """
        Generate configuration header.
        
        Args:
            nodes: List of DTSNode objects (unused, counts are pre-calculated)
            counts: Resource counts dict
            
        Returns:
            {'lq_config.h': content}
        """
        if counts is None:
            raise ValueError("ConfigGenerator requires counts dict")
        
        # Calculate memory savings vs default Kconfig values
        default_signals = 32
        signal_saving_pct = int((1 - counts['num_signals'] / default_signals) * 100) \
            if counts['num_signals'] < default_signals else 0
        
        content = self._header("Exact resource counts based on your devicetree")
        content += f"""/*
 * No manual Kconfig tuning needed - memory automatically optimized!
 * 
 * Signal array memory: {counts['num_signals']} signals (vs default {default_signals})
 * Savings: ~{signal_saving_pct}% reduction in static allocation
 */

#ifndef LQ_CONFIG_H_
#define LQ_CONFIG_H_

/* Signal counts - auto-calculated from DTS */
#define LQ_MAX_SIGNALS              {counts['num_signals']}

/* Driver instance counts - exact counts from DTS */
#define LQ_MAX_HW_INPUTS            {counts['num_hw_inputs']}
#define LQ_MAX_SCALES               {counts['num_scales']}
#define LQ_MAX_REMAPS               {counts['num_remaps']}
#define LQ_MAX_MERGES               {counts['num_merges']}
#define LQ_MAX_FAULT_MONITORS       {counts['num_fault_monitors']}
#define LQ_MAX_CYCLIC_OUTPUTS       {counts['num_cyclic_outputs']}
#define LQ_MAX_PID_CONTROLLERS      {counts['num_pid_controllers']}
#define LQ_MAX_VERIFIED_OUTPUTS     {counts['num_verified_outputs']}

/* System health monitoring - devices that register init/runtime status */
#define LQ_MAX_HEALTH_DEVICES       {counts['num_health_devices']}

/* Buffer sizes - calculated from actual usage */
#define LQ_MAX_MERGE_INPUTS         {counts['max_merge_inputs']}
#define LQ_MAX_OUTPUT_EVENTS        {counts['max_output_events']}
#define LQ_HW_RINGBUFFER_SIZE       {counts['hw_ringbuffer_size']}

/* DTS generation metadata */
#define LQ_CONFIG_FROM_DTS          1
#define LQ_CONFIG_SIGNAL_COUNT      {counts['num_signals']}

#endif /* LQ_CONFIG_H_ */
"""
        
        return {'lq_config.h': content}
