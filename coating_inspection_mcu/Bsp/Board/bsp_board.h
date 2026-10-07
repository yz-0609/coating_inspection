#ifndef BSP_BOARD_H
#define BSP_BOARD_H

/* Emergency result revocation, callable before scheduler start or from fault hooks.
 * Enables GPIO clocks but does not change uninitialized pin modes. */
void BSP_BoardSafeOutputs(void);
void BSP_BoardFaultOutputs(void);

#endif
