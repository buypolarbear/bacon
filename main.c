#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>

#include "bluez/lib/bluetooth.h"
#include "bluez/lib/hci.h"
#include "bluez/lib/hci_lib.h"

#include "./inih/ini.h"

static void change_adv_data(int dev, le_set_advertising_data_cp *data_cp, int to) {
    hci_le_set_advertise_enable(dev, 0, 1000);

    struct hci_request rq;
    uint8_t status;

    memset(&rq, 0, sizeof(rq));
    rq.ogf = OGF_LE_CTL;
    rq.ocf = OCF_LE_SET_ADVERTISING_DATA;
    rq.cparam = data_cp;
    rq.clen = LE_SET_ADVERTISING_DATA_CP_SIZE;
    rq.rparam = &status;
    rq.rlen = 1;

    if (hci_send_req(dev, &rq, to) < 0 || status != 0) {
        perror("hci_send_req failed");
    }

    hci_le_set_advertise_enable(dev, 1, 1000);
}

#define EDDYSTONE_UID 0x00
#define EDDYSTONE_URL 0x10
#define EDDYSTONE_TLM 0x30
const char *schemas[] = {
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    0
};

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
static uint8_t encodeurl(const char *url, char *out) {
    int p = 0;
    int o = 0;
    for (int i = 0; schemas[i]; i++) {
        if (strncmp(schemas[i], url + p, MIN(strlen(url), strlen(schemas[i]))) == 0) {
            p += strlen(schemas[i]);
            out[o] = i;
            o += 1;
            break;
        }
    }
    if (p == 0) {
        fprintf(stderr, "invalid schema\n");
        exit(3);
    }

    memcpy(out + o, url + p, strlen(url) - p);
    o += (strlen(url) - p);
    if (o > 17 || o < 0) {
        fprintf(stderr, "url too long\n");
        exit(3);
    }
    return o;
}

static void set_eddystone_url_adv(int dev, const char *url) {
    char payload[24];
    uint8_t plen = encodeurl(url, payload);

    uint8_t header[] = {
        0x02,   // Flags length
        0x01,   // Flags data type value
        0x1a,   // Flags data

        0x03,   // Service UUID length
        0x03,   // Service UUID data type value
        0xaa,   // 16-bit Eddystone UUID
        0xfe,   // 16-bit Eddystone UUID

        (5 + plen),  // Service Data length
        0x16,   // Service Data data type value
        0xaa,   // 16-bit Eddystone UUID
        0xfe,   // 16-bit Eddystone UUID

        EDDYSTONE_URL,   // Eddystone-url frame type
        0xed,   // txpower
    };

    le_set_advertising_data_cp d;
    memcpy(d.data, header, sizeof(header));
    memcpy(d.data + sizeof(header), payload, plen);
    d.length = sizeof(header) + plen;

    change_adv_data(dev, &d, 1000);
}

const char* eddystone_url = 0;
static int inihandler(void* user, const char* section, const char* name, const char* value)
{
    if (strcmp(section, "eddystone") == 0) {
        if (strcmp(name, "url") == 0) {
            eddystone_url = strdup(value);
        }
    }
    return 1;
}

int main(int argc, char **argv) {

    if (argc <  2) {
        fprintf(stderr, "use: %s config.ini\n", argv[0]);
        exit(1);
    }
    ini_parse(argv[1], inihandler, 0);

    int s = socket(PF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    int r = ioctl(s, HCIDEVUP, 0);
    close(s);
    if (r < 0) {
        if (errno != EALREADY) {
            perror("HCIDEVUP failed");
        }
    }

    int dev = hci_open_dev(hci_get_route(NULL));
    if(dev < 0) {
        perror("can't open hci dev");
        return 3;
    }
    if (eddystone_url) {
        set_eddystone_url_adv(dev, eddystone_url);
        fprintf(stderr, "eddystone transmitting...\n");
    } else {
        hci_le_set_advertise_enable(dev, 0, 1000);
        fprintf(stderr, "disabled...\n");
    }
}
