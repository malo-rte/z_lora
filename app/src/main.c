#include <zephyr/kernel.h>
//#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/lora.h>
#include <zephyr/console/console.h>

#include <zephyr/drivers/uart.h>
#define UART_CONSOLE_NODE DT_CHOSEN(zephyr_console)
static const struct device *const uart_dev = DEVICE_DT_GET(UART_CONSOLE_NODE);

static void uart_write_sync(const char *s, size_t n)
{
	for (size_t i = 0; i < n; i++) {
		uart_poll_out(uart_dev, s[i]);
		k_msleep(1);
	}
}

//LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LORA_NODE DT_NODELABEL(lora0)

static struct lora_modem_config cfg = {
	.frequency = 433175000, /* eu433 ch0; try also 433375000, 433575000 */
	// .frequency = 433375000, /* eu433 ch1; try also 433375000, 433575000 */
	// .frequency = 433575000, /* eu433 ch2; try also 433375000, 433575000 */

	.bandwidth = BW_250_KHZ,
	.datarate = SF_11, /* try SF7..SF12 to match transmitters */
	.coding_rate = CR_4_6,
	//.datarate = SF_7,
	//.coding_rate = CR_4_5,
	.preamble_len = 8,
	.tx_power = 10, /* unused in RX */
	.tx = false,
	.iq_inverted = false, /* set true to watch downlinks */
	.public_network = false,
};

#define DUMP_MAX_STR 2048

static void lora_dump(const uint8_t *buf, size_t len, int rssi, int snr)
{
	len = MIN(len, 256);

	static char out[DUMP_MAX_STR];
	size_t pos = 0;

	pos += snprintk(out + pos, sizeof(out) - pos, "\nRSSI: %d dBm    SNR: %d dB    SIZE: %u\n", rssi, snr, (unsigned)len);

	for (size_t off = 0; off < len && pos < sizeof(out) - 2; off += 16) {
		size_t n = MIN(16, len - off);

		pos += snprintk(out + pos, sizeof(out) - pos, "%08x  ", (unsigned)off);

		for (size_t i = 0; i < 16; i++) {
			if (i < n)
				pos += snprintk(out + pos, sizeof(out) - pos, "%02x", buf[off + i]);
			else
				pos += snprintk(out + pos, sizeof(out) - pos, "  ");
			pos += snprintk(out + pos, sizeof(out) - pos, (i == 7) ? "  " : " ");
		}

		out[pos++] = ' ';
		out[pos++] = '|';
		for (size_t i = 0; i < n && pos < sizeof(out) - 2; i++) {
			uint8_t c = buf[off + i];
			out[pos++] = (c >= 32 && c < 127) ? (char)c : '.';
		}
		out[pos++] = '|';
		out[pos++] = '\n';
	}

	uart_write_sync(out, pos);
}

static int clamp_int(int value, int min_value, int max_value)
{
	if (value < min_value) {
		return min_value;
	}
	if (value > max_value) {
		return max_value;
	}
	return value;
}

int main(void)
{
	k_msleep(100);
	// psram_backend_enable();

	const struct device *dev = DEVICE_DT_GET(LORA_NODE);

	if (!device_is_ready(dev)) {
		printk("Lora device not ready!\n");
		k_msleep(5000);

		return 0;
	}

	if (lora_config(dev, &cfg) != 0) {
		printk("lora_config failed\n");
		k_msleep(5000);

		return 0;
	}

	static uint8_t buffer[1024];

	while (1) {
		int16_t rssi = 0;
		int8_t snr = 0;

		int rc = lora_recv(dev, buffer, 255, K_FOREVER, &rssi, &snr);

		if (rc > 0) {
			lora_dump(buffer, (size_t)clamp_int(rc, 0, sizeof(buffer)), (int)rssi, (int)snr);
		} else if (rc < 0) {
			printk("RX: error: %d\n", rc);
		}
	}
}
