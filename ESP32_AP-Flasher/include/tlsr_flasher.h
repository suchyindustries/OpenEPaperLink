#pragma once
#include <Arduino.h>

/* Flashing a TLSR825x over Single Wire Slave (SWS).
 *
 * Only compiled when the board defines an SWS pin (FLASHER_AP_SWS). Without it
 * nothing here is built at all - deliberately, so that the resulting image is
 * unchanged for every other board.
 */
#ifdef FLASHER_AP_SWS

/* Write a file from the filesystem into the TLSR flash and verify it.
 * Progress goes to wsSerial(). Returns true once written and read back. */
bool tlsrFlashFile(const String &path);

/* Fetch firmware_TLSR.json from a release directory, pick the image for the
 * requested AP type and flash that. Mirrors what FlashC6_H2() does for an ESP
 * co-processor, so the web interface can treat both the same way. */
bool tlsrFlashFromUrl(const String &dirUrl, const String &apType);

/* Same, but in a task of its own - the web server must not wait for it.
 * An empty url flashes the local file instead. */
void tlsrFlashStart(const String &path, const String &url, const String &apType);

#endif
