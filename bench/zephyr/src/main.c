/* dsp-lab stage 2, board side: the cycle counter, the interrupt masking, the CRC and the USB console for bench_core. Arduino Nano 33 BLE
   (nRF52840, Cortex-M4F). Built with AI assistance (Claude).

   The cycle counter is the Cortex-M4 DWT cycle counter (DWT->CYCCNT, one count per CPU clock). Zephyr's k_cycle_get_32() is only the 32.768 kHz
   RTC on this chip and is used here just to measure the CPU clock. */
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/crc.h>
#include <zephyr/version.h>

#include <cmsis_core.h>

#include "bench_core.h"
#include "bench_variants.h"

#define REPEATS 3u
#define PAUSE_BETWEEN_RUNS_S 10

static uint32_t plat_cycles(void) { return DWT->CYCCNT; }
static unsigned plat_irq_lock(void) { return irq_lock(); }
static void plat_irq_unlock(unsigned key) { irq_unlock(key); }
static uint32_t plat_crc32(const uint8_t *data, size_t len) { return crc32_ieee(data, len); }

static void plat_emit(const char *line)
{
	printk("%s\n", line);
	k_msleep(5);                                    /* the USB serial buffer is small; do not outrun the PC */
}

/* Turns the DWT cycle counter on and proves it counts. */
static bool dwt_start(void)
{
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	if (DWT->CTRL & DWT_CTRL_NOCYCCNT_Msk) {
		return false;                               /* this core has no cycle counter */
	}
	DWT->CYCCNT = 0;
	DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
	uint32_t a = DWT->CYCCNT;
	for (volatile int i = 0; i < 16; i++) {
	}
	return DWT->CYCCNT != a;
}

/* CPU clock measured against the RTC over about 100 ms, so the cycles-per-second figure in the log is a measurement, not an assumption. */
static uint32_t measure_cpu_hz(void)
{
	uint32_t rtc_hz = sys_clock_hw_cycles_per_sec();
	uint32_t span = rtc_hz / 10u;
	uint32_t t0 = k_cycle_get_32();
	uint32_t c0 = DWT->CYCCNT;
	uint32_t dt;

	do {
		dt = k_cycle_get_32() - t0;
	} while (dt < span);
	uint32_t dc = DWT->CYCCNT - c0;

	return (uint32_t)(((uint64_t)dc * rtc_hz) / dt);
}

int main(void)
{
	static const bench_platform_t plat = {
		.cycles = plat_cycles,
		.irq_lock = plat_irq_lock,
		.irq_unlock = plat_irq_unlock,
		.crc32 = plat_crc32,
		.emit = plat_emit,
	};
	const struct device *con = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	uint32_t dtr = 0;

	if (!device_is_ready(con)) {
		return 0;
	}
	while (!dtr) {                                  /* wait until a terminal opens the port, so the first run is not lost */
		(void)uart_line_ctrl_get(con, UART_LINE_CTRL_DTR, &dtr);
		k_msleep(100);
	}
	k_msleep(1000);

	if (!dwt_start()) {
		for (;;) {
			printk("ERROR: the DWT cycle counter is not counting\n");
			k_sleep(K_SECONDS(2));
		}
	}

	for (;;) {
		printk("ENV zephyr %s\n", KERNEL_VERSION_STRING);
		printk("ENV board arduino_nano_33_ble (nRF52840, Cortex-M4F)\n");
		printk("ENV cpu_hz_measured %u\n", (unsigned)measure_cpu_hz());
		(void)bench_run(&plat, REPEATS, bench_variants, BENCH_NVARIANTS);
		k_sleep(K_SECONDS(PAUSE_BETWEEN_RUNS_S));
	}
	return 0;
}
