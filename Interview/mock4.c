#include<stdio.h>
#include<stdlib.h>
#include<stdbool.h>
#include<stdint.h>
#include<assert.h>

enum Type {
CHAR = 1,
INT,
FLOAT,
};

uint16_t last_type;
uint16_t last_len;
uint16_t calls;
const uint8_t *last_payload;

void handle_msg(uint16_t type, const uint8_t *payload, uint16_t len) {
    last_payload = payload;
    calls++;
    last_type = type, last_len = len;
    return;
}

// returns the number of parsed messages, -1 if there is a problem
int parse_msgs(const uint8_t *buf, size_t size){
    uint16_t counter = 0;
    uint16_t len = 0;
    for (size_t i = 0; i < size; i = i + len + 4){
        if (i + 4 > size) {
            fprintf(stderr, "guest wrote beyonf buffer\n");
            return -1;
        }
        uint16_t type = ((uint16_t)buf[i + 1] << 8) + (uint16_t)buf[i];
        if (type != 1 && type != 2 && type != 3){
            fprintf(stderr, "unrecognized type\n");
            return -1;
        }
        len = ((uint16_t)buf[i + 3] << 8) + (uint16_t)buf[i + 2];
        if (i + len + 4 > size){
            fprintf(stderr, "guest wrote beyonf buffer\n");
            return -1;
        }
        handle_msg(type, &buf[i + 4], len);
        counter++;
    }
    return counter;
}





int main(void) {
    {
        const uint8_t buf[] = {0x01, 0x00, 0x03, 0x00, 'a', 'b', 'c'};
        calls = 0;
        int res = parse_msgs(buf, sizeof(buf));
        assert(res == 1);
        assert(calls == 1);
        assert(last_len == 3);
        assert(last_type == 1);
        assert(last_payload == &buf[4]);   // pointer into buf, at the right offset
    }
    {   // two messages back to back
        const uint8_t buf[] = {0x01, 0x00, 0x03, 0x00, 'a', 'b', 'c',
                               0x02, 0x00, 0x00, 0x00};
        calls = 0;                      // reset the stub's globals
        int res = parse_msgs(buf, sizeof buf);
        assert(res == 2);
        assert(calls == 2);
        assert(last_type == 2 && last_len == 0);
        assert(last_payload == &buf[11]);   // pointer into buf, at the right offset
    }
    {   // len past the end
        const uint8_t buf[] = {0x01, 0x00, 0x05, 0x00, 0xAA};
        calls = 0;                      // reset the stub's globals
        int res = parse_msgs(buf, sizeof buf);
        assert(res == -1);
        assert(calls == 0);
    }
    {   // header cut off
        const uint8_t buf[] = {0x01, 0x00, 0x03, 0x00, 'a', 'b', 'c',
                               0x02, 0x00, 0x00, 0x00, 0x01, 0x00};
        calls = 0;                      // reset the stub's globals
        int res = parse_msgs(buf, sizeof buf);
        assert(res == -1);
        assert(calls == 2);
    }
    {   // unknown type
        const uint8_t buf[] = {0x07, 0x00, 0x00, 0x00};
        calls = 0;                      // reset the stub's globals
        int res = parse_msgs(buf, sizeof buf);
        assert(res == -1);
        assert(calls == 0);
    }
    {   // empty buffer
        const uint8_t buf[] = {0x07, 0x00, 0x00, 0x00};
        calls = 0;                      // reset the stub's globals
        int res = parse_msgs(buf, 0); // zero lengt array cannot be declared // zero lengt array cannot be declared
        assert(res == 0);
        assert(calls == 0);
    }
}
