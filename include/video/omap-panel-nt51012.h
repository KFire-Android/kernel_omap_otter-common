#ifndef __OMAP_NT51012_DSI_PANEL_H
#define __OMAP_NT51012_DSI_PANEL_H

/* Result is:
 *	mem[reg] = (val) | (mem[reg] & !clr) | (set);
 *
 * Example 1: Write value
 *	clr = 0x00 && set = 0x00 && val = new value
 *
 * Example 2: Clear bits
 *	val = 0x00 && set = 0x00 && clr = clear bits
 *
 * Example 3: Set bits
 *	val = 0x00 && clr = 0x00 && set = set bits
 *
 * Example 4: Finish
 *	clr = 0xFF && set = 0xFF
 *
 */
struct nt51012_pmic_regs {
	u8 reg, val, clr, set;
};

#define NT51012_REGS(_reg, _val, _clr, _set) \
	{ .reg = _reg, .val = _val, .clr = _clr, .set = _set }

/**
 * struct nt51012_pmic_data - NT51012 PMIC driver configuration
 * @pwr_ctrl: Power control mask
 */
struct nt51012_pmic_data {
	struct nt51012_pmic_regs *fw_enable;
	struct nt51012_pmic_regs *fw_disable;
};

#endif /* __OMAP_NT51012_DSI_PANEL_H */
