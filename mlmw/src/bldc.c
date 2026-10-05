#include "mlmw/bldc.h"

int8_t mlmw_hall_to_sector(uint8_t hall)
{
	/* index = hall code, value = sector */
	static const int8_t map[8] = {-1, 0, 2, 1, 4, 5, 3, -1};

	if (hall > 7u) {
		return -1;
	}
	return map[hall];
}

uint8_t mlmw_commutation(int8_t sector, bool forward)
{
	static const uint8_t fwd[6] = {
		MLMW_AH | MLMW_BL, MLMW_AH | MLMW_CL, MLMW_BH | MLMW_CL,
		MLMW_BH | MLMW_AL, MLMW_CH | MLMW_AL, MLMW_CH | MLMW_BL,
	};
	static const uint8_t rev[6] = {
		MLMW_BH | MLMW_AL, MLMW_CH | MLMW_AL, MLMW_CH | MLMW_BL,
		MLMW_AH | MLMW_BL, MLMW_AH | MLMW_CL, MLMW_BH | MLMW_CL,
	};

	if (sector < 0 || sector > 5) {
		return 0;
	}
	return forward ? fwd[sector] : rev[sector];
}

uint32_t mlmw_speed_rpm(uint32_t hall_period_us, uint8_t pole_pairs)
{
	if (hall_period_us == 0u || pole_pairs == 0u) {
		return 0;
	}
	return (uint32_t)(60000000ull / ((uint64_t)hall_period_us * 6u * pole_pairs));
}

int32_t mlmw_ramp(int32_t current, int32_t target, int32_t max_step)
{
	int64_t diff = (int64_t)target - current;

	if (max_step <= 0) {
		return current;
	}
	if (diff > max_step) {
		return current + max_step;
	}
	if (diff < -(int64_t)max_step) {
		return current - max_step;
	}
	return target;
}

static int32_t clamp32(int64_t v, int32_t lo, int32_t hi)
{
	if (v > hi) {
		return hi;
	}
	if (v < lo) {
		return lo;
	}
	return (int32_t)v;
}

void mlmw_pi_reset(mlmw_pi_t *pi)
{
	pi->integ = 0;
}

int32_t mlmw_pi_step(mlmw_pi_t *pi, int32_t err)
{
	int64_t p = ((int64_t)pi->kp_q15 * err) >> 15;
	int64_t inc = ((int64_t)pi->ki_q15 * err) >> 15;
	int64_t u = p + pi->integ;
	bool sat_hi = u >= pi->out_max;
	bool sat_lo = u <= pi->out_min;

	if (!((sat_hi && err > 0) || (sat_lo && err < 0))) {
		pi->integ = clamp32((int64_t)pi->integ + inc, pi->out_min, pi->out_max);
	}
	return clamp32(p + pi->integ, pi->out_min, pi->out_max);
}

void mlmw_fault_init(mlmw_fault_t *f, uint16_t oc_limit_ma, uint8_t oc_debounce,
		     uint32_t stall_timeout_ms, uint32_t now_ms)
{
	f->oc_limit_ma = oc_limit_ma;
	f->oc_debounce = oc_debounce;
	f->stall_timeout_ms = stall_timeout_ms;
	f->oc_count = 0;
	f->last_edge_ms = now_ms;
	f->code = MLMW_FAULT_NONE;
}

uint8_t mlmw_fault_update(mlmw_fault_t *f, uint16_t current_ma, uint32_t now_ms,
			  bool hall_edge, bool running)
{
	if (f->code != MLMW_FAULT_NONE) {
		return f->code;
	}
	if (current_ma > f->oc_limit_ma) {
		if (f->oc_count < 0xFFu) {
			f->oc_count++;
		}
		if (f->oc_count >= f->oc_debounce) {
			f->code = MLMW_FAULT_OVERCURRENT;
			return f->code;
		}
	} else {
		f->oc_count = 0;
	}
	if (hall_edge || !running) {
		f->last_edge_ms = now_ms;
	} else if ((uint32_t)(now_ms - f->last_edge_ms) > f->stall_timeout_ms) {
		f->code = MLMW_FAULT_STALL;
	}
	return f->code;
}

void mlmw_fault_clear(mlmw_fault_t *f, uint32_t now_ms)
{
	f->code = MLMW_FAULT_NONE;
	f->oc_count = 0;
	f->last_edge_ms = now_ms;
}
