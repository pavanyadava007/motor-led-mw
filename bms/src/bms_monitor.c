/* Cell and pack monitoring, see bms/bms_monitor.h. */
#include "bms/bms_monitor.h"

static uint8_t count_up(uint8_t c)
{
	return (c < 255u) ? (uint8_t)(c + 1u) : c;
}

/* Advances one debounce counter and returns true when it has reached the limit. */
static bool debounce(uint8_t *ctr, bool cond, uint8_t limit)
{
	*ctr = cond ? count_up(*ctr) : 0u;
	return (*ctr >= limit);
}

void bms_mon_init(bms_mon_t *m)
{
	uint8_t k;

	for (k = 0u; k < BMS_MAX_CELLS; k++) {
		m->ov[k] = 0u;
		m->uv[k] = 0u;
		m->wire[k] = 0u;
		m->stuck[k] = 0u;
		m->last_mv[k] = 0u;
	}
	for (k = 0u; k < BMS_MAX_TEMPS; k++) {
		m->ot[k] = 0u;
		m->ut[k] = 0u;
		m->tsens[k] = 0u;
	}
	m->oc_dis = 0u;
	m->oc_chg = 0u;
	m->have_last = false;
	m->latched = 0u;
	m->active = 0u;
	m->v_min_mv = 0u;
	m->v_max_mv = 0u;
	m->t_min_dc = 0;
	m->t_max_dc = 0;
}

static bms_faults_t check_cells(bms_mon_t *m, const bms_mon_cfg_t *cfg, const uint16_t *cell_mv,
				int32_t current_ma)
{
	bms_faults_t f = 0u;
	uint8_t k;
	uint8_t n = (cfg->n_cells <= BMS_MAX_CELLS) ? cfg->n_cells : (uint8_t)BMS_MAX_CELLS;
	bool loaded = (current_ma >= cfg->stuck_min_ma) || (current_ma <= -cfg->stuck_min_ma);
	uint16_t vmin = 0xFFFFu;
	uint16_t vmax = 0u;

	for (k = 0u; k < n; k++) {
		uint16_t v = cell_mv[k];
		bool bad = (v < cfg->wire_min_mv) || (v > cfg->wire_max_mv);
		bool same = m->have_last && (v == m->last_mv[k]);

		if (bad) {
			m->active |= BMS_F_OPEN_WIRE;
		}
		if (debounce(&m->wire[k], bad, cfg->debounce)) {
			f |= BMS_F_OPEN_WIRE;
		}
		if (same && loaded) {
			m->active |= BMS_F_SENSOR_STUCK;
		}
		if (debounce(&m->stuck[k], same && loaded, cfg->stuck_samples)) {
			f |= BMS_F_SENSOR_STUCK;
		}
		m->last_mv[k] = v;
		if (bad) {
			/* an open wire is a sensor fault: not used for OV/UV */
			m->ov[k] = 0u;
			m->uv[k] = 0u;
		} else {
			if (v > cfg->ov_mv) {
				m->active |= BMS_F_CELL_OV;
			}
			if (v < cfg->uv_mv) {
				m->active |= BMS_F_CELL_UV;
			}
			if (debounce(&m->ov[k], v > cfg->ov_mv, cfg->debounce)) {
				f |= BMS_F_CELL_OV;
			}
			if (debounce(&m->uv[k], v < cfg->uv_mv, cfg->debounce)) {
				f |= BMS_F_CELL_UV;
			}
			if (v < vmin) {
				vmin = v;
			}
			if (v > vmax) {
				vmax = v;
			}
		}
	}
	m->v_min_mv = (vmax == 0u) ? 0u : vmin;
	m->v_max_mv = vmax;
	return f;
}

static bms_faults_t check_temps(bms_mon_t *m, const bms_mon_cfg_t *cfg, const int16_t *temp_dc)
{
	bms_faults_t f = 0u;
	uint8_t k;
	uint8_t n = (cfg->n_temps <= BMS_MAX_TEMPS) ? cfg->n_temps : (uint8_t)BMS_MAX_TEMPS;
	int16_t tmin = 32767;
	int16_t tmax = -32768;
	bool any = false;

	for (k = 0u; k < n; k++) {
		int16_t t = temp_dc[k];
		bool bad = (t < cfg->temp_min_dc) || (t > cfg->temp_max_dc);

		if (bad) {
			m->active |= BMS_F_TEMP_SENSOR;
		}
		if (debounce(&m->tsens[k], bad, cfg->debounce)) {
			f |= BMS_F_TEMP_SENSOR;
		}
		if (bad) {
			m->ot[k] = 0u;
			m->ut[k] = 0u;
		} else {
			if (t > cfg->ot_dc) {
				m->active |= BMS_F_CELL_OT;
			}
			if (t < cfg->ut_dc) {
				m->active |= BMS_F_CELL_UT;
			}
			if (debounce(&m->ot[k], t > cfg->ot_dc, cfg->debounce)) {
				f |= BMS_F_CELL_OT;
			}
			if (debounce(&m->ut[k], t < cfg->ut_dc, cfg->debounce)) {
				f |= BMS_F_CELL_UT;
			}
			if (t < tmin) {
				tmin = t;
			}
			if (t > tmax) {
				tmax = t;
			}
			any = true;
		}
	}
	m->t_min_dc = any ? tmin : 0;
	m->t_max_dc = any ? tmax : 0;
	return f;
}

bms_faults_t bms_mon_step(bms_mon_t *m, const bms_mon_cfg_t *cfg, const uint16_t *cell_mv,
			  const int16_t *temp_dc, int32_t current_ma)
{
	bms_faults_t f;
	bool dis = (current_ma < 0) && (-current_ma > cfg->oc_dis_ma);
	bool chg = current_ma > cfg->oc_chg_ma;

	m->active = 0u;
	f = check_cells(m, cfg, cell_mv, current_ma);
	f |= check_temps(m, cfg, temp_dc);
	if (dis) {
		m->active |= BMS_F_OC_DISCHARGE;
	}
	if (chg) {
		m->active |= BMS_F_OC_CHARGE;
	}
	if (debounce(&m->oc_dis, dis, cfg->debounce)) {
		f |= BMS_F_OC_DISCHARGE;
	}
	if (debounce(&m->oc_chg, chg, cfg->debounce)) {
		f |= BMS_F_OC_CHARGE;
	}
	m->have_last = true;
	m->latched |= f;
	return m->latched;
}

bool bms_mon_clear(bms_mon_t *m)
{
	bool ok = (m->active == 0u);

	if (ok) {
		m->latched = 0u;
	}
	return ok;
}
