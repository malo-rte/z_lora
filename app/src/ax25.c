#include <stdint.h>
#include <stdbool.h>

#define AX25_MAX_DIGIS 8

typedef struct {
	char callsign[7];
	uint8_t ssid; // 0-15
	uint8_t raw_ssid; //
	bool last;
} ax25_addr_t;

typedef struct {
	ax25_addr_t dest;
	ax25_addr_t src;
	ax25_addr_t digis[AX25_MAX_DIGIS];
	uint8_t num_digis;

	uint8_t control;
	bool is_u_frame;
	bool is_s_frame;
	bool is_i_frame;

	uint8_t pid;
	const uint8_t *info;
	uint16_t info_len;

} ax25_frame_t;

/* -------- CRC-CCITT (X.25) LSB-first: init 0xFFFF, poly 0x8408, final ones' complement -------- */
static uint16_t ax25_crc_update(uint16_t crc, uint8_t byte)
{
	crc ^= byte;
	for (int i = 0; i < 8; i++) {
		if (crc & 0x0001) {
			crc = (crc >> 1) ^ 0x8408;
		} else {
			crc >>= 1;
		}
	}
	return crc;
}

static bool ax25_check_fcs(const uint8_t *buf, size_t len_no_flags)
{
	if (len_no_flags < 3) {
		return false;
	} // at least 1 byte + 2-byte FCS
	size_t payload_len = len_no_flags - 2;
	uint16_t crc = 0xFFFF;
	for (size_t i = 0; i < payload_len; i++) {
		crc = ax25_crc_update(crc, buf[i]);
	}
	crc = ~crc; // ones' complement

	uint16_t fcs_rx = (uint16_t)buf[payload_len] | ((uint16_t)buf[payload_len + 1] << 8); // LSB first on air
	return crc == fcs_rx;
}

/* -------- Address decoding --------
   Each address is 7 bytes: 6 callsign chars and 1 SSID/flags octet.
   All 7 bytes are left-shifted by 1 bit on-air; bit0 of the 7th octet is the Extension (E) bit.
*/
static void parse_callsign(const uint8_t *addr7, ax25_addr_t *out)
{
	// callsign chars are upper-case ASCII left-shifted by 1; pad is space.
	int end = 6;
	for (int i = 0; i < 6; i++) {
		char c = (char)(addr7[i] >> 1);
		out->callsign[i] = c;
		if (c != ' ') {
			end = i + 1;
		}
	}
	out->callsign[end] = '\0';

	uint8_t ssid_oct = addr7[6];
	out->raw_ssid = ssid_oct;
	out->last = (ssid_oct & 0x01) ? true : false; // E-bit
	out->ssid = (uint8_t)((ssid_oct >> 1) & 0x0F); // 4-bit SSID, 0..15
}

/* Returns bytes consumed from buf, or 0 on error. Fills dest, src, digis, num_digis. */
static size_t ax25_parse_addresses(const uint8_t *buf, size_t len, ax25_frame_t *f)
{
	if (len < 14) {
		return 0;
	}

	parse_callsign(buf + 0, &f->dest);
	parse_callsign(buf + 7, &f->src);

	size_t off = 14;
	f->num_digis = 0;

	// If 'src.last' is not set, there are digipeaters (up to 8), each 7 bytes, last has E-bit set.
	if (!f->src.last) {
		while (off + 7 <= len && f->num_digis < AX25_MAX_DIGIS) {
			parse_callsign(buf + off, &f->digis[f->num_digis]);
			f->num_digis++;
			off += 7;
			if (f->digis[f->num_digis - 1].last) {
				break;
			}
		}
		if (f->num_digis == 0) {
			return 0;
		} // malformed: no terminating E-bit found
		if (!f->digis[f->num_digis - 1].last) {
			return 0;
		} // malformed: not terminated
	}

	return off;
}

/* -------- Frame type helpers -------- */
static void classify_control(uint8_t ctl, bool *is_u, bool *is_s, bool *is_i)
{
	// I-frame: bit0 == 0
	// S-frame: 0b01 in bits1..0
	// U-frame: 0b11 in bits1..0
	*is_i = ((ctl & 0x01) == 0);
	*is_s = ((ctl & 0x03) == 0x01);
	*is_u = ((ctl & 0x03) == 0x03);
}

/* -------- Top-level decode -------- */
bool ax25_decode(const uint8_t *frame_bytes_no_flags, size_t len, ax25_frame_t *out)
{
	if (len < 16) {
		return false;
	} // minimal: 14 addr + 1 ctl + 1 FCS? But FCS is 2 bytes.

	if (!ax25_check_fcs(frame_bytes_no_flags, len)) {
		return false;
	}

	size_t addr_len = ax25_parse_addresses(frame_bytes_no_flags, len - 2, out);
	if (addr_len == 0) {
		return false;
	}

	if (addr_len + 1 > len - 2) {
		return false;
	}
	out->control = frame_bytes_no_flags[addr_len + 0];

	classify_control(out->control, &out->is_u_frame, &out->is_s_frame, &out->is_i_frame);

	size_t off = addr_len + 1;
	out->pid = 0;
	out->info = NULL;
	out->info_len = 0;

	// PID present for I-frames and most U-frames, also UI (0x03) specifically.
	bool pid_present = out->is_i_frame || (out->is_u_frame && out->control == 0x03);
	if (pid_present) {
		if (off + 1 > len - 2) {
			return false;
		}
		out->pid = frame_bytes_no_flags[off++];
		if (off <= len - 2) {
			out->info = frame_bytes_no_flags + off;
			out->info_len = (uint16_t)((len - 2) - off);
		}
	} else {
		// Supervisory or other U-frames without PID
		if (off <= len - 2) {
			out->info = frame_bytes_no_flags + off;
			out->info_len = (uint16_t)((len - 2) - off);
		}
	}

	return true;
}
